#pragma once
#include "../StudioEngine.h"
#include <cstdint>

namespace bbfx {

/// Displays the OGRE RenderTexture inside a dockable ImGui panel.
/// Handles dynamic resize: when the panel is resized, the RenderTexture is recreated.
class ViewportPanel {
public:
    explicit ViewportPanel(StudioEngine* engine);

    /// Called once per frame before ImGui rendering: syncs size + triggers OGRE render.
    void updateOgreRender();

    /// Called inside the ImGui frame to draw the panel.
    void render();

private:
    void syncSize();

    StudioEngine* mEngine;
    uint32_t mLastWidth  = 0;
    uint32_t mLastHeight = 0;
    uint32_t mPendingW   = 0;
    uint32_t mPendingH   = 0;
    bool mShowOverlay = true;
};

} // namespace bbfx
