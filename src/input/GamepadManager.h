#pragma once

#include "../bbfx.h"
#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace bbfx {

/// v3.5 Lot J — modern gamepad manager.
///
/// Supersedes the v3.0..v3.4 JoystickManager while preserving the exact
/// same public API for the methods that existed before (getAxisValue,
/// isButtonDown, getCount, init, handleEvent). Any caller that kept using
/// `bbfx::JoystickManager` continues to work: JoystickManager.h now
/// aliases `GamepadManager`.
///
/// New capabilities added in v3.5 Lot J (built on top of SDL3 APIs that
/// were already available but unused by BBFx):
///   - Gamepad type detection (Xbox / PS4 / PS5 / SwitchPro / Generic)
///   - Haptic rumble + trigger rumble
///   - Gyroscope + Accelerometer sampling
///   - Simple per-axis Kalman filter to tame gyro noise
///   - Gyroscope calibration (capture offset at rest)
/// The touchpad, LED, and battery readouts land in Lot K (I-1395..I-1402).
class GamepadManager {
public:
    enum class Type {
        Generic,
        Xbox,
        PS4,
        PS5,
        SwitchPro,
    };
    static const char* typeName(Type t);

    GamepadManager();
    ~GamepadManager();

    void init();
    void handleEvent(const SDL_Event& evt);
    // Poll sensors + advance rumble timeouts + apply Kalman filter.
    // Call once per frame from the engine update loop.
    void update();

    // --- Legacy API (preserved for backward-compat) -----------------------
    float getAxisValue(int index, int axis) const;
    bool  isButtonDown(int index, int button) const;
    int   getCount() const;

    // --- Lot J additions --------------------------------------------------
    Type        getType(int index) const;
    std::string getName(int index) const;

    // Rumble control (durationMs applies to both; 0 = stop)
    bool rumble       (int index, float lowFreq, float highFreq, int durationMs);
    bool rumbleTriggers(int index, float leftTrigger, float rightTrigger, int durationMs);
    void stopRumble   (int index);

    // Sensors (deg/s for gyro, m/s^2 for accel). Return false if the
    // sensor is not available on this device; `outX/Y/Z` are zeroed then.
    bool hasGyro (int index) const;
    bool hasAccel(int index) const;
    bool getGyro (int index, float& outX, float& outY, float& outZ) const;
    bool getAccel(int index, float& outX, float& outY, float& outZ) const;

    // Drop the current gyro reading as the "at rest" zero — all subsequent
    // getGyro calls subtract this offset. Pass index >= 0; no-op otherwise.
    void calibrateGyro(int index);

    // Kalman filter tuning — safe defaults work for most sticks.
    // Larger `measurementNoise` => smoother but laggier; larger
    // `processNoise` => more responsive but jittery.
    void setGyroFilter(float processNoise, float measurementNoise);

    // --- Lot K additions : touchpad / LED / battery ---------------------

    struct TouchFinger {
        bool  down     = false;
        float x        = 0.0f;   // 0..1 normalized across the touchpad
        float y        = 0.0f;   // 0..1
        float pressure = 0.0f;   // 0..1 (when supported)
    };

    enum class PowerState {
        Unknown,
        OnBattery,
        NoBattery,
        Charging,
        Charged,
    };
    static const char* powerStateName(PowerState s);

    bool hasTouchpad(int index) const;
    // Fills `out[0..1]` with the state of the first two fingers on the
    // touchpad. Returns the number of fingers actually populated (0..2).
    int  getTouchpadFingers(int index, TouchFinger out[2]) const;

    bool hasLED(int index) const;
    bool setLED(int index, uint8_t r, uint8_t g, uint8_t b);

    int        getBatteryPercent(int index) const;  // -1 if unknown
    PowerState getBatteryState(int index) const;

private:
    struct KalmanAxis {
        float x = 0.0f;   // state estimate
        float p = 1.0f;   // estimate covariance
        void step(float z, float q, float r) {
            // Predict
            p += q;
            // Update
            float k = p / (p + r);
            x = x + k * (z - x);
            p = (1.0f - k) * p;
        }
        void reset() { x = 0.0f; p = 1.0f; }
    };

    struct Entry {
        SDL_JoystickID id;
        SDL_Gamepad*   gamepad  = nullptr;   // non-null if SDL gamepad mapping exists
        SDL_Joystick*  joystick = nullptr;   // non-null for raw joystick fallback
        Type           type     = Type::Generic;
        std::string    name;

        bool hasGyro  = false;
        bool hasAccel = false;
        float gyroOffsetX = 0.0f, gyroOffsetY = 0.0f, gyroOffsetZ = 0.0f;
        float gyroX = 0.0f, gyroY = 0.0f, gyroZ = 0.0f;     // filtered deg/s
        float accelX = 0.0f, accelY = 0.0f, accelZ = 0.0f;  // raw m/s^2
        KalmanAxis kalmanX, kalmanY, kalmanZ;

        uint64_t rumbleStopMs        = 0;    // SDL_GetTicks time at which we need to stop
        uint64_t triggerRumbleStopMs = 0;

        // Lot K — touchpad / LED / battery
        bool         hasTouchpad = false;
        TouchFinger  touchpad[2];
        bool         hasLED      = false;
        uint8_t      ledR = 0, ledG = 0, ledB = 0;
        int          batteryPercent = -1;
        PowerState   batteryState   = PowerState::Unknown;
        uint64_t     batteryPollMs  = 0;  // throttle to every 2s
    };

    const Entry* entry(int index) const;
    Entry*       entryMut(int index);

    std::vector<Entry> mEntries;

    // Kalman noise model (same for all axes and all gamepads).
    float mProcessNoise     = 0.01f;
    float mMeasurementNoise = 0.5f;
};

} // namespace bbfx
