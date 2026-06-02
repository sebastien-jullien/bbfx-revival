#pragma once

#include <imgui.h>
#include <string>

namespace bbfx {

class SurfaceMap;
class OutputManager;
class StudioEngine;
class PerformanceModePanel;
struct Zone;
struct OutputSlot;

/// 2D surface zone editor (v3.4 Lot E).
///
/// Shows a top-view canvas with the physical projection surface.
/// Zones are displayed as coloured rectangles and can be dragged, resized,
/// and assigned to output slots.
class SurfaceEditorPanel {
public:
    SurfaceEditorPanel() = default;
    ~SurfaceEditorPanel();

    void setSurfaceMap(SurfaceMap* sm)          { mSurfaceMap = sm; }
    void setOutputManager(OutputManager* mgr)   { mOutputManager = mgr; }
    void setPerformanceModePanel(PerformanceModePanel* p) { mPerfPanel = p; }

    /// Render the panel.  Call every frame when the panel is visible.
    void render(StudioEngine* engine);

    /// D21 — pousse les overrides warp/blend d'une zone vers le slot de sortie.
    /// Extrait pour testabilité : le rendu lit `slot.warpProfile`/`slot.blendProfile`,
    /// donc l'override par-zone doit être recopié dans le slot assigné.
    static void applyZoneOverridesToSlot(const Zone& zone, OutputSlot& slot);

private:
    // ── Canvas helpers ───────────────────────────────────────────────────────
    void renderCanvas(ImVec2 canvasPos, ImVec2 canvasSize);
    void renderToolbar(StudioEngine* engine);
    void renderProperties(StudioEngine* engine);

    // ── Zone interaction ─────────────────────────────────────────────────────
    /// Returns zone id at canvas-local position, -1 if none.
    int zoneAtPos(float cx, float cy) const;
    /// Converts canvas pixel offset to normalised zone coords.
    ImVec2 canvasToNorm(ImVec2 canvasPos, ImVec2 canvasSize, ImVec2 screenPos) const;

    // ── State ────────────────────────────────────────────────────────────────
    SurfaceMap*             mSurfaceMap    = nullptr;
    OutputManager*          mOutputManager = nullptr;
    PerformanceModePanel*   mPerfPanel     = nullptr;

    int  mSelectedZoneId = -1;
    bool mDragging       = false;
    bool mResizing       = false;
    int  mResizeHandle   = -1; ///< 0-7: TL,T,TR,R,BR,B,BL,L

    /// Drag/resize origin (normalised).
    float mDragOriginX = 0.f, mDragOriginY = 0.f;
    float mDragZoneX   = 0.f, mDragZoneY   = 0.f;
    float mDragZoneW   = 0.f, mDragZoneH   = 0.f;

    // Background image (optional preview).
    std::string mLoadedBgPath;
    ImTextureID  mBgTexture = 0;

    // Zone colours (cycle).
    static const ImVec4 kZoneColors[];
    static const int    kNumZoneColors;
};

} // namespace bbfx
