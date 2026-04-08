#include "ShaderGalleryPanel.h"
#include "../ShaderPreviewRenderer.h"

#include <imgui.h>
#include <iostream>

namespace bbfx {

ShaderGalleryPanel::ShaderGalleryPanel() {
    // BBFx procedural shaders (matching the 8 from brainstorming)
    mShaders = {
        {"Plasma",              "plasma.frag",              "passthrough.vert"},
        {"Voronoi",             "voronoi.frag",             "passthrough.vert"},
        {"Mandelbrot",          "mandelbrot.frag",          "passthrough.vert"},
        {"Truchet",             "truchet.frag",             "passthrough.vert"},
        {"Flow Field",          "flowfield.frag",           "passthrough.vert"},
        {"Tunnel",              "tunnel.frag",              "passthrough.vert"},
        {"Reaction Diffusion",  "reaction_diffusion.frag",  "passthrough.vert"},
        {"Sphere Trace",        "sphere_trace.frag",        "passthrough.vert"},
    };
}

void ShaderGalleryPanel::render() {
    ImGui::Begin("Shader Gallery");

    if (!mRenderer || !mRenderer->isInitialized()) {
        ImGui::TextDisabled("Preview renderer not available");
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("Double-click to apply shader to selected object");
    ImGui::Separator();

    float panelWidth = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, static_cast<int>(panelWidth / 80.0f)); // 64px + padding
    float cellSize = 64.0f;

    for (int i = 0; i < static_cast<int>(mShaders.size()); ++i) {
        auto& shader = mShaders[i];

        if (i % cols != 0) ImGui::SameLine();

        ImGui::BeginGroup();

        // Get animated preview
        ImTextureID tex = mRenderer->getShaderPreview(shader.fragFile);
        bool selected = (i == mSelectedShader);

        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, {0.0f, 0.6f, 0.8f, 1.0f});
        }

        ImGui::PushID(i);
        if (ImGui::ImageButton("##shaderBtn", tex, {cellSize, cellSize})) {
            mSelectedShader = i;
        }

        // Double-click = apply
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            if (mApplyCb) {
                mApplyCb(shader.vertFile, shader.fragFile, mSelectedSceneObj);
            }
        }

        // Drag-drop source
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("SHADER_NAME", shader.fragFile.c_str(),
                                     shader.fragFile.size() + 1);
            ImGui::Image(tex, {32, 32});
            ImGui::SameLine();
            ImGui::TextUnformatted(shader.name.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\n%s", shader.name.c_str(), shader.fragFile.c_str());
        }

        ImGui::PopID();
        if (selected) ImGui::PopStyleColor();

        // Name below thumbnail (truncated)
        float textW = ImGui::CalcTextSize(shader.name.c_str()).x;
        if (textW > cellSize) {
            std::string truncated = shader.name.substr(0, 8) + "...";
            ImGui::TextDisabled("%s", truncated.c_str());
        } else {
            ImGui::TextDisabled("%s", shader.name.c_str());
        }

        ImGui::EndGroup();
    }

    ImGui::End();
}

} // namespace bbfx
