#pragma once
#include <imgui.h>

namespace bbfx {
class OutputManager;
class StudioEngine;
class StudioApp;

/// Multi-output manager panel (v3.4 — replaces OutputPanel).
/// Shows a list of output windows with per-output controls:
/// resolution, monitor, fullscreen, preview thumbnail, warp, blend.
/// Also hosts the WarpWizard calibration UI (Lot D).
class OutputManagerPanel {
public:
    void render(StudioEngine* engine, StudioApp* app = nullptr);
};

} // namespace bbfx
