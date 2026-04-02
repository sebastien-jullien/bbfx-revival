#pragma once
#include "../StudioEngine.h"
#include "../viewport/ViewportCameraController.h"
#include "../viewport/ViewportPicker.h"
#include "../viewport/ViewportGrid.h"
#include "../viewport/ViewportGizmo.h"
#include "../viewport/ViewportToolbar.h"
#include <cstdint>
#include <memory>

namespace bbfx {

/// Displays the OGRE RenderTexture inside a dockable ImGui panel.
/// Handles dynamic resize, camera interaction, picking, gizmos, grid.
class ViewportPanel {
public:
    explicit ViewportPanel(StudioEngine* engine);

    /// Called once per frame before ImGui rendering: syncs size + triggers OGRE render.
    void updateOgreRender();

    /// Called inside the ImGui frame to draw the panel.
    void render();

    /// Force RenderTexture resize on next render (call after Performance Mode exit).
    void invalidateSize() { mLastWidth = 0; mLastHeight = 0; }

    /// Access to the camera controller for external actions (focus, reset, etc.).
    ViewportCameraController* getCameraController() { return mCameraController.get(); }

    /// Access to the picker for external selection sync.
    ViewportPicker* getPicker() { return mPicker.get(); }

    /// Access to the gizmo for tool switching from StudioApp.
    ViewportGizmo* getGizmo() { return mGizmo.get(); }

    /// Access to the toolbar.
    ViewportToolbar& getToolbar() { return mToolbar; }

    // Keyboard is now polled directly via SDL_GetKeyboardState in FPS mode.

    /// Check if viewport is currently hovered by mouse.
    bool isHovered() const { return mIsHovered; }

    /// Check if FPS camera mode is active (RMB held, cursor locked).
    bool isFpsCaptured() const { return mFpsCaptured; }

private:
    void syncSize();

    StudioEngine* mEngine;
    uint32_t mLastWidth  = 0;
    uint32_t mLastHeight = 0;
    uint32_t mPendingW   = 0;
    uint32_t mPendingH   = 0;
    bool mShowOverlay = true;
    bool mIsHovered      = false;
    bool mRightMouseDown = false;
    bool mFpsCaptured    = false;  // true while RMB-held FPS mode with cursor locked
    bool mFpsFirstFrame  = false; // skip first delta after entering FPS mode
    bool mWasLockOn      = false; // track CTRL lock-on state transitions
    float mSavedMouseX   = 0;     // cursor position saved on RMB press
    float mSavedMouseY   = 0;

    // Sub-systems
    std::unique_ptr<ViewportCameraController> mCameraController;
    std::unique_ptr<ViewportPicker> mPicker;
    std::unique_ptr<ViewportGrid> mGrid;
    std::unique_ptr<ViewportGizmo> mGizmo;
    ViewportToolbar mToolbar;

};

} // namespace bbfx
