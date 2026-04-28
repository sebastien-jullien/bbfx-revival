#include "EffectRackPanel.h"
#include "GamepadPanel.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../../input/InputManager.h"
#include "../../input/GamepadManager.h"
#include "../../midi/MidiLearnManager.h"
#include "../../midi/MidiDeviceManager.h"
#include "../commands/EditCommands.h"
#include "../commands/CommandManager.h"

#include <imgui.h>
#include <nlohmann/json.hpp>
#include <iostream>

namespace bbfx {

// ── Render ───────────────────────────────────────────────────────────────────

void EffectRackPanel::render() {
    ImGui::Begin("Effect Rack");

    auto* animator = Animator::instance();
    if (!animator) {
        ImGui::TextDisabled("No animator");
        ImGui::End();
        return;
    }

    const auto& names = animator->getRegisteredNodeNames();
    if (names.empty()) {
        ImGui::TextDisabled("No nodes in graph");
        ImGui::End();
        return;
    }

    // Keyboard learn status banner
    if (!mKeyLearnNodeName.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 1.f, 1.f));
        ImGui::Text("KEY LEARN: press a key for '%s'...", mKeyLearnNodeName.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel##keylearn")) mKeyLearnNodeName.clear();
        ImGui::Separator();
    }

    // Gamepad learn status banner
    if (!mGamepadLearnNodeName.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.8f, 0.2f, 1.f));
        ImGui::Text("GAMEPAD LEARN: move a control for '%s'...", mGamepadLearnNodeName.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel##glearn")) mGamepadLearnNodeName.clear();
        ImGui::Separator();
    }

    for (const auto& name : names) {
        auto* node = animator->getRegisteredNode(name);
        if (!node) continue;

        ImGui::PushID(name.c_str());
        bool enabled = node->isEnabled();

        // ── LED indicator ────────────────────────────────────────────────
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImU32 ledColor = enabled
            ? IM_COL32(0, 220, 0, 255)
            : IM_COL32(220, 0, 0, 255);
        ImGui::GetWindowDrawList()->AddCircleFilled(
            ImVec2(pos.x + 6, pos.y + 8), 5.f, ledColor);
        ImGui::Dummy(ImVec2(16, 0));
        ImGui::SameLine();

        // ── Enable/Disable checkbox (undo-able) ─────────────────────────
        if (ImGui::Checkbox("##toggle", &enabled)) {
            CommandManager::instance().execute(
                std::make_unique<SetEnabledCommand>(name, !enabled, enabled));
        }
        ImGui::SameLine();

        // ── Node name + type (dimmed when disabled) ─────────────────────
        if (!enabled) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.f));
        ImGui::Text("%s", name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", node->getTypeName().c_str());
        if (!enabled) ImGui::PopStyleColor();

        // ── MIDI Learn button ────────────────────────────────────────────
        ImGui::SameLine();
        auto& mlm = MidiLearnManager::instance();

        // Find existing rack_toggle binding for this node
        const MidiBinding* midiBinding = nullptr;
        for (const auto& b : mlm.getBindings()) {
            if (b.target.type == "rack_toggle" && b.target.nodeName == name) {
                midiBinding = &b;
                break;
            }
        }

        bool isMidiLearning = mlm.isLearning()
            && mlm.getLearnTarget().type == "rack_toggle"
            && mlm.getLearnTarget().nodeName == name;

        if (isMidiLearning) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.f, 0.f, 1.f, 1.f));
            if (ImGui::SmallButton("...##midi")) mlm.cancelLearn();
            ImGui::PopStyleColor();
        } else if (midiBinding) {
            char badge[32];
            snprintf(badge, sizeof(badge), "%s%d##midi",
                midiBinding->midiType == "cc" ? "CC" : "N", midiBinding->number);
            if (ImGui::SmallButton(badge)) {
                MidiLearnTarget t;
                t.type = "rack_toggle";
                t.nodeName = name;
                mlm.startLearn(t);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("MIDI: %s %d (click to re-learn)",
                midiBinding->midiType == "cc" ? "CC" : "Note", midiBinding->number);
        } else {
            if (ImGui::SmallButton("M##midi")) {
                MidiLearnTarget t;
                t.type = "rack_toggle";
                t.nodeName = name;
                mlm.startLearn(t);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("MIDI Learn");
        }

        // ── Gamepad Learn button ─────────────────────────────────────────
        ImGui::SameLine();
        auto gpIt = mGamepadBindings.find(name);
        bool hasGpBinding = (gpIt != mGamepadBindings.end());
        bool isGpLearning = (mGamepadLearnNodeName == name);

        if (isGpLearning) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.f, 0.8f, 0.2f, 1.f));
            if (ImGui::SmallButton("...##gp")) mGamepadLearnNodeName.clear();
            ImGui::PopStyleColor();
        } else if (hasGpBinding) {
            char badge[32];
            snprintf(badge, sizeof(badge), "%s##gp", gpIt->second.c_str());
            if (ImGui::SmallButton(badge)) {
                // Re-learn
                mGamepadLearnNodeName = name;
                if (mGamepadPanel) {
                    mGamepadPanel->setLearnCallback([this](const std::string& source) {
                        if (!mGamepadLearnNodeName.empty()) {
                            mGamepadBindings[mGamepadLearnNodeName] = source;
                            std::cout << "[EffectRack] Gamepad: " << mGamepadLearnNodeName
                                      << " → " << source << std::endl;
                            mGamepadLearnNodeName.clear();
                        }
                    });
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Gamepad: %s (click to re-learn)", gpIt->second.c_str());
        } else {
            if (ImGui::SmallButton("G##gp")) {
                mGamepadLearnNodeName = name;
                if (mGamepadPanel) {
                    mGamepadPanel->setLearnCallback([this](const std::string& source) {
                        if (!mGamepadLearnNodeName.empty()) {
                            mGamepadBindings[mGamepadLearnNodeName] = source;
                            std::cout << "[EffectRack] Gamepad: " << mGamepadLearnNodeName
                                      << " → " << source << std::endl;
                            mGamepadLearnNodeName.clear();
                        }
                    });
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Gamepad Learn");
        }

        // ── Keyboard Learn button ────────────────────────────────────────
        ImGui::SameLine();
        auto keyIt = mKeyBindings.find(name);
        bool hasKeyBinding = (keyIt != mKeyBindings.end());
        bool isKeyLearning = (mKeyLearnNodeName == name);

        if (isKeyLearning) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.f, 0.f, 1.f, 1.f));
            if (ImGui::SmallButton("...##key")) mKeyLearnNodeName.clear();
            ImGui::PopStyleColor();
        } else if (hasKeyBinding) {
            char badge[32];
            snprintf(badge, sizeof(badge), "%s##key", sdlKeyName(keyIt->second));
            if (ImGui::SmallButton(badge)) {
                mKeyLearnNodeName = name;
            }
            if (ImGui::IsItemHovered()) {
                if (isConflictingKey(keyIt->second)) {
                    ImGui::SetTooltip("Key: %s (WARNING: may conflict with global shortcut)\nClick to re-learn",
                        sdlKeyName(keyIt->second));
                } else {
                    ImGui::SetTooltip("Key: %s (click to re-learn)", sdlKeyName(keyIt->second));
                }
            }
        } else {
            if (ImGui::SmallButton("K##key")) {
                mKeyLearnNodeName = name;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Keyboard Learn");
        }

        ImGui::PopID();
    }

    ImGui::End();
}

// ── Binding update (called unconditionally, even when panel is hidden) ────────

void EffectRackPanel::updateBindings() {
    applyMidiBindings();
    applyGamepadBindings();
}

// ── MIDI binding application ─────────────────────────────────────────────────

void EffectRackPanel::applyMidiBindings() {
    auto* midiMgr = MidiDeviceManager::instance();
    if (!midiMgr) return;

    auto* animator = Animator::instance();
    if (!animator) return;

    auto& mlm = MidiLearnManager::instance();
    for (const auto& binding : mlm.getBindings()) {
        if (binding.target.type != "rack_toggle") continue;
        if (binding.target.nodeName.empty()) continue;

        auto* node = animator->getRegisteredNode(binding.target.nodeName);
        if (!node) continue;

        if (binding.midiType == "cc") {
            // Read from state cache (no queue drain needed)
            float val = midiMgr->getLastCCValue(binding.channel, binding.number);
            bool shouldEnable = (val > 0.5f);
            if (shouldEnable != node->isEnabled()) {
                node->setEnabled(shouldEnable);
            }
        } else if (binding.midiType == "note") {
            // Note down = enable, note up = disable
            bool noteDown = midiMgr->isNoteDown(binding.channel, binding.number);
            if (noteDown != node->isEnabled()) {
                node->setEnabled(noteDown);
            }
        }
    }
}

// ── Gamepad binding application ──────────────────────────────────────────────

void EffectRackPanel::applyGamepadBindings() {
    auto* im = InputManager::instance();
    if (!im) return;
    auto& gm = im->getJoystick();

    auto* animator = Animator::instance();
    if (!animator) return;

    for (const auto& [nodeName, source] : mGamepadBindings) {
        auto* node = animator->getRegisteredNode(nodeName);
        if (!node) continue;

        // Detect button tokens (start with "button" or "dpad")
        bool isButton = (source.rfind("button", 0) == 0 || source.rfind("dpad", 0) == 0);
        if (!isButton) continue;  // Only buttons toggle enable — axes don't make sense for on/off

        // Map token to button index
        static const std::map<std::string, int> kButtonMap = {
            {"buttonA", 0}, {"buttonB", 1}, {"buttonX", 2}, {"buttonY", 3},
            {"buttonBack", 4}, {"buttonGuide", 5}, {"buttonStart", 6},
            {"buttonL3", 7}, {"buttonR3", 8}, {"buttonL1", 9}, {"buttonR1", 10},
            {"dpadUp", 11}, {"dpadDown", 12}, {"dpadLeft", 13}, {"dpadRight", 14},
        };
        auto it = kButtonMap.find(source);
        if (it == kButtonMap.end()) continue;

        bool pressed = gm.isButtonDown(0, it->second);
        bool& prev = mGamepadPrevButtonState[nodeName];

        // Rising edge → toggle
        if (pressed && !prev) {
            bool newEnabled = !node->isEnabled();
            CommandManager::instance().execute(
                std::make_unique<SetEnabledCommand>(nodeName, !newEnabled, newEnabled));
        }
        prev = pressed;
    }
}

// ── Keyboard learn ───────────────────────────────────────────────────────────

bool EffectRackPanel::handleKeyEvent(const SDL_Event& evt) {
    if (mKeyLearnNodeName.empty()) return false;
    if (evt.type != SDL_EVENT_KEY_DOWN) return false;

    SDL_Keycode key = evt.key.key;

    // Don't capture modifier-only keys
    if (key == SDLK_LSHIFT || key == SDLK_RSHIFT ||
        key == SDLK_LCTRL || key == SDLK_RCTRL ||
        key == SDLK_LALT || key == SDLK_RALT) {
        return false;
    }

    // Remove any existing binding for this key (avoid duplicates)
    for (auto it = mKeyBindings.begin(); it != mKeyBindings.end(); ) {
        if (it->second == key && it->first != mKeyLearnNodeName) {
            std::cout << "[EffectRack] Key " << sdlKeyName(key)
                      << " reassigned from '" << it->first << "'" << std::endl;
            it = mKeyBindings.erase(it);
        } else {
            ++it;
        }
    }

    mKeyBindings[mKeyLearnNodeName] = key;

    if (isConflictingKey(key)) {
        std::cout << "[EffectRack] WARNING: " << sdlKeyName(key)
                  << " may conflict with a global shortcut" << std::endl;
    }

    std::cout << "[EffectRack] Key: " << mKeyLearnNodeName
              << " → " << sdlKeyName(key) << std::endl;
    mKeyLearnNodeName.clear();
    return true;  // consumed
}

void EffectRackPanel::processKeyboardBindings() {
    if (mKeyBindings.empty()) return;

    auto* im = InputManager::instance();
    if (!im) return;
    auto& kb = im->getKeyboard();

    // Don't process if ImGui is capturing keyboard (text input active)
    if (ImGui::GetIO().WantCaptureKeyboard) return;

    auto* animator = Animator::instance();
    if (!animator) return;

    for (const auto& [nodeName, key] : mKeyBindings) {
        auto* node = animator->getRegisteredNode(nodeName);
        if (!node) continue;

        bool pressed = kb.isKeyDown(key);
        bool& prev = mKeyPrevState[nodeName];

        // Rising edge → toggle
        if (pressed && !prev) {
            bool newEnabled = !node->isEnabled();
            CommandManager::instance().execute(
                std::make_unique<SetEnabledCommand>(nodeName, !newEnabled, newEnabled));
        }
        prev = pressed;
    }
}

// ── Helpers ──────────────────────────────────────────────────────────────────

const char* EffectRackPanel::sdlKeyName(SDL_Keycode key) {
    return SDL_GetKeyName(key);
}

bool EffectRackPanel::isConflictingKey(SDL_Keycode key) {
    // Known global shortcuts — warn but don't block
    static const SDL_Keycode kConflicting[] = {
        SDLK_ESCAPE, SDLK_SPACE, SDLK_RETURN, SDLK_DELETE,
        SDLK_F1, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5,
        SDLK_F6, SDLK_F7, SDLK_F8, SDLK_F9, SDLK_F11,
        SDLK_HOME,
    };
    for (auto k : kConflicting) {
        if (key == k) return true;
    }
    return false;
}

// ── Serialisation ────────────────────────────────────────────────────────────

nlohmann::json EffectRackPanel::toJson() const {
    nlohmann::json j;

    // Key bindings
    nlohmann::json keys = nlohmann::json::object();
    for (const auto& [nodeName, key] : mKeyBindings) {
        keys[nodeName] = static_cast<int>(key);
    }
    j["keyBindings"] = keys;

    // Gamepad bindings
    nlohmann::json gp = nlohmann::json::object();
    for (const auto& [nodeName, source] : mGamepadBindings) {
        gp[nodeName] = source;
    }
    j["gamepadBindings"] = gp;

    // MIDI bindings are handled by MidiLearnManager (no duplication)
    return j;
}

void EffectRackPanel::fromJson(const nlohmann::json& j) {
    mKeyBindings.clear();
    mGamepadBindings.clear();

    if (j.contains("keyBindings") && j["keyBindings"].is_object()) {
        for (auto& [nodeName, val] : j["keyBindings"].items()) {
            mKeyBindings[nodeName] = static_cast<SDL_Keycode>(val.get<int>());
        }
    }

    if (j.contains("gamepadBindings") && j["gamepadBindings"].is_object()) {
        for (auto& [nodeName, val] : j["gamepadBindings"].items()) {
            mGamepadBindings[nodeName] = val.get<std::string>();
        }
    }
}

} // namespace bbfx
