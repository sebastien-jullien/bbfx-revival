#pragma once

#include <functional>
#include <string>

#include "../../plugin/PluginManifest.h"

namespace bbfx {

/// v3.5 Lot F — Permission prompt modal.
///
/// Used by the install pipeline right before a plugin is activated the
/// first time. Displays the plugin's name, author, description and its
/// requested permissions (with a human-readable icon + description), then
/// blocks on an Accept/Cancel decision.
///
/// Usage (from StudioApp::renderFrame, after ImGui NewFrame):
///   PermissionPromptDialog::instance().draw();
///
/// And from the install pipeline:
///   PermissionPromptDialog::instance().open(manifest,
///       [id]() { ... on accept ... },
///       []   () { ... on cancel ... });
class PermissionPromptDialog {
public:
    static PermissionPromptDialog& instance();

    /// Queue a prompt for this manifest. Callbacks run on the main thread
    /// (the same thread that calls draw()).
    void open(const PluginManifest& manifest,
              std::function<void()> onAccept,
              std::function<void()> onCancel);

    /// Call from ImGui frame. Noop when no prompt is pending.
    void draw();

    bool isOpen() const { return mPending; }

private:
    PermissionPromptDialog() = default;

    bool mPending = false;
    PluginManifest mManifest;
    std::function<void()> mOnAccept;
    std::function<void()> mOnCancel;
};

/// Human-friendly mapping for a PluginPermission.
struct PermissionDescription {
    const char* icon;        // single unicode glyph, UTF-8
    const char* label;       // e.g. "Audio analysis"
    const char* description; // e.g. "Access FFT, BPM, beat detection"
};
const PermissionDescription& describePermission(PluginPermission p);

} // namespace bbfx
