#include "PerformanceModePanel.h"
#include "CompositorStackPanel.h"
#include "../StudioEngine.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../../core/AnimationPort.h"
#include "../../core/PrimitiveNodes.h"
#include "../../audio/AudioAnalyzer.h"
#include "../../audio/BeatDetector.h"

#include "../../core/ParamSpec.h"

#include <imgui.h>
#include <sol/sol.hpp>
#include <GL/gl.h>
#include <OgreRoot.h>
#include <OgreViewport.h>
#include <OgreCompositorManager.h>
#include <algorithm>
#include <iostream>

namespace bbfx {

PerformanceModePanel::PerformanceModePanel(sol::state& lua) : mLua(lua) {
    // Initialize first trigger page with default chord names
    static const char* defaultChords[16] = {
        "Intro","Verse","Chorus","Bridge",
        "Drop","Build","Break","Outro",
        "FX1","FX2","FX3","FX4",
        "Trig1","Trig2","Trig3","Trig4"
    };
    std::array<TriggerSlot, 16> page1{};
    for (int i = 0; i < 16; ++i) {
        page1[i].label = defaultChords[i];
        page1[i].action = std::string("chord:") + defaultChords[i];
        page1[i].hue = static_cast<float>(i % 4) / 4.0f;
    }
    mTriggerPages.push_back(page1);
}

void PerformanceModePanel::onLearnParam(const std::string& nodeName, const std::string& portName) {
    if (mLearnFaderIndex < 0 || mLearnFaderIndex >= 8) return;
    auto& slot = mFaders[mLearnFaderIndex];
    slot.nodeName = nodeName;
    slot.portName = portName;
    slot.value = 0.0f;
    slot.minVal = 0.0f;
    slot.maxVal = 1.0f;
    // Try to read min/max from ParamSpec
    auto* animator = Animator::instance();
    if (animator) {
        auto* node = animator->getRegisteredNode(nodeName);
        if (node && node->getParamSpec()) {
            auto* p = node->getParamSpec()->getParam(portName);
            if (p && p->type == ParamType::FLOAT) {
                slot.minVal = p->minVal;
                slot.maxVal = p->maxVal;
            }
        }
    }
    mLearnFaderIndex = -1;
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
    auto w = static_cast<uint32_t>(viewW);
    auto h = static_cast<uint32_t>(viewH);
    if (engine && (w != mLastPerfW || h != mLastPerfH)) {
        engine->resizeRenderTexture(w, h);
        if (mCompositorStack) {
            mCompositorStack->invalidateApplied();
            mCompositorsPending = true;
        }
        mLastPerfW = w; mLastPerfH = h;
    }
    // Apply compositors AFTER resize (the resize destroys the viewport and its compositors)
    if (mCompositorsPending && engine) {
        auto* rt1 = engine->getRenderTarget();
        if (rt1 && rt1->getNumViewports() > 0) {
            auto* viewport = rt1->getViewport(0);
            if (viewport && mCompositorStack) {
                mCompositorStack->syncStackOrder();
                if (!mCompositorStack->getStackOrder().empty()) {
                    viewport->setClearEveryFrame(false);
                    mCompositorStack->applyToViewport(viewport);
                }
            }
        }
        mCompositorsPending = false;
    }

    // Render scene + compositors to RT1
    if (engine) engine->updateRenderTarget();

    // Display RT1 (with compositor effects applied)
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

    // Tab key cycles trigger pages
    if (ImGui::IsKeyPressed(ImGuiKey_Tab) && !ImGui::GetIO().WantTextInput) {
        if (!mTriggerPages.empty()) {
            mCurrentTriggerPage = (mCurrentTriggerPage + 1) % static_cast<int>(mTriggerPages.size());
        }
    }

    // Keyboard shortcuts for triggers: 1-9 = triggers 0-8, Q-W-E-R-T = triggers 9-13
    if (!mTriggerPages.empty()) {
        auto& page = mTriggerPages[mCurrentTriggerPage];
        ImGuiKey numKeys[] = {ImGuiKey_1,ImGuiKey_2,ImGuiKey_3,ImGuiKey_4,ImGuiKey_5,ImGuiKey_6,ImGuiKey_7,ImGuiKey_8,ImGuiKey_9};
        for (int i = 0; i < 9; ++i) {
            if (ImGui::IsKeyPressed(numKeys[i])) {
                page[i].active = !page[i].active;
                executeTriggerAction(page[i].action);
            }
        }
        ImGuiKey qwerKeys[] = {ImGuiKey_Q,ImGuiKey_W,ImGuiKey_E,ImGuiKey_R,ImGuiKey_T};
        for (int i = 0; i < 5; ++i) {
            if (ImGui::IsKeyPressed(qwerKeys[i])) {
                int idx = 9 + i;
                if (idx < 16) {
                    page[idx].active = !page[idx].active;
                    executeTriggerAction(page[idx].action);
                }
            }
        }
    }

    // Beat flash border
    {
        float beatFrac = 0.0f;
        auto* rootTime = RootTimeNode::instance();
        if (rootTime) {
            auto& outs = rootTime->getOutputs();
            auto bfIt = outs.find("beatFrac");
            if (bfIt != outs.end()) beatFrac = bfIt->second->getValue();
        }
        float alpha = std::max(0.0f, 1.0f - beatFrac * 3.0f) * 0.4f;
        if (alpha > 0.01f) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 wp = ImGui::GetWindowPos();
            ImVec2 ws = ImGui::GetWindowSize();
            dl->AddRect(wp, {wp.x + ws.x, wp.y + ws.y},
                IM_COL32(0, 220, 220, static_cast<int>(alpha * 255)), 0, 0, 3.0f);
        }
    }

    ImGui::End();
}

void PerformanceModePanel::renderTriggerGrid() {
    if (mTriggerPages.empty()) return;

    // Page indicator
    ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "TRIGGER  Page %d/%d",
                       mCurrentTriggerPage + 1, static_cast<int>(mTriggerPages.size()));
    ImGui::SameLine();
    if (ImGui::SmallButton("+##addPage")) {
        mTriggerPages.push_back(std::array<TriggerSlot, 16>{});
    }

    auto& page = mTriggerPages[mCurrentTriggerPage];

    for (int i = 0; i < 16; ++i) {
        if (i % 4 != 0) ImGui::SameLine();

        auto& trig = page[i];

        // Color: use hue with saturation/value based on active state
        float r, g, b;
        if (trig.active) {
            float glow = 0.9f;
            auto* rootTime = RootTimeNode::instance();
            if (rootTime) {
                auto& outs = rootTime->getOutputs();
                auto bfIt = outs.find("beatFrac");
                if (bfIt != outs.end()) {
                    float bf = bfIt->second->getValue();
                    glow = 0.7f + 0.3f * (1.0f - bf);
                }
            }
            ImGui::ColorConvertHSVtoRGB(trig.hue, 0.5f, glow, r, g, b);
        } else {
            ImGui::ColorConvertHSVtoRGB(trig.hue, 0.6f, 0.3f, r, g, b);
        }
        ImGui::PushStyleColor(ImGuiCol_Button, {r, g, b, 0.95f});

        // Node activity indicator
        auto* animator = Animator::instance();
        std::string prefix;
        if (!trig.action.empty()) {
            auto cp = trig.action.find(':');
            if (cp != std::string::npos) prefix = trig.action.substr(0, cp);
        }
        if (prefix == "enable" || prefix == "disable") {
            auto target = trig.action.substr(trig.action.find(':') + 1);
            if (animator) {
                auto* node = animator->getRegisteredNode(target);
                if (node) {
                    ImU32 dotCol = node->isEnabled() ? IM_COL32(0, 200, 0, 255) : IM_COL32(120, 120, 120, 255);
                    ImGui::GetWindowDrawList()->AddCircleFilled(
                        {ImGui::GetCursorScreenPos().x + 52, ImGui::GetCursorScreenPos().y + 4}, 4.0f, dotCol);
                } else {
                    ImGui::GetWindowDrawList()->AddCircleFilled(
                        {ImGui::GetCursorScreenPos().x + 52, ImGui::GetCursorScreenPos().y + 4}, 4.0f,
                        IM_COL32(255, 140, 0, 255));
                }
            }
        }

        std::string lbl = trig.label + "##tg" + std::to_string(i);
        ImGui::Button(lbl.c_str(), {56, 48});
        if (trig.momentary) {
            // Momentary: active while button held
            bool pressed = ImGui::IsItemActive();
            if (pressed && !trig.active) {
                trig.active = true;
                executeTriggerAction(trig.action);
            } else if (!pressed && trig.active) {
                trig.active = false;
                if (trig.action.find("enable:") == 0) {
                    executeTriggerAction("disable:" + trig.action.substr(7));
                } else if (trig.action.find("disable:") == 0) {
                    executeTriggerAction("enable:" + trig.action.substr(8));
                }
            }
        } else {
            // Toggle: flip on click
            if (ImGui::IsItemClicked()) {
                trig.active = !trig.active;
                executeTriggerAction(trig.action);
            }
        }
        ImGui::PopStyleColor();

        // Right-click context menu for trigger assignment
        std::string ctxTrig = "##trigCtx" + std::to_string(i);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup(ctxTrig.c_str());
        }
        if (ImGui::BeginPopup(ctxTrig.c_str())) {
            ImGui::TextDisabled("Assign Action");
            if (animator) {
                if (ImGui::BeginMenu("Enable Node")) {
                    for (auto& name : animator->getRegisteredNodeNames()) {
                        if (ImGui::MenuItem(name.c_str())) {
                            trig.action = "enable:" + name;
                            trig.label = "En:" + name.substr(0, std::min((size_t)4, name.size()));
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Disable Node")) {
                    for (auto& name : animator->getRegisteredNodeNames()) {
                        if (ImGui::MenuItem(name.c_str())) {
                            trig.action = "disable:" + name;
                            trig.label = "Di:" + name.substr(0, std::min((size_t)4, name.size()));
                        }
                    }
                    ImGui::EndMenu();
                }
            }
            ImGui::Separator();
            ImGui::SliderFloat("Hue", &trig.hue, 0.0f, 1.0f);
            ImGui::Checkbox("Momentary", &trig.momentary);
            if (ImGui::MenuItem("Clear")) {
                trig.action.clear();
                trig.label = "+";
                trig.active = false;
            }
            ImGui::EndPopup();
        }
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
        ImGui::VSliderFloat(lbl.c_str(), {44, 100}, &slot.value, slot.minVal, slot.maxVal);

        // Write back to DAG
        if (animator && !slot.nodeName.empty() && !slot.portName.empty()) {
            auto* node = animator->getRegisteredNode(slot.nodeName);
            if (node) {
                auto& inputs = node->getInputs();
                auto it = inputs.find(slot.portName);
                if (it != inputs.end()) {
                    it->second->setValue(slot.value);
                    // Record to automation if recording
                    if (mIsRecording && mRecordValueCb && ImGui::IsItemActive()) {
                        mRecordValueCb(slot.nodeName, slot.portName, slot.value, mCurrentBeat);
                    }
                }
            }
        }

        // Numeric value
        ImGui::Text("%.2f", slot.value);
        // Label
        std::string caption = slot.portName.empty() ? "---" :
            (slot.nodeName.substr(0, std::min((size_t)6, slot.nodeName.size())) + "." + slot.portName);
        ImGui::TextDisabled("%s", caption.c_str());
        // Tooltip with full name
        if (ImGui::IsItemHovered() && !slot.portName.empty()) {
            ImGui::SetTooltip("%s / %s", slot.nodeName.c_str(), slot.portName.c_str());
        }
        // Learn button
        bool isLearning = (mLearnFaderIndex == i);
        if (isLearning) ImGui::PushStyleColor(ImGuiCol_Button, {1.0f, 0.3f, 0.0f, 1.0f});
        if (ImGui::SmallButton(isLearning ? ("L##learn" + std::to_string(i)).c_str()
                                          : ("L##learn" + std::to_string(i)).c_str())) {
            mLearnFaderIndex = isLearning ? -1 : i;
        }
        if (isLearning) ImGui::PopStyleColor();
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

void PerformanceModePanel::executeTriggerAction(const std::string& action) {
    if (action.empty()) return;
    auto colonPos = action.find(':');
    if (colonPos == std::string::npos) return;
    std::string prefix = action.substr(0, colonPos);
    std::string target = action.substr(colonPos + 1);

    if (prefix == "chord") {
        try {
            sol::object obj = mLua["ChordSystem"];
            if (obj.valid()) {
                sol::table cs = obj.as<sol::table>();
                sol::function fn = cs["toggle"];
                if (fn.valid()) fn(cs, target);
            }
        } catch (const std::exception& e) {
            std::cerr << "[PerfMode] Chord toggle error: " << e.what() << std::endl;
        }
    } else if (prefix == "enable" || prefix == "disable") {
        auto* animator = Animator::instance();
        if (animator) {
            auto* node = animator->getRegisteredNode(target);
            if (node) node->setEnabled(prefix == "enable");
        }
    } else if (prefix == "compositor") {
        auto* animator = Animator::instance();
        if (animator) {
            auto* node = animator->getRegisteredNode(target);
            if (node) {
                auto* spec = node->getParamSpec();
                if (spec) {
                    auto* p = spec->getParam("enabled");
                    if (p) p->boolVal = !p->boolVal;
                }
            }
        }
    } else if (prefix == "preset") {
        // Load preset via Lua debugger API
        try {
            sol::table dbg = mLua["dbg"];
            if (dbg.valid()) {
                sol::function fn = dbg["preset"];
                if (fn.valid()) fn(target);
            }
        } catch (const std::exception& e) {
            std::cerr << "[PerfMode] Preset load error: " << e.what() << std::endl;
        }
    } else if (prefix == "reset") {
        // Reset a port to 0: target = "nodeName.portName"
        auto dotPos = target.find('.');
        if (dotPos != std::string::npos) {
            std::string nodeName = target.substr(0, dotPos);
            std::string portName = target.substr(dotPos + 1);
            auto* animator = Animator::instance();
            if (animator) {
                auto* node = animator->getRegisteredNode(nodeName);
                if (node) {
                    auto& inputs = node->getInputs();
                    auto it = inputs.find(portName);
                    if (it != inputs.end()) it->second->setValue(0.0f);
                }
            }
        }
    }
}

void PerformanceModePanel::applyCompositorChain(StudioEngine* engine) {
    // Set flag to apply compositors after the next resize in render().
    (void)engine;
    mCompositorsPending = true;
}

void PerformanceModePanel::removeCompositorChain(StudioEngine* engine) {
    if (!engine || !mCompositorStack) return;
    auto* rt1 = engine->getRenderTarget();
    if (!rt1 || rt1->getNumViewports() == 0) return;
    auto* viewport = rt1->getViewport(0);
    if (!viewport) return;

    // Remove all compositors from the viewport
    mCompositorStack->removeFromViewport(viewport);

    // Remove ALL compositors via OGRE directly (safety net)
    Ogre::CompositorManager::getSingleton().removeCompositorChain(viewport);

    // Restore auto-clear
    viewport->setClearEveryFrame(true);

    // Reset pending flag
    mCompositorsPending = false;

    // Force FBO cache reset
    engine->invalidateFBOCache();

    // Reset cached F5 size so next F5 entry triggers a resize (which re-creates RT1 cleanly)
    mLastPerfW = 0;
    mLastPerfH = 0;
}

} // namespace bbfx
