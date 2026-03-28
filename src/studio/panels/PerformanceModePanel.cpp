#include "PerformanceModePanel.h"
#include "../StudioEngine.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../../core/AnimationPort.h"

#include <imgui.h>
#include <GL/gl.h>
#include <algorithm>
#include <iostream>

namespace bbfx {

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
            ImGui::Image(texId, {viewW, viewH}, {0.0f, 1.0f}, {1.0f, 0.0f});
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

    // Tab focuses the trigger grid
    if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
        ImGui::SetKeyboardFocusHere(-16); // Focus back to first trigger button
    }

    ImGui::End();
}

void PerformanceModePanel::renderTriggerGrid() {
    ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "TRIGGER");

    static const char* chordNames[16] = {
        "Intro","Verse","Chorus","Bridge",
        "Drop","Build","Break","Outro",
        "FX1","FX2","FX3","FX4",
        "Trig1","Trig2","Trig3","Trig4"
    };
    static const ImU32 btnColors[4] = {
        IM_COL32(0,150,150,255), IM_COL32(150,0,150,255),
        IM_COL32(150,150,0,255), IM_COL32(200,100,0,255)
    };

    for (int i = 0; i < 16; ++i) {
        if (i % 4 != 0) ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(
            ((btnColors[i/4] >> 0)  & 0xFF) / 255.0f,
            ((btnColors[i/4] >> 8)  & 0xFF) / 255.0f,
            ((btnColors[i/4] >> 16) & 0xFF) / 255.0f, 0.8f));

        std::string lbl = std::string(chordNames[i]) + "##tg" + std::to_string(i);
        if (ImGui::Button(lbl.c_str(), {56, 48})) {
            std::cout << "[PerfMode] Trigger: " << chordNames[i] << std::endl;
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

        std::string caption = slot.portName.empty() ? "---" : slot.portName;
        ImGui::TextDisabled("%s", caption.substr(0, 4).c_str());
    }
}

void PerformanceModePanel::renderVUMeters() {
    // VU meters shown as colored bars in the bottom-left of the viewport window
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 pos       = ImGui::GetWindowPos();
    ImVec2 sz        = ImGui::GetWindowSize();

    static const ImU32 cols[3] = {
        IM_COL32(0,200,255,200),   // low: cyan
        IM_COL32(200,0,255,200),   // mid: magenta
        IM_COL32(255,200,0,200),   // high: yellow
    };
    static const char* labels[3] = {"LO","MID","HI"};

    float barW = 16.0f, maxH = 60.0f;
    float x0 = pos.x + 10.0f;
    float yBase = pos.y + sz.y - 20.0f;

    for (int b = 0; b < 3; ++b) {
        float h = mBands[b] * maxH;
        draw->AddRectFilled(
            {x0 + b*20, yBase - h},
            {x0 + b*20 + barW, yBase},
            cols[b], 2.0f);
        draw->AddText({x0 + b*20 + 2, yBase + 2},
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
