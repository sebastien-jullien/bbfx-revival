#pragma once

namespace bbfx {

/// Toolbar rendered at the top of the viewport panel.
/// Contains tool selection (translate/rotate/scale), toggles (snap, local/world, grid).
class ViewportToolbar {
public:
    enum class Tool { Translate, Rotate, Scale };
    enum class Space { World, Local };

    ViewportToolbar() = default;

    /// Render the toolbar. Call inside the viewport ImGui window, before the image.
    void render();

    Tool  getTool()      const { return mTool; }
    Space getSpace()     const { return mSpace; }
    bool  isSnapOn()     const { return mSnapOn; }
    float getSnapSize()  const { return mSnapSize; }
    bool  isGridOn()     const { return mGridOn; }
    bool  areOverlaysOn() const { return mOverlaysOn; }

    void setTool(Tool t)  { mTool = t; }
    void setSpace(Space s) { mSpace = s; }
    void toggleGrid()     { mGridOn = !mGridOn; }
    void toggleOverlays() { mOverlaysOn = !mOverlaysOn; }

private:
    Tool  mTool      = Tool::Translate;
    Space mSpace     = Space::World;
    bool  mSnapOn    = false;
    float mSnapSize  = 1.0f;
    bool  mGridOn    = true;
    bool  mOverlaysOn = true;
};

} // namespace bbfx
