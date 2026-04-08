#include "ToastSystem.h"

namespace bbfx {

ToastSystem& ToastSystem::instance() {
    static ToastSystem s;
    return s;
}

void ToastSystem::toast(const std::string& message, ToastSeverity severity, float durationSec) {
    mToasts.push_back({message, severity, durationSec, durationSec});
    // Trim oldest if exceeding max
    while (static_cast<int>(mToasts.size()) > kMaxToasts) {
        mToasts.erase(mToasts.begin());
    }
}

void ToastSystem::render() {
    if (mToasts.empty()) return;

    float dt = ImGui::GetIO().DeltaTime;
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    float yOffset = displaySize.y - 40; // start from bottom

    // Iterate in reverse (newest at bottom)
    for (int i = static_cast<int>(mToasts.size()) - 1; i >= 0; --i) {
        auto& t = mToasts[i];
        t.remaining -= dt;
        if (t.remaining <= 0) {
            mToasts.erase(mToasts.begin() + i);
            continue;
        }

        // Fade out in last 0.5s
        float alpha = (t.remaining < 0.5f) ? t.remaining / 0.5f : 1.0f;

        ImVec4 col;
        switch (t.severity) {
            case ToastSeverity::Info:    col = {0.2f, 0.5f, 0.8f, alpha}; break;
            case ToastSeverity::Warning: col = {0.8f, 0.7f, 0.2f, alpha}; break;
            case ToastSeverity::Error:   col = {0.9f, 0.2f, 0.2f, alpha}; break;
        }

        ImVec2 textSize = ImGui::CalcTextSize(t.message.c_str());
        float boxW = textSize.x + 20;
        float boxH = textSize.y + 10;
        float boxX = displaySize.x - boxW - 10;
        float boxY = yOffset - boxH;

        ImGui::SetNextWindowPos({boxX, boxY});
        ImGui::SetNextWindowSize({boxW, boxH});
        ImGui::SetNextWindowBgAlpha(alpha * 0.85f);

        char winName[64];
        snprintf(winName, sizeof(winName), "##toast_%d", i);
        ImGui::Begin(winName, nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);

        ImGui::TextColored(col, "%s", t.message.c_str());
        ImGui::End();

        yOffset -= boxH + 5;
    }
}

} // namespace bbfx
