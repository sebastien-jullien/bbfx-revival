#include "OutputPanel.h"
#include "../StudioEngine.h"
#include <imgui.h>
#include <SDL3/SDL.h>

namespace bbfx {

void OutputPanel::render(StudioEngine* engine) {
    ImGui::Begin("Output");

    if (!engine) { ImGui::TextDisabled("Engine not available"); ImGui::End(); return; }

    bool isOpen = engine->isOutputOpen();

    // Toggle
    if (ImGui::Button(isOpen ? "Close Output" : "Open Output")) {
        if (isOpen) engine->closeOutputWindow();
        else engine->openOutputWindow();
    }

    if (isOpen) {
        ImGui::SameLine();
        if (ImGui::Button("Fullscreen")) engine->toggleOutputFullscreen();
        ImGui::SameLine();
        ImGui::TextDisabled("(F11)");

        ImGui::Separator();

        // Resolution presets
        ImGui::TextDisabled("Resolution:");
        if (ImGui::Button("720p"))  engine->setOutputResolution(1280, 720);
        ImGui::SameLine();
        if (ImGui::Button("1080p")) engine->setOutputResolution(1920, 1080);
        ImGui::SameLine();
        if (ImGui::Button("4K"))    engine->setOutputResolution(3840, 2160);

        ImGui::Separator();

        // Monitor selection
        ImGui::TextDisabled("Monitor:");
        int displayCount = 0;
        auto* displays = SDL_GetDisplays(&displayCount);
        if (displays && displayCount > 0) {
            for (int i = 0; i < displayCount; ++i) {
                const char* name = SDL_GetDisplayName(displays[i]);
                SDL_Rect bounds;
                SDL_GetDisplayBounds(displays[i], &bounds);
                char label[128];
                snprintf(label, sizeof(label), "%d: %s (%dx%d)",
                         i + 1, name ? name : "Display", bounds.w, bounds.h);
                if (ImGui::Button(label)) {
                    engine->setOutputMonitor(i);
                }
                if (i + 1 < displayCount) ImGui::SameLine();
            }
            SDL_free(displays);
        } else {
            ImGui::TextDisabled("No displays detected");
        }

        ImGui::Separator();

        // Preview thumbnail
        ImTextureID outTex = engine->getOutputTextureID();
        if (outTex) {
            ImGui::TextDisabled("Preview:");
            float availW = ImGui::GetContentRegionAvail().x;
            float aspect = 16.0f / 9.0f;
            float previewW = std::min(availW, 320.0f);
            ImGui::Image(outTex, {previewW, previewW / aspect});
        }
    } else {
        ImGui::TextDisabled("Output window closed");
    }

    ImGui::End();
}

} // namespace bbfx
