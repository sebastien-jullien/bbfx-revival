#include "PluginManagerPanel.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <memory>
#include <regex>

#include <imgui.h>

#include "../../plugin/PluginManager.h"
#include "../../plugin/PluginRegistry.h"
#include "../../plugin/PluginErrorLog.h"
#include "../ToastSystem.h"
#include "../commands/CommandManager.h"
#include "../commands/PluginCommands.h"

namespace bbfx {

namespace {

// Lowercase helper for case-insensitive search.
std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Directory size on disk (in bytes). Best-effort — IO errors counted as 0.
uint64_t dirSize(const std::filesystem::path& p) {
    uint64_t total = 0;
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) return 0;
    for (auto& e : std::filesystem::recursive_directory_iterator(p, ec)) {
        if (ec) break;
        if (e.is_regular_file(ec)) {
            auto sz = e.file_size(ec);
            if (!ec) total += sz;
        }
    }
    return total;
}

std::string humanSize(uint64_t bytes) {
    char buf[32];
    if (bytes < 1024)            std::snprintf(buf, sizeof(buf), "%llu B",  static_cast<unsigned long long>(bytes));
    else if (bytes < 1024*1024)  std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    else if (bytes < 1024ull*1024*1024) std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024));
    else std::snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024 * 1024));
    return buf;
}

const ImVec4& badgeColor(PluginState s) {
    static const ImVec4 kGreen   {0.40f, 0.90f, 0.50f, 1.0f};
    static const ImVec4 kGray    {0.60f, 0.60f, 0.60f, 1.0f};
    static const ImVec4 kRed     {0.95f, 0.45f, 0.45f, 1.0f};
    static const ImVec4 kBlue    {0.55f, 0.75f, 1.00f, 1.0f};
    static const ImVec4 kYellow  {0.95f, 0.85f, 0.30f, 1.0f};
    switch (s) {
        case PluginState::ENABLED:    return kGreen;
        case PluginState::DISABLED:
        case PluginState::UNLOADED:   return kGray;
        case PluginState::FAILED:     return kRed;
        case PluginState::LOADED:     return kBlue;
        case PluginState::VALIDATED:
        case PluginState::DISCOVERED: return kYellow;
    }
    return kGray;
}

void openInExplorer(const std::filesystem::path& p) {
#ifdef _WIN32
    std::string cmd = "explorer \"" + p.string() + "\"";
    std::system(cmd.c_str());
#else
    std::string cmd = "xdg-open \"" + p.string() + "\" >/dev/null 2>&1 &";
    std::system(cmd.c_str());
#endif
}

void openUrlInBrowser(const std::string& url) {
    if (url.empty()) return;
#ifdef _WIN32
    std::string cmd = "start \"\" \"" + url + "\"";
    std::system(cmd.c_str());
#else
    std::string cmd = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
    std::system(cmd.c_str());
#endif
}

bool matchesSearch(const PluginInfo& p, const std::string& query, bool regex) {
    if (query.empty()) return true;
    if (!regex) {
        std::string needle = toLower(query);
        if (toLower(p.manifest.name).find(needle) != std::string::npos) return true;
        if (toLower(p.id).find(needle) != std::string::npos) return true;
        if (toLower(p.manifest.description).find(needle) != std::string::npos) return true;
        for (const auto& tag : p.manifest.tags)
            if (toLower(tag).find(needle) != std::string::npos) return true;
        return false;
    }
    try {
        std::regex r(query, std::regex::icase);
        if (std::regex_search(p.manifest.name, r)) return true;
        if (std::regex_search(p.id, r)) return true;
        if (std::regex_search(p.manifest.description, r)) return true;
    } catch (const std::exception&) {
        return false; // malformed regex — treat as no match
    }
    return false;
}

} // anonymous

void PluginManagerPanel::render(bool* open) {
    if (!open || !*open) return;

    ImGui::SetNextWindowSize({ 720, 480 }, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Plugin Manager##v35", open)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("##pmgrTabs")) {
        if (ImGui::BeginTabItem("Installed")) {
            mActiveTab = 0;
            renderInstalledTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Community")) {
            mActiveTab = 1;
            renderCommunityTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    renderUninstallConfirm();

    ImGui::End();
}

void PluginManagerPanel::renderInstalledTab() {
    auto& pm = PluginManager::instance();
    auto ids = pm.listPlugins();

    // ── Toolbar : search + sort + bulk actions ────────────────────────────
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputTextWithHint("##pmgr_search", "Search (name/id/tags)",
                             mSearchBuf, sizeof(mSearchBuf));
    ImGui::SameLine();
    ImGui::Checkbox("regex", &mUseRegex);
    ImGui::SameLine();

    static const char* kSortLabels[] = { "Name", "Installed", "Last used", "Size" };
    ImGui::SetNextItemWidth(140.0f);
    ImGui::Combo("Sort", &mSortMode, kSortLabels, IM_ARRAYSIZE(kSortLabels));

    ImGui::SameLine();
    if (ImGui::SmallButton("Enable all")) {
        for (const auto& id : ids) {
            const auto* p = pm.getPlugin(id);
            if (p && p->state != PluginState::ENABLED && !p->isBuiltin) {
                CommandManager::instance().execute(
                    std::make_unique<PluginEnableCommand>(id));
            }
        }
        ToastSystem::instance().toast("Enabled all plugins", ToastSeverity::Info, 2.5f);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Disable all")) {
        for (const auto& id : ids) {
            const auto* p = pm.getPlugin(id);
            if (p && p->state == PluginState::ENABLED) {
                CommandManager::instance().execute(
                    std::make_unique<PluginDisableCommand>(id));
            }
        }
        ToastSystem::instance().toast("Disabled all plugins", ToastSeverity::Info, 2.5f);
    }

    ImGui::Separator();

    // ── Filter + sort ─────────────────────────────────────────────────────
    struct Row {
        std::string id;
        const PluginInfo* p = nullptr;
        uint64_t sizeBytes = 0;
    };
    std::vector<Row> rows;
    std::string q(mSearchBuf);
    uint64_t totalSize = 0;
    for (const auto& id : ids) {
        const auto* p = pm.getPlugin(id);
        if (!p) continue;
        if (!matchesSearch(*p, q, mUseRegex)) continue;
        Row r;
        r.id = id;
        r.p = p;
        r.sizeBytes = dirSize(p->directoryPath);
        totalSize += r.sizeBytes;
        rows.push_back(std::move(r));
    }

    auto cmp = [this](const Row& a, const Row& b) {
        switch (mSortMode) {
            case 1: return a.p->installedAt > b.p->installedAt;
            case 2: return a.p->lastUsedAt  > b.p->lastUsedAt;
            case 3: return a.sizeBytes      > b.sizeBytes;
            default: return a.p->manifest.name < b.p->manifest.name;
        }
    };
    std::sort(rows.begin(), rows.end(), cmp);

    if (rows.empty()) {
        ImGui::TextDisabled("No plugins match the current filter.");
    }

    // ── Rows ──────────────────────────────────────────────────────────────
    ImGui::BeginChild("##pmgr_list", {0, -ImGui::GetFrameHeightWithSpacing() * 1.5f});
    for (const auto& row : rows) {
        const auto* p = row.p;
        bool enabled = p->state == PluginState::ENABLED;
        ImGui::PushID(p->id.c_str());

        // Toggle (disabled for FAILED plugins — user must uninstall/fix).
        bool disableCheckbox = p->state == PluginState::FAILED;
        ImGui::BeginDisabled(disableCheckbox);
        if (ImGui::Checkbox("##enabled", &enabled)) {
            if (enabled) {
                CommandManager::instance().execute(
                    std::make_unique<PluginEnableCommand>(p->id));
            } else {
                CommandManager::instance().execute(
                    std::make_unique<PluginDisableCommand>(p->id));
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        // Name + version + state badge.
        ImGui::TextColored(badgeColor(p->state), "%s", toString(p->state));
        ImGui::SameLine();
        ImGui::Text("%s", p->manifest.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("v%s", p->manifest.version.c_str());
        if (p->isBuiltin) {
            ImGui::SameLine();
            ImGui::TextColored({0.55f, 0.75f, 1.0f, 1.0f}, "[builtin]");
        }
        ImGui::SameLine(0.0f, 20.0f);
        ImGui::TextDisabled("%s", humanSize(row.sizeBytes).c_str());

        // Error badge + tooltip
        if (!p->lastError.empty()) {
            ImGui::SameLine();
            ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, "[error]");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", p->lastError.c_str());
        }

        // Inline actions dropdown (... button)
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 28.0f);
        if (ImGui::SmallButton("...")) ImGui::OpenPopup("action_menu");
        if (ImGui::BeginPopup("action_menu")) {
            if (ImGui::MenuItem("Open folder")) {
                openInExplorer(p->directoryPath);
            }
            if (!p->manifest.homepage.empty()) {
                if (ImGui::MenuItem("Homepage")) openUrlInBrowser(p->manifest.homepage);
            }
            if (ImGui::MenuItem("Reload",
                                 nullptr, false,
                                 p->state != PluginState::FAILED && !p->isBuiltin)) {
                // Soft reload: unload + load. Preserves enabled state.
                bool wasEnabled = (p->state == PluginState::ENABLED);
                PluginManager::instance().unload(p->id);
                if (PluginManager::instance().load(p->id) && wasEnabled) {
                    PluginManager::instance().enable(p->id);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Uninstall...", nullptr, false, !p->isBuiltin)) {
                mPendingUninstall = p->id;
                mOpenUninstallPopup = true;
            }
            ImGui::EndPopup();
        }

        // Description row
        if (!p->manifest.description.empty()) {
            ImGui::Indent(28.0f);
            ImGui::TextDisabled("%s", p->manifest.description.c_str());
            ImGui::Unindent(28.0f);
        }

        ImGui::Spacing();
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::TextDisabled("%zu plugins — %s total on disk",
                        rows.size(), humanSize(totalSize).c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("  |  Ctrl+Shift+X to toggle this panel");
}

void PluginManagerPanel::renderCommunityTab() {
    ImGui::TextDisabled("Community Browser");
    ImGui::Separator();
    ImGui::TextWrapped(
        "The full VS Code Marketplace-style community browser lands in Lot H "
        "(I-1357..I-1374). For now you can install plugins by:");
    ImGui::Bullet(); ImGui::Text("Dropping a .zip onto the Studio window");
    ImGui::Bullet(); ImGui::Text("Calling bbfx.plugin.install(path) from the Console");
    ImGui::Bullet(); ImGui::Text("Calling bbfx.plugin.installFromUrl(url, sha256?) from Lua");
}

void PluginManagerPanel::renderUninstallConfirm() {
    if (mOpenUninstallPopup) {
        ImGui::OpenPopup("Confirm uninstall##pmgr_uninstall");
        mOpenUninstallPopup = false;
    }
    if (ImGui::BeginPopupModal("Confirm uninstall##pmgr_uninstall", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Remove \"%s\" ?", mPendingUninstall.c_str());
        ImGui::TextDisabled("This deletes the plugin directory on disk.");
        ImGui::TextDisabled("Plugin data cannot be recovered unless you keep a backup.");
        ImGui::Separator();
        if (ImGui::Button("Cancel", {120, 0})) {
            mPendingUninstall.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove", {120, 0})) {
            std::string id = mPendingUninstall;
            mPendingUninstall.clear();
            ImGui::CloseCurrentPopup();
            if (PluginManager::instance().uninstall(id)) {
                ToastSystem::instance().toast("Uninstalled '" + id + "'",
                                                ToastSeverity::Info, 3.0f);
            } else {
                ToastSystem::instance().toast("Uninstall failed for '" + id + "'",
                                                ToastSeverity::Error, 4.0f);
            }
        }
        ImGui::EndPopup();
    }
}

} // namespace bbfx
