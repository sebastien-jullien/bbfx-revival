#include "CommandPalette.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include <imgui.h>

#include "../../plugin/CommunityIndex.h"
#include "../../plugin/PluginManager.h"
#include "../ToastSystem.h"

namespace bbfx {

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Case-insensitive substring match — good enough for a "fuzzy" palette
// without pulling in an external fuzzy-finder library.
bool contains(const std::string& hay, const std::string& needle) {
    if (needle.empty()) return true;
    return toLower(hay).find(toLower(needle)) != std::string::npos;
}

} // anonymous

CommandPalette::CommandPalette() {
    // D22 — les anciennes entrées statiques "Open Plugin Manager / Community
    // Browser / Plugin Errors" avaient des lambdas vides (« wired by StudioApp »
    // qui ne les surchargeait jamais) → 3 commandes mortes, doublons des
    // "Toggle Plugin Manager / Community Browser / Plugin Errors" enregistrées
    // par StudioApp::registerCommand. Supprimées : la fonctionnalité existe via
    // les commandes Toggle. On ne garde ici que les commandes réellement câblées.
    mStaticCommands.push_back({"Refresh Community Index",
                               "Re-fetch the GitHub index.json",
                               []() {
        CommunityIndex::instance().refresh([](bool) {});
        ToastSystem::instance().toast("Refreshing community index...",
                                        ToastSeverity::Info, 2.0f);
    }});
}

CommandPalette& CommandPalette::instance() {
    static CommandPalette inst;
    return inst;
}

void CommandPalette::open() {
    mSearch[0] = 0;
    mSelected = 0;
    rebuildDynamicCommands();
    mOpen = true;
}

void CommandPalette::close() { mOpen = false; }

void CommandPalette::registerCommand(Command cmd) {
    mStaticCommands.push_back(std::move(cmd));
}

void CommandPalette::rebuildDynamicCommands() {
    mDynamicCommands.clear();

    // Installed plugins: Enable / Disable / Uninstall
    for (const auto& id : PluginManager::instance().listPlugins()) {
        const auto* p = PluginManager::instance().getPlugin(id);
        if (!p) continue;
        std::string name = p->manifest.name.empty() ? id : (p->manifest.name + "  (" + id + ")");

        if (p->state != PluginState::ENABLED) {
            mDynamicCommands.push_back({
                "Enable " + name,
                std::string(toString(p->state)) + " → ENABLED",
                [id]() { PluginManager::instance().enable(id); }
            });
        } else {
            mDynamicCommands.push_back({
                "Disable " + name,
                "ENABLED → DISABLED",
                [id]() { PluginManager::instance().disable(id); }
            });
        }

        if (!p->isBuiltin) {
            mDynamicCommands.push_back({
                "Uninstall " + name,
                "Remove plugin from disk",
                [id]() {
                    if (PluginManager::instance().uninstall(id)) {
                        ToastSystem::instance().toast("Uninstalled " + id,
                                                       ToastSeverity::Info, 3.0f);
                    }
                }
            });
        }
    }

    // Community plugins: Install from index
    for (const auto& e : CommunityIndex::instance().entries()) {
        mDynamicCommands.push_back({
            "Install " + e.name + "  (" + e.id + ")",
            e.description,
            [e]() {
                if (e.downloadUrl.empty()) {
                    ToastSystem::instance().toast(
                        "No downloadUrl for " + e.id,
                        ToastSeverity::Error, 5.0f);
                    return;
                }
                ToastSystem::instance().toast("Downloading " + e.name + "...",
                                               ToastSeverity::Info, 2.5f);
                PluginManager::instance().installFromUrl(e.downloadUrl, e.sha256,
                    [id = e.id](bool ok, std::string installedId, std::string err) {
                        if (!ok) {
                            ToastSystem::instance().toast(
                                "Install failed (" + id + "): " + err,
                                ToastSeverity::Error, 6.0f);
                        } else {
                            ToastSystem::instance().toast(
                                "Installed '" + installedId + "'",
                                ToastSeverity::Info, 4.0f);
                        }
                    });
            }
        });
    }
}

void CommandPalette::draw() {
    if (!mOpen) return;
    static const char* kTitle = "Command Palette##bbfx_palette";
    if (!ImGui::IsPopupOpen(kTitle)) ImGui::OpenPopup(kTitle);

    // Big central modal.
    auto* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                              vp->WorkPos.y + vp->WorkSize.y * 0.35f},
                             ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({ 640, 400 }, ImGuiCond_Always);
    if (ImGui::BeginPopupModal(kTitle, &mOpen,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoTitleBar)) {

        // Build filtered list.
        std::vector<const Command*> visible;
        auto addAll = [&](const std::vector<Command>& cmds) {
            for (const auto& c : cmds) {
                if (contains(c.label, mSearch) || contains(c.detail, mSearch)) {
                    visible.push_back(&c);
                }
            }
        };
        addAll(mStaticCommands);
        addAll(mDynamicCommands);

        ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##cp_search", "> Type a command",
                                 mSearch, sizeof(mSearch));
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) mSelected++;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))   mSelected--;
        if (visible.empty()) mSelected = 0;
        else mSelected = std::clamp(mSelected, 0, int(visible.size()) - 1);

        ImGui::Separator();
        if (ImGui::BeginChild("##cp_list", { 0, -40 })) {
            for (int i = 0; i < int(visible.size()); ++i) {
                const auto* c = visible[i];
                bool sel = (i == mSelected);
                if (ImGui::Selectable(c->label.c_str(), sel,
                                       ImGuiSelectableFlags_AllowDoubleClick)) {
                    mSelected = i;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        if (c->action) c->action();
                        mOpen = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
                if (!c->detail.empty()) {
                    ImGui::SameLine(360.0f);
                    ImGui::TextDisabled("%s", c->detail.c_str());
                }
            }
        }
        ImGui::EndChild();

        // Status row
        ImGui::Separator();
        ImGui::TextDisabled("%zu / %zu commands visible — Enter to run, Esc to close",
                             visible.size(),
                             mStaticCommands.size() + mDynamicCommands.size());

        if (ImGui::IsKeyPressed(ImGuiKey_Enter) ||
            ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
            if (!visible.empty() && visible[mSelected]->action) {
                visible[mSelected]->action();
            }
            mOpen = false;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            mOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace bbfx
