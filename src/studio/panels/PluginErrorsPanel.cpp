#include "PluginErrorsPanel.h"

#include <chrono>
#include <cstring>
#include <ctime>
#include <string>

#include <imgui.h>

#include "../../plugin/PluginErrorLog.h"
#include "../../plugin/PluginManager.h"
#include "../ToastSystem.h"

namespace bbfx {

namespace {

std::string formatTime(std::chrono::system_clock::time_point tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tmv);
    return buf;
}

ImVec4 severityColor(PluginErrorLog::Severity s) {
    switch (s) {
        case PluginErrorLog::Severity::Info:    return {0.60f, 0.80f, 1.0f, 1.0f};
        case PluginErrorLog::Severity::Warning: return {1.00f, 0.85f, 0.30f, 1.0f};
        case PluginErrorLog::Severity::Error:   return {1.00f, 0.45f, 0.45f, 1.0f};
    }
    return {1.0f, 1.0f, 1.0f, 1.0f};
}

} // anonymous

void PluginErrorsPanel::render(bool* open) {
    if (!open || !*open) return;

    ImGui::SetNextWindowSize({ 680, 400 }, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Plugin Errors##pmgr_err", open)) {
        ImGui::End();
        return;
    }

    // Toolbar
    static const char* kSeverityLabels[] = { "All", "Warning+", "Error only" };
    ImGui::SetNextItemWidth(140.0f);
    ImGui::Combo("Severity", &mSeverityFilter, kSeverityLabels,
                  IM_ARRAYSIZE(kSeverityLabels));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("##pluginFilter", "Plugin id filter",
                              mPluginIdFilter, sizeof(mPluginIdFilter));
    ImGui::SameLine();
    if (ImGui::SmallButton("Acknowledge all")) {
        PluginErrorLog::instance().acknowledgeAll();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
        PluginErrorLog::instance().clear();
    }

    ImGui::Separator();

    PluginErrorLog::Severity minSev = PluginErrorLog::Severity::Info;
    if (mSeverityFilter == 1) minSev = PluginErrorLog::Severity::Warning;
    if (mSeverityFilter == 2) minSev = PluginErrorLog::Severity::Error;

    auto entries = PluginErrorLog::instance().snapshot(mPluginIdFilter, minSev);
    if (entries.empty()) {
        ImGui::TextDisabled("(no plugin errors logged)");
    }

    // Render newest first
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        ImGui::PushID(static_cast<int>(std::distance(entries.rbegin(), it)));
        ImGui::TextColored(severityColor(it->severity), "[%s]",
                            PluginErrorLog::severityName(it->severity));
        ImGui::SameLine();
        ImGui::TextDisabled("%s", formatTime(it->timestamp).c_str());
        ImGui::SameLine();
        ImGui::Text("%s", it->pluginId.c_str());
        if (!it->context.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", it->context.c_str());
        }

        // Action: if the plugin is currently FAILED, offer a re-enable button.
        const auto* pinfo = PluginManager::instance().getPlugin(it->pluginId);
        if (pinfo && pinfo->state == PluginState::FAILED) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Re-enable")) {
                // Re-enable means retry load + enable. We clear the lastError
                // so the next success is a clean state.
                auto* mut = PluginManager::instance().getPluginMutable(it->pluginId);
                if (mut) mut->lastError.clear();
                // FAILED -> we need a path back. The public enable() accepts
                // VALIDATED/UNLOADED/DISABLED as source states, so we cycle
                // through unload first.
                PluginManager::instance().unload(it->pluginId);
                if (PluginManager::instance().enable(it->pluginId)) {
                    ToastSystem::instance().toast("Re-enabled " + it->pluginId,
                                                    ToastSeverity::Info, 3.0f);
                } else {
                    ToastSystem::instance().toast("Re-enable failed for " + it->pluginId,
                                                    ToastSeverity::Error, 4.0f);
                }
            }
        }

        ImGui::Indent();
        ImGui::TextWrapped("%s", it->message.c_str());
        ImGui::Unindent();
        ImGui::Spacing();
        ImGui::PopID();
    }

    ImGui::End();
}

} // namespace bbfx
