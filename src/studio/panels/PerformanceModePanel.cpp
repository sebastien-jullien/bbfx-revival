#include "PerformanceModePanel.h"
#include "../StudioEngine.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../../core/AnimationPort.h"
#include "../../core/PrimitiveNodes.h"
#include "../../audio/AudioAnalyzer.h"
#include "../../audio/BeatDetector.h"

#include <imgui.h>
#include <sol/sol.hpp>
#include <GL/gl.h>
#include <algorithm>
#include <iostream>

namespace bbfx {

PerformanceModePanel::PerformanceModePanel(sol::state& lua) : mLua(lua) {
    // Initialize trigger chord assignments from default names
    static const char* defaultChords[16] = {
        "Intro","Verse","Chorus","Bridge",
        "Drop","Build","Break","Outro",
        "FX1","FX2","FX3","FX4",
        "Trig1","Trig2","Trig3","Trig4"
    };
    for (int i = 0; i < 16; ++i) {
        mTriggerChords[i] = defaultChords[i];
    }
}

void PerformanceModePanel::render(StudioEngine* engine) {
    // Fill the entire screen with the OGRE viewport
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##PerfMode", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);

    // OGRE render occupies 80% of width
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float viewW = avail.x * 0.80f;
    float viewH = avail.y;

    // Resize RenderTexture to match performance viewport size
    static uint32_t lastW = 0, lastH = 0;
    auto w = static_cast<uint32_t>(viewW);
    auto h = static_cast<uint32_t>(viewH);
    if (engine && (w != lastW || h != lastH)) {
        engine->resizeRenderTexture(w, h);
        lastW = w; lastH = h;
    }
    if (engine) engine->updateRenderTarget();

    if (engine) {
        ImTextureID texId = engine->getRenderTextureID();
        if (texId) {
            ImGui::Image(texId, {viewW, viewH}, {0.0f, 0.0f}, {1.0f, 1.0f});
        }
    }

    // Right column: trigger grid + faders
    ImGui::SameLine();
    ImGui::BeginChild("##PerfRight", {avail.x - viewW - 4, viewH}, false);
    renderTriggerGrid();
    ImGui::Separator();
    renderFaders();
    ImGui::EndChild();

    // Bottom overlays (drawn over the viewport)
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 winPos    = ImGui::GetWindowPos();
    ImVec2 winSize   = ImGui::GetWindowSize();

    renderVUMeters();
    renderBPMOverlay();

    // Panic button (bottom center of viewport)
    ImGui::SetCursorPos({viewW / 2 - 60, viewH - 50});
    renderPanicButton();

    // Keyboard shortcuts for triggers: 1-9 = triggers 0-8, Q-W-E-R-T = triggers 9-13
    {
        ImGuiKey numKeys[] = {ImGuiKey_1,ImGuiKey_2,ImGuiKey_3,ImGuiKey_4,ImGuiKey_5,ImGuiKey_6,ImGuiKey_7,ImGuiKey_8,ImGuiKey_9};
        for (int i = 0; i < 9; ++i) {
            if (ImGui::IsKeyPressed(numKeys[i])) {
                mTriggerStates[i] = !mTriggerStates[i];
                try {
                    sol::object obj = mLua["ChordSystem"];
                    if (obj.valid()) {
                        sol::table cs = obj.as<sol::table>();
                        sol::function fn = cs["toggle"];
                        if (fn.valid()) fn(cs, mTriggerChords[i]);
                    }
                } catch (...) {}
            }
        }
        ImGuiKey qwerKeys[] = {ImGuiKey_Q,ImGuiKey_W,ImGuiKey_E,ImGuiKey_R,ImGuiKey_T};
        for (int i = 0; i < 5; ++i) {
            if (ImGui::IsKeyPressed(qwerKeys[i])) {
                int idx = 9 + i;
                if (idx < 16) {
                    mTriggerStates[idx] = !mTriggerStates[idx];
                    try {
                        sol::object obj = mLua["ChordSystem"];
                        if (obj.valid()) {
                            sol::table cs = obj.as<sol::table>();
                            sol::function fn = cs["toggle"];
                            if (fn.valid()) fn(cs, mTriggerChords[idx]);
                        }
                    } catch (...) {}
                }
            }
        }
    }

    ImGui::End();
}

void PerformanceModePanel::renderTriggerGrid() {
    ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "TRIGGER");

    static const ImU32 btnColors[4] = {
        IM_COL32(0,150,150,255), IM_COL32(150,0,150,255),
        IM_COL32(150,150,0,255), IM_COL32(200,100,0,255)
    };

    for (int i = 0; i < 16; ++i) {
        if (i % 4 != 0) ImGui::SameLine();

        // Color based on active state — active triggers pulse with beat
        if (mTriggerStates[i]) {
            // Glow pulse: read beatFrac from RootTimeNode for smooth animation
            float glow = 0.7f;
            auto* rootTime = RootTimeNode::instance();
            if (rootTime) {
                auto& outs = rootTime->getOutputs();
                auto bfIt = outs.find("beatFrac");
                if (bfIt != outs.end()) {
                    float bf = bfIt->second->getValue();
                    glow = 0.6f + 0.4f * (1.0f - bf); // bright on beat, dim between
                }
            }
            ImGui::PushStyleColor(ImGuiCol_Button, {glow, glow, glow, 0.95f});
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(
                ((btnColors[i/4] >> 0)  & 0xFF) / 255.0f,
                ((btnColors[i/4] >> 8)  & 0xFF) / 255.0f,
                ((btnColors[i/4] >> 16) & 0xFF) / 255.0f, 0.8f));
        }

        std::string lbl = mTriggerChords[i] + "##tg" + std::to_string(i);
        if (ImGui::Button(lbl.c_str(), {56, 48})) {
            mTriggerStates[i] = !mTriggerStates[i];
            // Toggle chord via Lua chord system
            try {
                sol::object obj = mLua["ChordSystem"];
                if (obj.valid() && obj.get_type() == sol::type::table) {
                    sol::table chordSys = obj.as<sol::table>();
                    sol::function toggleFn = chordSys["toggle"];
                    if (toggleFn.valid()) {
                        toggleFn(chordSys, mTriggerChords[i]);
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[PerfMode] Chord toggle error: " << e.what() << std::endl;
            }
        }
        ImGui::PopStyleColor();
    }
}

void PerformanceModePanel::renderFaders() {
    ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "FADERS");

    auto* animator = Animator::instance();
    for (int i = 0; i < 8; ++i) {
        if (i % 4 != 0) ImGui::SameLine();

        auto& slot = mFaders[i];
        std::string lbl = "##fader" + std::to_string(i);

        // Try to sync value from DAG
        if (animator && !slot.nodeName.empty() && !slot.portName.empty()) {
            auto* node = animator->getRegisteredNode(slot.nodeName);
            if (node) {
                auto& inputs = node->getInputs();
                auto it = inputs.find(slot.portName);
                if (it != inputs.end()) slot.value = it->second->getValue();
            }
        }

        ImGui::BeginGroup();
        ImGui::VSliderFloat(lbl.c_str(), {44, 100}, &slot.value, 0.0f, 10.0f);

        // Write back to DAG
        if (animator && !slot.nodeName.empty() && !slot.portName.empty()) {
            auto* node = animator->getRegisteredNode(slot.nodeName);
            if (node) {
                auto& inputs = node->getInputs();
                auto it = inputs.find(slot.portName);
                if (it != inputs.end()) it->second->setValue(slot.value);
            }
        }

        std::string caption = slot.portName.empty() ? "---" :
            (slot.nodeName.substr(0, std::min((size_t)6, slot.nodeName.size())) + "." + slot.portName);
        ImGui::TextDisabled("%s", caption.c_str());
        ImGui::EndGroup();

        // Right-click context menu for port assignment
        std::string ctxId = "##faderCtx" + std::to_string(i);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup(ctxId.c_str());
        }
        if (ImGui::BeginPopup(ctxId.c_str())) {
            ImGui::TextDisabled("Assign Port");
            if (animator) {
                for (auto& name : animator->getRegisteredNodeNames()) {
                    auto* node = animator->getRegisteredNode(name);
                    if (!node) continue;
                    if (ImGui::BeginMenu(name.c_str())) {
                        for (auto& [portName, port] : node->getInputs()) {
                            if (ImGui::MenuItem(portName.c_str())) {
                                slot.nodeName = name;
                                slot.portName = portName;
                                slot.value = port->getValue();
                            }
                        }
                        ImGui::EndMenu();
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Clear")) {
                slot.nodeName.clear();
                slot.portName.clear();
                slot.value = 0.0f;
            }
            ImGui::EndPopup();
        }
    }
}

void PerformanceModePanel::renderVUMeters() {
    // Try to read bands from AudioAnalyzerNode in DAG
    auto* animator = Animator::instance();
    bool foundAnalyzer = false;
    if (animator) {
        for (auto& name : animator->getRegisteredNodeNames()) {
            auto* node = animator->getRegisteredNode(name);
            if (node && node->getTypeName() == "AudioAnalyzerNode") {
                auto* analyzerNode = dynamic_cast<AudioAnalyzerNode*>(node);
                if (analyzerNode) {
                    // Average bands into low (0-2), mid (3-5), high (6-7)
                    mBands[0] = (analyzerNode->getBand(0) + analyzerNode->getBand(1) + analyzerNode->getBand(2)) / 3.0f;
                    mBands[1] = (analyzerNode->getBand(3) + analyzerNode->getBand(4) + analyzerNode->getBand(5)) / 3.0f;
                    mBands[2] = (analyzerNode->getBand(6) + analyzerNode->getBand(7)) / 2.0f;
                    mRMS = analyzerNode->getRMS();
                    foundAnalyzer = true;
                }
                break;
            }
        }
        // Sync BPM from BeatDetectorNode if present
        for (auto& name : animator->getRegisteredNodeNames()) {
            auto* node = animator->getRegisteredNode(name);
            if (node && node->getTypeName() == "BeatDetectorNode") {
                auto& outputs = node->getOutputs();
                auto bpmIt = outputs.find("bpm");
                if (bpmIt != outputs.end()) {
                    float detectedBPM = bpmIt->second->getValue();
                    if (detectedBPM > 20.0f) mBPM = detectedBPM;
                }
                break;
            }
        }
    }

    if (!foundAnalyzer) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 pos2 = ImGui::GetWindowPos();
        ImVec2 sz2  = ImGui::GetWindowSize();
        draw->AddText({pos2.x + 10.0f, pos2.y + sz2.y - 80.0f},
                      IM_COL32(120, 120, 120, 200), "No Audio");
    }

    // VU meters shown as colored bars in the bottom-left of the viewport window
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 pos       = ImGui::GetWindowPos();
    ImVec2 sz        = ImGui::GetWindowSize();

    static const ImU32 cols[5] = {
        IM_COL32(200,50,50,200),   // Sub: red
        IM_COL32(0,200,255,200),   // Low: cyan
        IM_COL32(200,0,255,200),   // Mid: magenta
        IM_COL32(255,200,0,200),   // High: yellow
        IM_COL32(200,200,200,200), // Air: white
    };
    static const char* labels[5] = {"Sub","Low","Mid","High","Air"};

    // Distribute 8 bands into 5 VU groups
    float vuBands[5] = {0};
    if (mBands[0] > 0 || mBands[1] > 0 || mBands[2] > 0) {
        vuBands[0] = mBands[0] * 0.7f;  // Sub (low freq emphasis)
        vuBands[1] = mBands[0];          // Low
        vuBands[2] = mBands[1];          // Mid
        vuBands[3] = mBands[2];          // High
        vuBands[4] = mBands[2] * 0.5f;  // Air (high freq tail)
    }

    float barW = 14.0f, maxH = 60.0f;
    float x0 = pos.x + 10.0f;
    float yBase = pos.y + sz.y - 20.0f;

    for (int b = 0; b < 5; ++b) {
        float h = vuBands[b] * maxH;
        draw->AddRectFilled(
            {x0 + b*18, yBase - h},
            {x0 + b*18 + barW, yBase},
            cols[b], 2.0f);
        draw->AddText({x0 + b*18, yBase + 2},
                      IM_COL32(180,180,180,200), labels[b]);
    }
}

void PerformanceModePanel::renderBPMOverlay() {
    // BPM displayed large in the bottom center of the viewport
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 sz  = ImGui::GetWindowSize();
    ImDrawList* draw = ImGui::GetWindowDrawList();

    char bpmStr[32];
    snprintf(bpmStr, sizeof(bpmStr), "%.0f BPM", mBPM);

    // Large text: use two passes (shadow then text) for readability
    ImVec2 textPos = {pos.x + sz.x * 0.35f, pos.y + sz.y - 50.0f};
    draw->AddText(ImGui::GetFont(), 36.0f,
                  {textPos.x + 2, textPos.y + 2},
                  IM_COL32(0, 0, 0, 180), bpmStr);
    draw->AddText(ImGui::GetFont(), 36.0f,
                  textPos,
                  IM_COL32(0, 220, 220, 255), bpmStr);
}

void PerformanceModePanel::renderPanicButton() {
    ImGui::PushStyleColor(ImGuiCol_Button,        {0.7f, 0.0f, 0.0f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {1.0f, 0.0f, 0.0f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {1.0f, 0.3f, 0.3f, 1.0f});

    if (ImGui::Button("PANIC", {120, 36})) {
        // Reset all DAG port values to 0
        auto* animator = Animator::instance();
        if (animator) {
            for (auto& name : animator->getRegisteredNodeNames()) {
                auto* node = animator->getRegisteredNode(name);
                if (!node) continue;
                for (auto& [_, port] : node->getInputs()) {
                    port->setValue(0.0f);
                }
            }
        }
        std::cout << "[PerfMode] PANIC — all ports reset to 0" << std::endl;
    }

    ImGui::PopStyleColor(3);
}

} // namespace bbfx
