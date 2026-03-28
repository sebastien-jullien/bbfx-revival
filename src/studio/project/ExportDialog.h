#pragma once
#include <string>

namespace bbfx { class StudioEngine; }

namespace bbfx {

/// Modal dialog for exporting the current session as a PNG frame sequence.
/// Orchestrates: setOfflineMode → InputPlayer → VideoExporter → progress bar → setOnlineMode.
class ExportDialog {
public:
    ExportDialog() = default;

    /// Open the dialog (ImGui::OpenPopup is called internally).
    void open();

    /// Render the dialog (must be called every frame from StudioApp).
    void render(StudioEngine* engine);

    bool isVisible() const { return mOpen; }

private:
    bool startExport(StudioEngine* engine);
    void tickExport(StudioEngine* engine);
    void finishExport(StudioEngine* engine);

    bool mOpen = false;

    // Export settings
    int   mWidth    = 1920;
    int   mHeight   = 1080;
    int   mFPS      = 60;
    int   mDuration = 30; // seconds
    char  mOutputDir[512] = "output/frames";
    char  mSessionFile[512] = "";

    // Export state
    bool   mExporting = false;
    int    mFramesCaptured = 0;
    int    mTotalFrames = 0;
};

} // namespace bbfx
