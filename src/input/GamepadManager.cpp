#include "GamepadManager.h"

#include <algorithm>

namespace bbfx {

const char* GamepadManager::typeName(Type t) {
    switch (t) {
        case Type::Xbox:      return "Xbox";
        case Type::PS4:       return "PS4";
        case Type::PS5:       return "PS5";
        case Type::SwitchPro: return "SwitchPro";
        case Type::Generic:   default: return "Generic";
    }
}

const char* GamepadManager::powerStateName(PowerState s) {
    switch (s) {
        case PowerState::OnBattery: return "OnBattery";
        case PowerState::NoBattery: return "NoBattery";
        case PowerState::Charging:  return "Charging";
        case PowerState::Charged:   return "Charged";
        case PowerState::Unknown:   default: return "Unknown";
    }
}

static GamepadManager::Type classify(SDL_Gamepad* gp) {
    if (!gp) return GamepadManager::Type::Generic;
    SDL_GamepadType t = SDL_GetGamepadType(gp);
    switch (t) {
        case SDL_GAMEPAD_TYPE_XBOX360:
        case SDL_GAMEPAD_TYPE_XBOXONE:
            return GamepadManager::Type::Xbox;
        case SDL_GAMEPAD_TYPE_PS3:
        case SDL_GAMEPAD_TYPE_PS4:
            return GamepadManager::Type::PS4;
        case SDL_GAMEPAD_TYPE_PS5:
            return GamepadManager::Type::PS5;
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
            return GamepadManager::Type::SwitchPro;
        default:
            return GamepadManager::Type::Generic;
    }
}

GamepadManager::GamepadManager() = default;

GamepadManager::~GamepadManager() {
    for (auto& e : mEntries) {
        if (e.gamepad)  SDL_CloseGamepad(e.gamepad);
        if (e.joystick) SDL_CloseJoystick(e.joystick);
    }
}

void GamepadManager::init() {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    if (!ids) return;
    for (int i = 0; i < count; ++i) {
        SDL_JoystickID id = ids[i];
        Entry entry;
        entry.id = id;
        if (SDL_IsGamepad(id)) {
            entry.gamepad = SDL_OpenGamepad(id);
            if (!entry.gamepad) continue;
            entry.type = classify(entry.gamepad);
            const char* n = SDL_GetGamepadName(entry.gamepad);
            entry.name = n ? n : "Gamepad";

            // Attempt to enable the sensors — silently succeed/fail.
            if (SDL_SetGamepadSensorEnabled(entry.gamepad, SDL_SENSOR_GYRO, true)) {
                entry.hasGyro = true;
            }
            if (SDL_SetGamepadSensorEnabled(entry.gamepad, SDL_SENSOR_ACCEL, true)) {
                entry.hasAccel = true;
            }

            // Lot K — detect touchpad + LED capabilities.
            entry.hasTouchpad = SDL_GetNumGamepadTouchpads(entry.gamepad) > 0;
            entry.hasLED      = SDL_GetBooleanProperty(
                SDL_GetGamepadProperties(entry.gamepad),
                SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false);

            SDL_Log("[GamepadManager] Opened %s (%s) id=%u gyro=%d accel=%d "
                    "touchpad=%d led=%d",
                    entry.name.c_str(), typeName(entry.type), id,
                    entry.hasGyro, entry.hasAccel,
                    entry.hasTouchpad, entry.hasLED);
        } else {
            entry.joystick = SDL_OpenJoystick(id);
            if (!entry.joystick) continue;
            entry.type = Type::Generic;
            const char* n = SDL_GetJoystickName(entry.joystick);
            entry.name = n ? n : "Joystick";
            SDL_Log("[GamepadManager] Opened raw joystick '%s' id=%u",
                    entry.name.c_str(), id);
        }
        mEntries.push_back(std::move(entry));
    }
    SDL_free(ids);
}

void GamepadManager::handleEvent(const SDL_Event& evt) {
    if (evt.type == SDL_EVENT_GAMEPAD_ADDED) {
        SDL_Gamepad* gp = SDL_OpenGamepad(evt.gdevice.which);
        if (gp) {
            Entry e;
            e.id = evt.gdevice.which;
            e.gamepad = gp;
            e.type = classify(gp);
            const char* n = SDL_GetGamepadName(gp);
            e.name = n ? n : "Gamepad";
            if (SDL_SetGamepadSensorEnabled(gp, SDL_SENSOR_GYRO,  true)) e.hasGyro  = true;
            if (SDL_SetGamepadSensorEnabled(gp, SDL_SENSOR_ACCEL, true)) e.hasAccel = true;
            e.hasTouchpad = SDL_GetNumGamepadTouchpads(gp) > 0;
            e.hasLED      = SDL_GetBooleanProperty(
                SDL_GetGamepadProperties(gp),
                SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false);
            mEntries.push_back(std::move(e));
        }
    } else if (evt.type == SDL_EVENT_GAMEPAD_REMOVED) {
        for (auto it = mEntries.begin(); it != mEntries.end(); ++it) {
            if (it->id == evt.gdevice.which && it->gamepad) {
                SDL_CloseGamepad(it->gamepad);
                mEntries.erase(it);
                break;
            }
        }
    } else if (evt.type == SDL_EVENT_JOYSTICK_ADDED) {
        if (!SDL_IsGamepad(evt.jdevice.which)) {
            SDL_Joystick* js = SDL_OpenJoystick(evt.jdevice.which);
            if (js) {
                Entry e;
                e.id = evt.jdevice.which;
                e.joystick = js;
                const char* n = SDL_GetJoystickName(js);
                e.name = n ? n : "Joystick";
                mEntries.push_back(std::move(e));
            }
        }
    } else if (evt.type == SDL_EVENT_JOYSTICK_REMOVED) {
        for (auto it = mEntries.begin(); it != mEntries.end(); ++it) {
            if (it->id == evt.jdevice.which && it->joystick) {
                SDL_CloseJoystick(it->joystick);
                mEntries.erase(it);
                break;
            }
        }
    }
}

void GamepadManager::update() {
    uint64_t now = SDL_GetTicks();
    for (auto& e : mEntries) {
        // Auto-stop rumble when its requested duration expired.
        // (SDL itself does it, but keeping our own timer lets us query
        // rumbleStopMs in UI and cancel cleanly.)
        if (e.rumbleStopMs && now >= e.rumbleStopMs) {
            if (e.gamepad) SDL_RumbleGamepad(e.gamepad, 0, 0, 0);
            e.rumbleStopMs = 0;
        }
        if (e.triggerRumbleStopMs && now >= e.triggerRumbleStopMs) {
            if (e.gamepad) SDL_RumbleGamepadTriggers(e.gamepad, 0, 0, 0);
            e.triggerRumbleStopMs = 0;
        }

        if (!e.gamepad) continue;

        // Gyroscope poll + Kalman smoothing + offset subtraction.
        if (e.hasGyro) {
            float raw[3] = {0, 0, 0};
            if (SDL_GetGamepadSensorData(e.gamepad, SDL_SENSOR_GYRO, raw, 3)) {
                // SDL returns rad/s — convert to deg/s for user-facing API.
                constexpr float kRadToDeg = 57.29577951308232f;
                float gx = raw[0] * kRadToDeg;
                float gy = raw[1] * kRadToDeg;
                float gz = raw[2] * kRadToDeg;
                e.kalmanX.step(gx, mProcessNoise, mMeasurementNoise);
                e.kalmanY.step(gy, mProcessNoise, mMeasurementNoise);
                e.kalmanZ.step(gz, mProcessNoise, mMeasurementNoise);
                e.gyroX = e.kalmanX.x - e.gyroOffsetX;
                e.gyroY = e.kalmanY.x - e.gyroOffsetY;
                e.gyroZ = e.kalmanZ.x - e.gyroOffsetZ;
            }
        }
        if (e.hasAccel) {
            float raw[3] = {0, 0, 0};
            if (SDL_GetGamepadSensorData(e.gamepad, SDL_SENSOR_ACCEL, raw, 3)) {
                // Accelerometer stays raw — plugins that want smoothing
                // can roll their own filter in Lua.
                e.accelX = raw[0];
                e.accelY = raw[1];
                e.accelZ = raw[2];
            }
        }

        // Lot K — touchpad poll. Only the first touchpad is exposed (PS5
        // has one; PS4 has one). `GetGamepadTouchpadFinger` returns per-
        // finger state.
        if (e.hasTouchpad) {
            int n = SDL_GetNumGamepadTouchpadFingers(e.gamepad, 0);
            for (int f = 0; f < 2; ++f) {
                auto& t = e.touchpad[f];
                if (f >= n) { t.down = false; continue; }
                bool down = false;
                float x = 0, y = 0, pressure = 0;
                if (SDL_GetGamepadTouchpadFinger(e.gamepad, 0, f, &down, &x, &y, &pressure)) {
                    t.down = down;
                    t.x = x;
                    t.y = y;
                    t.pressure = pressure;
                } else {
                    t.down = false;
                }
            }
        }

        // Lot K — battery poll throttled to every 2s.
        if (now >= e.batteryPollMs) {
            int pct = -1;
            SDL_PowerState sp = SDL_GetGamepadPowerInfo(e.gamepad, &pct);
            e.batteryPercent = pct;
            switch (sp) {
                case SDL_POWERSTATE_ON_BATTERY: e.batteryState = PowerState::OnBattery; break;
                case SDL_POWERSTATE_NO_BATTERY: e.batteryState = PowerState::NoBattery; break;
                case SDL_POWERSTATE_CHARGING:   e.batteryState = PowerState::Charging;  break;
                case SDL_POWERSTATE_CHARGED:    e.batteryState = PowerState::Charged;   break;
                default:                        e.batteryState = PowerState::Unknown;   break;
            }
            e.batteryPollMs = now + 2000;
        }
    }
}

// ── Lot K accessors ────────────────────────────────────────────────────────

bool GamepadManager::hasTouchpad(int index) const {
    const auto* e = entry(index); return e && e->hasTouchpad;
}

int GamepadManager::getTouchpadFingers(int index, TouchFinger out[2]) const {
    const auto* e = entry(index);
    if (!e || !e->hasTouchpad) { out[0] = {}; out[1] = {}; return 0; }
    out[0] = e->touchpad[0];
    out[1] = e->touchpad[1];
    int n = 0;
    if (out[0].down) ++n;
    if (out[1].down) ++n;
    return n;
}

bool GamepadManager::hasLED(int index) const {
    const auto* e = entry(index); return e && e->hasLED;
}

bool GamepadManager::setLED(int index, uint8_t r, uint8_t g, uint8_t b) {
    auto* e = entryMut(index);
    if (!e || !e->gamepad || !e->hasLED) return false;
    if (!SDL_SetGamepadLED(e->gamepad, r, g, b)) return false;
    e->ledR = r; e->ledG = g; e->ledB = b;
    return true;
}

int GamepadManager::getBatteryPercent(int index) const {
    const auto* e = entry(index); return e ? e->batteryPercent : -1;
}

GamepadManager::PowerState GamepadManager::getBatteryState(int index) const {
    const auto* e = entry(index); return e ? e->batteryState : PowerState::Unknown;
}

// ── Legacy API ─────────────────────────────────────────────────────────────

float GamepadManager::getAxisValue(int index, int axis) const {
    const auto* e = entry(index);
    if (!e) return 0.0f;
    if (e->gamepad) {
        Sint16 val = SDL_GetGamepadAxis(e->gamepad, static_cast<SDL_GamepadAxis>(axis));
        return static_cast<float>(val) / 32767.0f;
    }
    if (e->joystick) {
        Sint16 val = SDL_GetJoystickAxis(e->joystick, axis);
        return static_cast<float>(val) / 32767.0f;
    }
    return 0.0f;
}

bool GamepadManager::isButtonDown(int index, int button) const {
    const auto* e = entry(index);
    if (!e) return false;
    if (e->gamepad) {
        return SDL_GetGamepadButton(e->gamepad, static_cast<SDL_GamepadButton>(button));
    }
    if (e->joystick) {
        return SDL_GetJoystickButton(e->joystick, button);
    }
    return false;
}

int GamepadManager::getCount() const {
    return static_cast<int>(mEntries.size());
}

// ── Lot J additions ────────────────────────────────────────────────────────

GamepadManager::Type GamepadManager::getType(int index) const {
    const auto* e = entry(index);
    return e ? e->type : Type::Generic;
}

std::string GamepadManager::getName(int index) const {
    const auto* e = entry(index);
    return e ? e->name : std::string();
}

bool GamepadManager::rumble(int index, float low, float high, int durationMs) {
    auto* e = entryMut(index);
    if (!e || !e->gamepad) return false;
    low  = std::clamp(low,  0.0f, 1.0f);
    high = std::clamp(high, 0.0f, 1.0f);
    Uint16 lo = static_cast<Uint16>(low  * 65535.0f);
    Uint16 hi = static_cast<Uint16>(high * 65535.0f);
    bool ok = SDL_RumbleGamepad(e->gamepad, lo, hi, static_cast<Uint32>(durationMs));
    if (ok) e->rumbleStopMs = SDL_GetTicks() + static_cast<uint64_t>(durationMs);
    return ok;
}

bool GamepadManager::rumbleTriggers(int index, float left, float right, int durationMs) {
    auto* e = entryMut(index);
    if (!e || !e->gamepad) return false;
    left  = std::clamp(left,  0.0f, 1.0f);
    right = std::clamp(right, 0.0f, 1.0f);
    Uint16 l = static_cast<Uint16>(left  * 65535.0f);
    Uint16 r = static_cast<Uint16>(right * 65535.0f);
    bool ok = SDL_RumbleGamepadTriggers(e->gamepad, l, r, static_cast<Uint32>(durationMs));
    if (ok) e->triggerRumbleStopMs = SDL_GetTicks() + static_cast<uint64_t>(durationMs);
    return ok;
}

void GamepadManager::stopRumble(int index) {
    auto* e = entryMut(index);
    if (!e || !e->gamepad) return;
    SDL_RumbleGamepad        (e->gamepad, 0, 0, 0);
    SDL_RumbleGamepadTriggers(e->gamepad, 0, 0, 0);
    e->rumbleStopMs = 0;
    e->triggerRumbleStopMs = 0;
}

bool GamepadManager::hasGyro(int index) const {
    const auto* e = entry(index); return e && e->hasGyro;
}
bool GamepadManager::hasAccel(int index) const {
    const auto* e = entry(index); return e && e->hasAccel;
}

bool GamepadManager::getGyro(int index, float& ox, float& oy, float& oz) const {
    const auto* e = entry(index);
    ox = oy = oz = 0.0f;
    if (!e || !e->hasGyro) return false;
    ox = e->gyroX; oy = e->gyroY; oz = e->gyroZ;
    return true;
}

bool GamepadManager::getAccel(int index, float& ox, float& oy, float& oz) const {
    const auto* e = entry(index);
    ox = oy = oz = 0.0f;
    if (!e || !e->hasAccel) return false;
    ox = e->accelX; oy = e->accelY; oz = e->accelZ;
    return true;
}

void GamepadManager::calibrateGyro(int index) {
    auto* e = entryMut(index);
    if (!e || !e->hasGyro) return;
    // Capture the current filtered reading as the new zero.
    e->gyroOffsetX = e->kalmanX.x;
    e->gyroOffsetY = e->kalmanY.x;
    e->gyroOffsetZ = e->kalmanZ.x;
    SDL_Log("[GamepadManager] Calibrated gyro for %s: offset=(%.3f, %.3f, %.3f) deg/s",
            e->name.c_str(), e->gyroOffsetX, e->gyroOffsetY, e->gyroOffsetZ);
}

void GamepadManager::setGyroFilter(float processNoise, float measurementNoise) {
    mProcessNoise     = std::max(0.0f, processNoise);
    mMeasurementNoise = std::max(0.0001f, measurementNoise);
}

const GamepadManager::Entry* GamepadManager::entry(int index) const {
    if (index < 0 || index >= static_cast<int>(mEntries.size())) return nullptr;
    return &mEntries[index];
}
GamepadManager::Entry* GamepadManager::entryMut(int index) {
    if (index < 0 || index >= static_cast<int>(mEntries.size())) return nullptr;
    return &mEntries[index];
}

} // namespace bbfx
