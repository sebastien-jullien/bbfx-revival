#pragma once
#include <imgui.h>
#include <map>
#include <string>

namespace bbfx {

/// Rich tooltip with hover delay and optional keyboard shortcut display.
/// Usage: call after each interactive widget with IsItemHovered().
inline void RichTooltip(const char* text, const char* shortcut = nullptr) {
    if (!ImGui::IsItemHovered()) return;

    // Simple timer based on hover duration
    static std::map<ImGuiID, float> sHoverTimes;
    ImGuiID id = ImGui::GetItemID();
    float dt = ImGui::GetIO().DeltaTime;

    auto& t = sHoverTimes[id];
    t += dt;

    if (t < 0.5f) return; // don't show until 0.5s hover

    ImGui::BeginTooltip();
    ImGui::TextUnformatted(text);
    if (shortcut) {
        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", shortcut);
    }
    ImGui::EndTooltip();

    // Cleanup: remove stale entries periodically
    if (sHoverTimes.size() > 200) {
        sHoverTimes.clear();
    }
}

/// Reset tooltip timer for a widget (call when not hovered)
inline void RichTooltipReset(ImGuiID id) {
    // No-op — handled by checking IsItemHovered above
}

} // namespace bbfx
