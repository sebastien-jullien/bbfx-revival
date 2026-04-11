#pragma once
#include <imgui.h>

namespace bbfx {
class StudioEngine;

/// Control panel for the secondary output window (projector/dual screen).
class OutputPanel {
public:
    void render(StudioEngine* engine);
};

} // namespace bbfx
