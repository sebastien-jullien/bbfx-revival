#include "studio/dialogs/PermissionPromptDialog.h"

#include <imgui.h>

namespace bbfx {

namespace {

const PermissionDescription& descTable(PluginPermission p) {
    // Icons are plain ASCII prefixes rather than emoji to avoid font
    // glyph availability issues on the default ImGui font.
    static const PermissionDescription kUnknown{
        "?", "Unknown", "Unknown permission"};
    switch (p) {
        case PluginPermission::AUDIO: {
            static const PermissionDescription d{
                "[A]", "Audio analysis",
                "Access FFT, BPM detection, beat/onset events"};
            return d;
        }
        case PluginPermission::MIDI: {
            static const PermissionDescription d{
                "[M]", "MIDI in/out",
                "Read CCs & notes, send CC/notes, MIDI Learn"};
            return d;
        }
        case PluginPermission::OSC: {
            static const PermissionDescription d{
                "[O]", "OSC in/out",
                "Send and receive OSC messages over UDP"};
            return d;
        }
        case PluginPermission::ARTNET: {
            static const PermissionDescription d{
                "[D]", "DMX / Artnet",
                "Send and receive DMX over Art-Net (port 6454)"};
            return d;
        }
        case PluginPermission::TEXTURE_SHARE: {
            static const PermissionDescription d{
                "[T]", "Texture share (Spout/NDI)",
                "Publish or receive GPU textures across applications"};
            return d;
        }
        case PluginPermission::NETWORK: {
            static const PermissionDescription d{
                "[N]", "Network",
                "HTTP requests and WebSocket connections (outbound)"};
            return d;
        }
        case PluginPermission::FS: {
            static const PermissionDescription d{
                "[F]", "Filesystem (scoped)",
                "Read/write files under this plugin's directory only"};
            return d;
        }
        case PluginPermission::UI: {
            static const PermissionDescription d{
                "[U]", "Custom UI",
                "Register ImGui panels and custom Inspector widgets"};
            return d;
        }
        case PluginPermission::GAMEPAD: {
            static const PermissionDescription d{
                "[G]", "Gamepad",
                "Read gamepad sticks, triggers, gyro, touchpad, rumble"};
            return d;
        }
        case PluginPermission::MODELS: {
            static const PermissionDescription d{
                "[3]", "3D models",
                "Import 3D models at runtime (OBJ/FBX/glTF)"};
            return d;
        }
        case PluginPermission::SCENE: {
            static const PermissionDescription d{
                "[S]", "Scene",
                "Control camera, lights, fog, skybox, render textures"};
            return d;
        }
    }
    return kUnknown;
}

} // anonymous

const PermissionDescription& describePermission(PluginPermission p) {
    return descTable(p);
}

PermissionPromptDialog& PermissionPromptDialog::instance() {
    static PermissionPromptDialog inst;
    return inst;
}

void PermissionPromptDialog::open(const PluginManifest& manifest,
                                   std::function<void()> onAccept,
                                   std::function<void()> onCancel) {
    mManifest = manifest;
    mOnAccept = std::move(onAccept);
    mOnCancel = std::move(onCancel);
    mPending = true;
}

void PermissionPromptDialog::draw() {
    if (!mPending) return;
    static const char* kTitle = "Install plugin##bbfx_perm_prompt";

    if (!ImGui::IsPopupOpen(kTitle)) {
        ImGui::OpenPopup(kTitle);
    }

    ImGui::SetNextWindowSize({ 520, 0 }, ImGuiCond_Always);
    if (ImGui::BeginPopupModal(kTitle, nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("Install \"%s\" %s by %s?",
                    mManifest.name.c_str(),
                    mManifest.version.c_str(),
                    mManifest.author.name.c_str());
        if (!mManifest.description.empty()) {
            ImGui::TextWrapped("%s", mManifest.description.c_str());
        }
        ImGui::Separator();

        if (mManifest.permissions.empty()) {
            ImGui::TextDisabled("No special permissions requested.");
        } else {
            ImGui::TextDisabled("Requested permissions:");
            for (auto p : mManifest.permissions) {
                const auto& d = describePermission(p);
                ImGui::BulletText("%s  %s", d.icon, d.label);
                ImGui::Indent();
                ImGui::TextDisabled("%s", d.description);
                ImGui::Unindent();
            }
        }
        ImGui::Separator();
        ImGui::TextDisabled("The plugin will run inside a sandbox — it cannot access");
        ImGui::TextDisabled("system files or execute programs regardless of your choice.");
        ImGui::Separator();

        const float buttonW = 120.0f;
        ImGui::Dummy({ImGui::GetContentRegionAvail().x - 2 * buttonW - 8, 4});
        ImGui::SameLine();
        if (ImGui::Button("Cancel", { buttonW, 0 })) {
            auto cb = std::move(mOnCancel);
            mOnAccept = {};
            mPending = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            if (cb) cb();
            return;
        }
        ImGui::SameLine();
        if (ImGui::Button("Install", { buttonW, 0 })) {
            auto cb = std::move(mOnAccept);
            mOnCancel = {};
            mPending = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            if (cb) cb();
            return;
        }

        ImGui::EndPopup();
    }
}

} // namespace bbfx
