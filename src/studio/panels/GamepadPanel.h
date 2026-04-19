#pragma once

#include <array>
#include <functional>
#include <string>

namespace bbfx {

class GamepadManager;

/// v3.5 Lot L — Gamepad visualisation / calibration / learn panel.
///
/// One dockable window that surfaces everything the Lot J/K backend can
/// report: per-gamepad header (name + type + battery), sticks, triggers,
/// buttons, a small 3D gyroscope cube (rotated live from accumulated
/// gyro samples), the touchpad pad with two fingers drawn at their
/// actual position and pressure-scaled radius, LED colour picker,
/// rumble/trigger-rumble/LED test buttons, gyro calibration flow, and
/// a "learn" mode that watches all inputs for the biggest delta and
/// hands the winner off to the NodeEditor to pick a port.
///
/// Opened from menu Input > Gamepad... (Ctrl+Shift+G).
class GamepadPanel {
public:
    GamepadPanel();
    ~GamepadPanel();

    void render(bool* open);

    // Learn mode — wired by StudioApp to the NodeEditor so that when the
    // panel detects a strong input it can ask the NodeEditor to pick a
    // destination port. The callback receives the source token (same
    // vocabulary as GamepadMappingProfile: gyroX, leftStickX, touchpad0X,
    // buttonA, ...). When the user picks a target port downstream, the
    // panel is done and exits learn mode automatically.
    using LearnCallback = std::function<void(const std::string& source)>;
    void setLearnCallback(LearnCallback cb) { mLearn = std::move(cb); }

private:
    // Selected gamepad (index into GamepadManager; -1 = none).
    int mIndex = 0;

    // Accumulated rotation from gyro samples (degrees), decayed softly.
    float mGyroAccumX = 0.0f;
    float mGyroAccumY = 0.0f;
    float mGyroAccumZ = 0.0f;
    double mLastUpdateSec = 0.0;

    // LED picker state (0..1 linear RGB).
    float mLedRgb[3] = { 1.0f, 0.0f, 0.5f };

    // Calibration: 3s countdown before freezing gyro offset.
    bool   mCalibrating     = false;
    double mCalibStartSec   = 0.0;

    // LED cycle test (rotates hue over ~3s once armed).
    bool   mLedCycling      = false;
    double mLedCycleStart   = 0.0;

    // Learn state.
    bool   mLearning           = false;
    double mLearnStartSec      = 0.0;
    // Snapshot of axes/buttons at learn start — we compare deltas each
    // frame and pick the single biggest one once the user confirms.
    struct LearnSnapshot {
        std::array<float, 6> axes   = {};   // left X/Y, right X/Y, triggers
        std::array<bool, 15> buttons = {};
        float gyroX = 0.0f, gyroY = 0.0f, gyroZ = 0.0f;
        float touch0X = 0.0f, touch0Y = 0.0f;
    };
    LearnSnapshot mLearnStart;
    std::string   mLearnBestSource;
    float         mLearnBestDelta = 0.0f;

    LearnCallback mLearn;

    // Renderers ------------------------------------------------------
    void renderHeader(GamepadManager& gm);
    void renderSticks(GamepadManager& gm);
    void renderTriggers(GamepadManager& gm);
    void renderButtons(GamepadManager& gm);
    void renderGyroSphere(GamepadManager& gm, double dt);
    void renderTouchpad(GamepadManager& gm);
    void renderLED(GamepadManager& gm, double now);
    void renderTestButtons(GamepadManager& gm, double now);
    void renderCalibration(GamepadManager& gm, double now);
    void renderLearn(GamepadManager& gm);

    // Utility.
    void snapshotLearnStart(GamepadManager& gm);
    void updateLearnBest(GamepadManager& gm);
};

} // namespace bbfx
