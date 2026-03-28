#pragma once
#include <string>

namespace bbfx { class StudioEngine; }

namespace bbfx {

/// Performance Mode (F5): fullscreen viewport, trigger grid, faders, VU meters, BPM overlay, panic.
class PerformanceModePanel {
public:
    PerformanceModePanel() = default;

    void render(StudioEngine* engine);

private:
    void renderTriggerGrid();
    void renderFaders();
    void renderVUMeters();
    void renderBPMOverlay();
    void renderPanicButton();

    struct FaderSlot {
        std::string nodeName;
        std::string portName;
        float value = 0.0f;
    };
    FaderSlot mFaders[8];

    float mBPM = 120.0f;
    float mRMS = 0.0f;
    float mBands[3] = {}; // low, mid, high
};

} // namespace bbfx
