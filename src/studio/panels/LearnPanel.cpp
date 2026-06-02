#include "LearnPanel.h"
#include "GamepadPanel.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../../core/AnimationPort.h"
#include "../../midi/MidiMessage.h"
#include "../../midi/MidiDeviceManager.h"
#include "../../input/InputManager.h"
#include "../../input/GamepadManager.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>
#include <nlohmann/json.hpp>

namespace bbfx {

// ── Source-token translation helpers ─────────────────────────────────────
namespace {
    int gamepadButtonFromToken(const std::string& token) {
        if (token == "buttonA") return 0;
        if (token == "buttonB") return 1;
        if (token == "buttonX") return 2;
        if (token == "buttonY") return 3;
        if (token == "leftBumper") return 4;
        if (token == "rightBumper") return 5;
        if (token == "back") return 6;
        if (token == "start") return 7;
        if (token == "leftStickBtn") return 8;
        if (token == "rightStickBtn") return 9;
        if (token == "dpadUp") return 10;
        if (token == "dpadDown") return 11;
        if (token == "dpadLeft") return 12;
        if (token == "dpadRight") return 13;
        return -1;
    }
    int gamepadAxisFromToken(const std::string& token) {
        if (token == "leftStickX") return 0;
        if (token == "leftStickY") return 1;
        if (token == "rightStickX") return 2;
        if (token == "rightStickY") return 3;
        if (token == "leftTrigger") return 4;
        if (token == "rightTrigger") return 5;
        return -1;
    }
}

const char* LearnPanel::sourceTypeLabel(LearnBindingManager::SourceType t) {
    switch (t) {
    case LearnBindingManager::SourceType::MidiCC:        return "MIDI CC";
    case LearnBindingManager::SourceType::MidiNote:      return "MIDI Note";
    case LearnBindingManager::SourceType::GamepadButton: return "Gamepad Btn";
    case LearnBindingManager::SourceType::GamepadAxis:   return "Gamepad Axis";
    case LearnBindingManager::SourceType::KeyboardKey:   return "Key";
    }
    return "?";
}

std::string LearnPanel::bindingLabel(const LearnBindingManager::Binding& b) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s #%d", sourceTypeLabel(b.sourceType), b.sourceId);
    return std::string(buf);
}

// ── Port collection ──────────────────────────────────────────────────────
std::vector<LearnPanel::PortRef> LearnPanel::collectPorts() const {
    std::vector<PortRef> out;
    auto* animator = Animator::instance();
    if (!animator) return out;
    for (const auto& nodeName : animator->getRegisteredNodeNames()) {
        auto* n = animator->getRegisteredNode(nodeName);
        if (!n) continue;
        for (auto& [portName, port] : n->getInputs()) {
            PortRef ref;
            ref.nodeName = nodeName;
            ref.portName = portName;
            ref.nodePath = nodeName + "." + portName;
            ref.portType = "Float";  // ports are float-typed in BBFx DAG
            out.push_back(ref);
        }
    }
    return out;
}

bool LearnPanel::passesFilters(const PortRef& p) const {
    if (!mFilterNodeName.empty() && p.nodeName != mFilterNodeName) return false;
    if (mSearch[0]) {
        std::string s = mSearch;
        std::string path = p.nodePath;
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        std::transform(path.begin(), path.end(), path.begin(), ::tolower);
        if (path.find(s) == std::string::npos) return false;
    }
    // Single port type in BBFx (Float). Filters reserved for future extension.
    return mFilterFloat;
}

// ── Capture flow ─────────────────────────────────────────────────────────
void LearnPanel::startLearn(const std::string& portPath) {
    mLearningPortPath = portPath;
    if (mGamepadPanel) {
        mGamepadPanel->setLearnCallback([this](const std::string& token) {
            if (mLearningPortPath.empty()) return;
            int btnIdx = gamepadButtonFromToken(token);
            if (btnIdx >= 0) {
                completeLearn(LearnBindingManager::SourceType::GamepadButton, btnIdx);
                return;
            }
            int axIdx = gamepadAxisFromToken(token);
            if (axIdx >= 0) {
                completeLearn(LearnBindingManager::SourceType::GamepadAxis, axIdx);
            }
        });
    }
}

void LearnPanel::cancelLearn() {
    mLearningPortPath.clear();
    mAutoMapAll = false;
    mAutoMapList.clear();
    mAutoMapIndex = 0;
    if (mGamepadPanel) mGamepadPanel->setLearnCallback(nullptr);
}

void LearnPanel::completeLearn(LearnBindingManager::SourceType type, int sourceId) {
    if (mLearningPortPath.empty()) return;
    auto& mgr = LearnBindingManager::instance();
    LearnBindingManager::Binding b;
    b.portPath   = mLearningPortPath;
    b.sourceType = type;
    b.sourceId   = sourceId;
    b.scale      = 1.0f;
    b.offset     = 0.0f;
    b.invert     = false;
    mgr.addBinding(b);
    mLearningPortPath.clear();
    if (mGamepadPanel) mGamepadPanel->setLearnCallback(nullptr);
    if (mAutoMapAll) advanceAutoMap();
}

void LearnPanel::advanceAutoMap() {
    mAutoMapIndex++;
    if (mAutoMapIndex >= mAutoMapList.size()) {
        mAutoMapAll = false;
        mAutoMapIndex = 0;
        mAutoMapList.clear();
        return;
    }
    startLearn(mAutoMapList[mAutoMapIndex]);
}

// ── Event integration ────────────────────────────────────────────────────
bool LearnPanel::handleKeyEvent(const SDL_Event& evt) {
    if (mLearningPortPath.empty()) return false;
    if (evt.type != SDL_EVENT_KEY_DOWN) return false;
    completeLearn(LearnBindingManager::SourceType::KeyboardKey,
                  static_cast<int>(evt.key.key));
    return true;
}

void LearnPanel::processMidiMessages(const std::vector<MidiMessage>& messages) {
    if (mLearningPortPath.empty()) return;
    for (const auto& msg : messages) {
        const uint8_t st = msg.type();
        if (st == MidiMessage::ControlChange) {
            completeLearn(LearnBindingManager::SourceType::MidiCC, msg.data1);
            return;
        }
        if (st == MidiMessage::NoteOn && msg.data2 > 0) {
            completeLearn(LearnBindingManager::SourceType::MidiNote, msg.data1);
            return;
        }
    }
}

// ── Per-frame apply (drives DAG ports from bindings) ─────────────────────
void LearnPanel::update() {
    // D16 — polling live des bindings unifiés : pour chaque binding, on lit la
    // valeur courante de la source (MIDI CC/note via le cache MidiDeviceManager,
    // gamepad via GamepadManager, clavier via l'état SDL), on applique
    // scale/offset/invert, et on pousse sur le port DAG cible. Avant, cet update()
    // était un no-op documenté → les bindings MIDI/clavier créés dans LearnPanel
    // ne pilotaient jamais leur port (seul le gamepad marchait via callback).
    auto& mgr = LearnBindingManager::instance();
    auto* animator = Animator::instance();
    if (!animator || mgr.bindingCount() == 0) return;

    auto* midi = MidiDeviceManager::instance();
    auto* im = InputManager::instance();
    GamepadManager* gp = im ? &im->getJoystick() : nullptr;
    const bool* keyState = SDL_GetKeyboardState(nullptr);

    for (size_t i = 0; i < mgr.bindingCount(); ++i) {
        const auto& b = mgr.getBinding(i);
        if (b.portPath.empty()) continue;

        float raw = 0.0f;
        switch (b.sourceType) {
            case LearnBindingManager::SourceType::MidiCC:
                if (midi) for (int ch = 1; ch <= 16; ++ch) {
                    float v = midi->getLastCCValue(ch, b.sourceId);
                    if (v != 0.0f) { raw = v; break; }
                }
                break;
            case LearnBindingManager::SourceType::MidiNote:
                if (midi) for (int ch = 1; ch <= 16; ++ch) {
                    float v = midi->getNoteVelocity(ch, b.sourceId);
                    if (v != 0.0f) { raw = v; break; }
                }
                break;
            case LearnBindingManager::SourceType::GamepadButton:
                if (gp) raw = gp->isButtonDown(0, b.sourceId) ? 1.0f : 0.0f;
                break;
            case LearnBindingManager::SourceType::GamepadAxis:
                if (gp) raw = gp->getAxisValue(0, b.sourceId);
                break;
            case LearnBindingManager::SourceType::KeyboardKey:
                if (keyState) {
                    SDL_Scancode sc = SDL_GetScancodeFromKey(
                        static_cast<SDL_Keycode>(b.sourceId), nullptr);
                    if (sc != SDL_SCANCODE_UNKNOWN) raw = keyState[sc] ? 1.0f : 0.0f;
                }
                break;
        }

        float val = LearnBindingManager::applyTransform(b, raw);

        // Push sur le port "nodeName.portName".
        auto dot = b.portPath.find('.');
        if (dot == std::string::npos) continue;
        std::string nodeName = b.portPath.substr(0, dot);
        std::string portName = b.portPath.substr(dot + 1);
        auto* node = animator->getRegisteredNode(nodeName);
        if (!node) continue;
        auto& inputs = node->getInputs();
        auto it = inputs.find(portName);
        if (it != inputs.end()) it->second->setValue(val);
    }
}

// ── ImGui render ─────────────────────────────────────────────────────────
void LearnPanel::render() {
    if (!mVisible) return;
    if (!ImGui::Begin("Learn Bindings", &mVisible,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    // ── Toolbar ──────────────────────────────────────────────────────────
    if (ImGui::Button("Auto-map all")) {
        // Snapshot the visible port list and start sequentially.
        mAutoMapList.clear();
        for (auto& p : collectPorts()) {
            if (passesFilters(p)) mAutoMapList.push_back(p.nodePath);
        }
        if (!mAutoMapList.empty()) {
            mAutoMapAll = true;
            mAutoMapIndex = 0;
            startLearn(mAutoMapList[0]);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All")) mPendingClearAll = true;
    ImGui::SameLine();
    if (!mLearningPortPath.empty()) {
        if (ImGui::Button("Cancel learn")) cancelLearn();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f),
                           "Learning '%s' — press a control or key...",
                           mLearningPortPath.c_str());
    } else if (mAutoMapAll) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f),
                           "Auto-map %zu / %zu", mAutoMapIndex + 1, mAutoMapList.size());
    }

    ImGui::Separator();

    // ── Filters ──────────────────────────────────────────────────────────
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##search", "Search ports...", mSearch, sizeof(mSearch));
    ImGui::SameLine();
    ImGui::Checkbox("Float", &mFilterFloat);
    ImGui::SameLine(); ImGui::Checkbox("Int", &mFilterInt);
    ImGui::SameLine(); ImGui::Checkbox("Bool", &mFilterBool);
    ImGui::SameLine(); ImGui::Checkbox("Trigger", &mFilterTrigger);

    ImGui::Separator();

    // ── Confirm modal: Clear All ─────────────────────────────────────────
    if (mPendingClearAll) {
        ImGui::OpenPopup("Clear all bindings?");
        mPendingClearAll = false;
    }
    if (ImGui::BeginPopupModal("Clear all bindings?", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("This will remove all %zu Learn bindings. Cannot be undone.",
                    LearnBindingManager::instance().bindingCount());
        ImGui::Separator();
        if (ImGui::Button("Clear All", ImVec2(120, 0))) {
            LearnBindingManager::instance().clearAll();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Main table ───────────────────────────────────────────────────────
    if (ImGui::BeginTable("##LearnTable", 7,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
          | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Node",       ImGuiTableColumnFlags_WidthStretch, 0.20f);
        ImGui::TableSetupColumn("Port",       ImGuiTableColumnFlags_WidthStretch, 0.18f);
        ImGui::TableSetupColumn("Mapped to",  ImGuiTableColumnFlags_WidthStretch, 0.18f);
        ImGui::TableSetupColumn("Scale",      ImGuiTableColumnFlags_WidthFixed,   80);
        ImGui::TableSetupColumn("Offset",     ImGuiTableColumnFlags_WidthFixed,   80);
        ImGui::TableSetupColumn("Invert",     ImGuiTableColumnFlags_WidthFixed,   60);
        ImGui::TableSetupColumn("Actions",    ImGuiTableColumnFlags_WidthFixed,   140);
        ImGui::TableHeadersRow();

        auto& mgr = LearnBindingManager::instance();
        auto ports = collectPorts();
        for (const auto& p : ports) {
            if (!passesFilters(p)) continue;
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(p.nodeName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(p.portName.c_str());

            // Find the binding (first match on portPath)
            int bindingIdx = -1;
            for (size_t i = 0; i < mgr.bindingCount(); ++i) {
                if (mgr.getBinding(i).portPath == p.nodePath) {
                    bindingIdx = static_cast<int>(i); break;
                }
            }

            ImGui::TableNextColumn();
            if (mLearningPortPath == p.nodePath) {
                // Pulse the row to show capture is active.
                float t = ImGui::GetTime();
                float pulse = 0.5f + 0.5f * std::sin(t * 6.0f);
                ImGui::TextColored(ImVec4(1.0f, 0.7f * pulse, 0.0f, 1.0f),
                                   "Listening...");
            } else if (bindingIdx >= 0) {
                ImGui::TextUnformatted(bindingLabel(mgr.getBinding(bindingIdx)).c_str());
            } else {
                ImGui::TextDisabled("—");
            }

            // Scale / Offset / Invert (only if a binding exists)
            ImGui::TableNextColumn();
            if (bindingIdx >= 0) {
                ImGui::PushID(bindingIdx);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("##scale",
                                 &mgr.getBindingMut(bindingIdx).scale,
                                 0.01f, 0.01f, 10.0f, "%.2f");
                ImGui::PopID();
            }
            ImGui::TableNextColumn();
            if (bindingIdx >= 0) {
                ImGui::PushID(bindingIdx);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("##offset",
                                 &mgr.getBindingMut(bindingIdx).offset,
                                 0.01f, -1.0f, 1.0f, "%.2f");
                ImGui::PopID();
            }
            ImGui::TableNextColumn();
            if (bindingIdx >= 0) {
                ImGui::PushID(bindingIdx);
                ImGui::Checkbox("##invert", &mgr.getBindingMut(bindingIdx).invert);
                ImGui::PopID();
            }

            // Actions
            ImGui::TableNextColumn();
            if (bindingIdx >= 0) {
                ImGui::PushID(bindingIdx);
                if (ImGui::SmallButton("Clear")) mgr.removeBindingAt(bindingIdx);
                ImGui::PopID();
            } else if (mLearningPortPath != p.nodePath) {
                ImGui::PushID((int)std::hash<std::string>{}(p.nodePath));
                if (ImGui::SmallButton("Learn")) startLearn(p.nodePath);
                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

// ── Persistence ──────────────────────────────────────────────────────────
nlohmann::json LearnPanel::toJson() const {
    return LearnBindingManager::instance().toJson();
}

void LearnPanel::fromJson(const nlohmann::json& j) {
    LearnBindingManager::instance().fromJson(j);
}

} // namespace bbfx
