#include "PluginAuthoringDialog.h"

#include <cstring>

#include <imgui.h>

#include "../../plugin/PluginAuthoringBackend.h"
#include "../../plugin/PluginManager.h"
#include "../ToastSystem.h"

namespace bbfx {

PluginAuthoringDialog::PluginAuthoringDialog() {
    std::snprintf(mAuthor, sizeof(mAuthor), "BBFx Studio user");
}

void PluginAuthoringDialog::open(Mode m) {
    mMode = m;
    ImGui::OpenPopup("Plugin Authoring##pauth");
}

void PluginAuthoringDialog::render(bool* keepOpen) {
    if (!keepOpen) return;
    if (!ImGui::IsPopupOpen("Plugin Authoring##pauth")) return;
    ImGui::SetNextWindowSize({640, 480}, ImGuiCond_FirstUseEver);
    if (!ImGui::BeginPopupModal("Plugin Authoring##pauth", keepOpen)) return;

    const char* modeLabel =
        (mMode == Mode::Subgraph)        ? "Export Subgraph as Plugin" :
        (mMode == Mode::Scene)           ? "Export Scene Preset Plugin" :
                                            "Export Output Template Plugin";
    ImGui::TextColored({0.3f, 0.9f, 1.0f, 1.0f}, "%s", modeLabel);
    ImGui::Separator();

    ImGui::Text("Metadata");
    ImGui::InputText("Name",        mName,        sizeof(mName));
    ImGui::InputText("Id (slug)",   mId,          sizeof(mId));
    if (ImGui::Button("Slugify from name")) {
        auto s = PluginAuthoringBackend::slugify(mName);
        std::snprintf(mId, sizeof(mId), "%s", s.c_str());
    }
    ImGui::InputText("Version",     mVersion,     sizeof(mVersion));
    ImGui::InputText("Author",      mAuthor,      sizeof(mAuthor));
    ImGui::InputTextMultiline("Description",
        mDescription, sizeof(mDescription), {0, 60});
    ImGui::InputText("License",     mLicense,     sizeof(mLicense));

    ImGui::Separator();
    ImGui::TextDisabled("Resources will be auto-detected when the full "
                         "graph export pipeline lands — current build "
                         "produces a valid plugin stub with metadata + init.lua.");

    ImGui::Separator();
    if (ImGui::Button("Generate")) generate();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        *keepOpen = false;
        ImGui::CloseCurrentPopup();
    }

    if (!mLastResult.empty()) {
        ImGui::Separator();
        ImGui::TextColored({0.4f, 1.0f, 0.4f, 1.0f},
                             "Plugin written: %s", mLastResult.c_str());
    }
    if (!mLastError.empty()) {
        ImGui::Separator();
        ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f},
                             "Failed: %s", mLastError.c_str());
    }

    ImGui::EndPopup();
}

void PluginAuthoringDialog::generate() {
    mLastResult.clear();
    mLastError.clear();

    PluginAuthoringBackend::Metadata meta;
    meta.id          = mId;
    meta.name        = mName;
    meta.version     = mVersion;
    meta.author      = mAuthor;
    meta.description = mDescription;
    meta.license     = mLicense;

    if (!PluginAuthoringBackend::isValidId(meta.id)) {
        mLastError = "Invalid id '" + meta.id +
                      "' — use lowercase letters, digits, '.' or '-'.";
        return;
    }

    std::filesystem::path out;
    switch (mMode) {
        case Mode::Subgraph: {
            nlohmann::json spec = { {"nodes", nlohmann::json::array()},
                                      {"links", nlohmann::json::array()} };
            out = PluginAuthoringBackend::exportSubgraph(meta, spec);
            break;
        }
        case Mode::Scene: {
            // Placeholder capture — StudioApp wires the real scene JSON.
            nlohmann::json scene = {};
            out = PluginAuthoringBackend::exportScenePreset(meta, scene);
            break;
        }
        case Mode::OutputTemplate: {
            nlohmann::json outputs = nlohmann::json::array();
            out = PluginAuthoringBackend::exportOutputTemplate(meta, outputs);
            break;
        }
    }
    if (!out.empty()) {
        mLastResult = out.string();
        ToastSystem::instance().toast(
            "Plugin exported: " + meta.id, ToastSeverity::Info, 5.0f);
    } else {
        mLastError = "writePlugin returned an empty path (see console for details).";
    }
}

} // namespace bbfx
