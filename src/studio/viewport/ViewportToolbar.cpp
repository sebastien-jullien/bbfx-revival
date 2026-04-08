#include "ViewportToolbar.h"
#include "../RichTooltip.h"
#include <imgui.h>

namespace bbfx {

void ViewportToolbar::render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.0f, 2.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4.0f, 2.0f});

    // Tool selection (radio buttons style)
    auto toolBtn = [&](const char* label, Tool t) {
        bool selected = (mTool == t);
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::SmallButton(label)) {
            mTool = t;
        }
        if (selected) {
            ImGui::PopStyleColor();
        }
    };

    toolBtn("W:Move", Tool::Translate);
    RichTooltip("Translate tool — move objects", "W");
    ImGui::SameLine();
    toolBtn("E:Rot",  Tool::Rotate);
    RichTooltip("Rotate tool — spin objects", "E");
    ImGui::SameLine();
    toolBtn("R:Scl",  Tool::Scale);
    RichTooltip("Scale tool — resize objects", "R");

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Space toggle
    if (ImGui::SmallButton(mSpace == Space::World ? "World" : "Local")) {
        mSpace = (mSpace == Space::World) ? Space::Local : Space::World;
    }
    RichTooltip("Toggle World/Local transform space");

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Snap toggle
    if (mSnapOn) ImGui::PushStyleColor(ImGuiCol_Button, {0.2f, 0.5f, 0.2f, 1.0f});
    if (ImGui::SmallButton("Snap")) {
        mSnapOn = !mSnapOn;
    }
    if (mSnapOn) ImGui::PopStyleColor();
    RichTooltip("Snap to grid when transforming", "Ctrl");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50.0f);
    ImGui::DragFloat("##snapSize", &mSnapSize, 0.1f, 0.1f, 10.0f, "%.1f");

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Grid toggle
    if (mGridOn) ImGui::PushStyleColor(ImGuiCol_Button, {0.2f, 0.4f, 0.6f, 1.0f});
    if (ImGui::SmallButton("Grid")) {
        mGridOn = !mGridOn;
    }
    if (mGridOn) ImGui::PopStyleColor();
    RichTooltip("Toggle ground grid display");

    ImGui::SameLine();

    // Overlays toggle
    if (mOverlaysOn) ImGui::PushStyleColor(ImGuiCol_Button, {0.4f, 0.4f, 0.2f, 1.0f});
    if (ImGui::SmallButton("Overlays")) {
        mOverlaysOn = !mOverlaysOn;
    }
    if (mOverlaysOn) ImGui::PopStyleColor();
    RichTooltip("Toggle overlays (gizmos, outlines, labels)");

    ImGui::PopStyleVar(2);
}

} // namespace bbfx
