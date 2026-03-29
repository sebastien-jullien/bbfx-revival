#include "ExportDialog.h"
#include "../StudioEngine.h"
#include "../../record/InputPlayer.h"
#include "../../record/VideoExporter.h"

#include <imgui.h>
#include <iostream>
#include <filesystem>

namespace bbfx {

void ExportDialog::open() {
    mOpen = true;
}

void ExportDialog::render(StudioEngine* engine) {
    if (!mOpen) return;

    // OpenPopup must be called during ImGui frame, not from event handlers
    if (!ImGui::IsPopupOpen("Export Session")) {
        ImGui::OpenPopup("Export Session");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({480, 0}, ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Export Session", &mOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!mExporting) {
            // ── Settings ─────────────────────────────────────────────────────
            ImGui::InputText("Output Directory", mOutputDir, sizeof(mOutputDir));
            ImGui::InputText("Session File (.bbfx-session)", mSessionFile, sizeof(mSessionFile));
            ImGui::InputInt("Width",    &mWidth,  16, 320);
            ImGui::InputInt("Height",   &mHeight, 16, 180);
            ImGui::InputInt("FPS",      &mFPS,     1,  10);
            ImGui::InputInt("Duration (s)", &mDuration, 1, 5);

            mWidth    = std::max(  64, std::min(7680, mWidth));
            mHeight   = std::max(  64, std::min(4320, mHeight));
            mFPS      = std::max(   1, std::min( 120, mFPS));
            mDuration = std::max(   1, std::min(3600, mDuration));

            ImGui::Separator();
            if (ImGui::Button("Start Export", {200, 0})) {
                startExport(engine);
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", {100, 0})) {
                mOpen = false;
                ImGui::CloseCurrentPopup();
            }
        } else {
            // ── Progress ─────────────────────────────────────────────────────
            tickExport(engine);

            float progress = mTotalFrames > 0
                ? static_cast<float>(mFramesCaptured) / mTotalFrames
                : 0.0f;

            ImGui::Text("Exporting to: %s", mOutputDir);
            char progLabel[64];
            snprintf(progLabel, sizeof(progLabel), "Frame %d / %d",
                     mFramesCaptured, mTotalFrames);
            ImGui::ProgressBar(progress, {-1, 0}, progLabel);

            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Button, {0.5f, 0.0f, 0.0f, 1.0f});
            if (ImGui::Button("Cancel Export", {150, 0})) {
                finishExport(engine);
                mOpen = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor();

            if (mFramesCaptured >= mTotalFrames && mTotalFrames > 0) {
                finishExport(engine);
            }
        }

        ImGui::EndPopup();
    }
}

bool ExportDialog::startExport(StudioEngine* engine) {
    if (!engine) return false;

    mTotalFrames    = mFPS * mDuration;
    mFramesCaptured = 0;
    mExporting      = true;

    // Resize render target to export resolution
    engine->resizeRenderTexture(static_cast<uint32_t>(mWidth),
                                 static_cast<uint32_t>(mHeight));
    engine->setOfflineMode(mFPS);

    std::filesystem::create_directories(mOutputDir);

    std::cout << "[ExportDialog] Starting export: "
              << mWidth << "×" << mHeight << " @" << mFPS << "fps → " << mOutputDir << std::endl;
    return true;
}

void ExportDialog::tickExport(StudioEngine* engine) {
    if (!mExporting || !engine) return;

    // Render one offline frame
    engine->updateRenderTarget();

    // Capture the frame
    char filename[1024];
    snprintf(filename, sizeof(filename), "%s/frame_%06d.png",
             mOutputDir, mFramesCaptured + 1);

    engine->captureFrame(std::string(filename));

    ++mFramesCaptured;
}

void ExportDialog::finishExport(StudioEngine* engine) {
    mExporting = false;
    if (engine) {
        engine->setOnlineMode();
        // Restore viewport to a reasonable default resolution
        // (ViewportPanel will resize to actual window size on next render)
        engine->resizeRenderTexture(1280, 720);
    }
    std::cout << "[ExportDialog] Export complete: " << mFramesCaptured << " frames → "
              << mOutputDir << "/" << std::endl;
    std::cout << "  ffmpeg -framerate " << mFPS << " -i "
              << mOutputDir << "/frame_%06d.png -c:v libx264 -pix_fmt yuv420p out.mp4" << std::endl;
}

} // namespace bbfx
