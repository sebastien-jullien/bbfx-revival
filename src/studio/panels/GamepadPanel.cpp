#include "GamepadPanel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <imgui.h>

#include "../../input/GamepadManager.h"
#include "../../input/GamepadMappingProfile.h"
#include "../../input/InputManager.h"
#include "../ToastSystem.h"

namespace bbfx {

namespace {

constexpr int    kStickButtonCount = 15;  // SDL3 face+dpad+shoulder+stick+start/back
constexpr float  kDeadzone         = 0.15f;

const char* buttonName(int b) {
    static const char* kNames[] = {
        "A", "B", "X", "Y",
        "Back", "Guide", "Start",
        "L3", "R3", "L1", "R1",
        "Up", "Down", "Left", "Right",
    };
    if (b >= 0 && b < IM_ARRAYSIZE(kNames)) return kNames[b];
    return "?";
}

ImU32 batteryColor(int pct) {
    if (pct < 0)      return IM_COL32(120, 120, 120, 255);
    if (pct < 20)     return IM_COL32(230,  60,  60, 255);
    if (pct < 50)     return IM_COL32(240, 160,  40, 255);
    return IM_COL32(60, 200, 90, 255);
}

GamepadManager& gp() {
    return InputManager::instance()->getJoystick();
}

float axisDeadzone(float v) {
    return (std::fabs(v) < kDeadzone) ? 0.0f : v;
}

void drawStick(ImDrawList* dl, ImVec2 center, float radius,
               float x, float y, const char* label) {
    dl->AddCircle(center, radius, IM_COL32(100, 100, 110, 255), 48, 2.0f);
    // Deadzone circle
    dl->AddCircle(center, radius * kDeadzone, IM_COL32(60, 60, 70, 255), 24, 1.0f);
    // Point
    ImVec2 p{ center.x + x * radius, center.y + y * radius };
    dl->AddCircleFilled(p, 6.0f, IM_COL32(100, 200, 255, 255), 16);
    dl->AddLine(center, p, IM_COL32(100, 200, 255, 180), 1.5f);
    // Label
    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText({ center.x - ts.x * 0.5f, center.y - radius - ts.y - 2 },
                IM_COL32(220, 220, 220, 255), label);
}

void drawTrigger(ImDrawList* dl, ImVec2 topLeft, ImVec2 size,
                 float value, const char* label) {
    value = std::clamp(value, 0.0f, 1.0f);
    dl->AddRect(topLeft, { topLeft.x + size.x, topLeft.y + size.y },
                IM_COL32(100, 100, 110, 255), 2.0f, 0, 1.5f);
    float h = size.y * value;
    dl->AddRectFilled({ topLeft.x + 2, topLeft.y + size.y - h },
                      { topLeft.x + size.x - 2, topLeft.y + size.y - 2 },
                      IM_COL32(255, 180, 60, 220), 1.5f);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%s %3d%%", label, int(value * 100.0f));
    dl->AddText({ topLeft.x, topLeft.y + size.y + 2 },
                IM_COL32(220, 220, 220, 255), buf);
}

// Project an XYZ point to 2D with tiny orthographic projection for the
// rotating cube preview. Rotation matrix applied is RzRyRx.
void project(float x, float y, float z,
             float ax, float ay, float az,
             ImVec2 center, float scale,
             ImVec2& out) {
    auto toRad = [](float d) { return d * 3.14159265f / 180.0f; };
    float cx = std::cos(toRad(ax)), sx = std::sin(toRad(ax));
    float cy = std::cos(toRad(ay)), sy = std::sin(toRad(ay));
    float cz = std::cos(toRad(az)), sz = std::sin(toRad(az));
    // Rx
    float y1 = cx * y - sx * z;
    float z1 = sx * y + cx * z;
    // Ry
    float x2 = cy * x + sy * z1;
    float z2 = -sy * x + cy * z1;
    // Rz
    float x3 = cz * x2 - sz * y1;
    float y3 = sz * x2 + cz * y1;
    // Orthographic projection (ignore z2, add slight perspective for depth)
    float s = scale / (1.0f + z2 * 0.002f);
    out.x = center.x + x3 * s;
    out.y = center.y + y3 * s;
}

void drawGyroCube(ImDrawList* dl, ImVec2 center, float size,
                  float ax, float ay, float az) {
    float s = size * 0.5f;
    const float verts[8][3] = {
        {-s,-s,-s},{ s,-s,-s},{ s, s,-s},{-s, s,-s},
        {-s,-s, s},{ s,-s, s},{ s, s, s},{-s, s, s},
    };
    ImVec2 p[8];
    for (int i = 0; i < 8; ++i) {
        project(verts[i][0], verts[i][1], verts[i][2],
                ax, ay, az, center, 1.0f, p[i]);
    }
    static const int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7},
    };
    for (auto& e : edges) {
        dl->AddLine(p[e[0]], p[e[1]], IM_COL32(120, 200, 255, 255), 1.5f);
    }
    // Draw local axes
    ImVec2 o, xx, yy, zz;
    project(0, 0, 0, ax, ay, az, center, 1.0f, o);
    project(size * 0.7f, 0, 0, ax, ay, az, center, 1.0f, xx);
    project(0, size * 0.7f, 0, ax, ay, az, center, 1.0f, yy);
    project(0, 0, size * 0.7f, ax, ay, az, center, 1.0f, zz);
    dl->AddLine(o, xx, IM_COL32(240,  80,  80, 255), 2.0f);
    dl->AddLine(o, yy, IM_COL32( 80, 240,  80, 255), 2.0f);
    dl->AddLine(o, zz, IM_COL32( 80,  80, 240, 255), 2.0f);
}

} // anonymous

GamepadPanel::GamepadPanel() {
    // mLastUpdateSec already initialized to 0.0 in the header;
    // ImGui::GetTime() is not safe here (context may not exist yet).
}

GamepadPanel::~GamepadPanel() = default;

void GamepadPanel::render(bool* open) {
    if (!open || !*open) return;

    ImGui::SetNextWindowSize({ 720, 600 }, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Gamepad##gp_panel", open)) {
        ImGui::End();
        return;
    }

    GamepadManager& g = gp();
    int count = g.getCount();
    if (count == 0) {
        ImGui::TextDisabled("No gamepad detected. Plug in a controller and try again.");
        ImGui::End();
        return;
    }

    if (mIndex >= count) mIndex = 0;

    double now = ImGui::GetTime();
    double dt  = std::max(0.0, now - mLastUpdateSec);
    mLastUpdateSec = now;

    // Device selector
    if (count > 1) {
        std::vector<std::string> names;
        names.reserve(count);
        for (int i = 0; i < count; ++i) names.emplace_back(g.getName(i));
        std::vector<const char*> ptrs;
        ptrs.reserve(names.size());
        for (auto& n : names) ptrs.push_back(n.c_str());
        ImGui::SetNextItemWidth(320);
        ImGui::Combo("Gamepad", &mIndex, ptrs.data(), static_cast<int>(ptrs.size()));
    }

    renderHeader(g);
    ImGui::Separator();
    renderSticks(g);
    renderTriggers(g);
    renderButtons(g);
    ImGui::Separator();
    renderGyroSphere(g, dt);
    renderTouchpad(g);
    ImGui::Separator();
    renderLED(g, now);
    renderTestButtons(g, now);
    renderCalibration(g, now);
    ImGui::Separator();
    renderLearn(g);

    ImGui::End();
}

void GamepadPanel::renderHeader(GamepadManager& g) {
    auto type = g.getType(mIndex);
    ImGui::Text("%s  —  %s", g.getName(mIndex).c_str(),
                GamepadManager::typeName(type));

    // Battery bar
    int pct = g.getBatteryPercent(mIndex);
    auto state = g.getBatteryState(mIndex);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 220);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float barW = 160.0f, barH = 18.0f;
    dl->AddRect(p, { p.x + barW, p.y + barH }, IM_COL32(120,120,120,255), 2.0f, 0, 1.0f);
    if (pct >= 0) {
        float w = (barW - 4) * (pct / 100.0f);
        dl->AddRectFilled({ p.x + 2, p.y + 2 }, { p.x + 2 + w, p.y + barH - 2 },
                          batteryColor(pct), 1.0f);
    }
    char buf[64];
    if (pct < 0) std::snprintf(buf, sizeof(buf), "  ?  (%s)", GamepadManager::powerStateName(state));
    else         std::snprintf(buf, sizeof(buf), " %d%%  (%s)", pct, GamepadManager::powerStateName(state));
    ImGui::SetCursorScreenPos({ p.x + barW + 4, p.y });
    ImGui::Text("%s", buf);
}

void GamepadPanel::renderSticks(GamepadManager& g) {
    ImGui::Text("Sticks");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float radius = 60.0f;
    float spacing = 50.0f;

    float lx = axisDeadzone(g.getAxisValue(mIndex, SDL_GAMEPAD_AXIS_LEFTX));
    float ly = axisDeadzone(g.getAxisValue(mIndex, SDL_GAMEPAD_AXIS_LEFTY));
    float rx = axisDeadzone(g.getAxisValue(mIndex, SDL_GAMEPAD_AXIS_RIGHTX));
    float ry = axisDeadzone(g.getAxisValue(mIndex, SDL_GAMEPAD_AXIS_RIGHTY));

    ImVec2 cL{ p.x + radius + 10,                  p.y + radius + 18 };
    ImVec2 cR{ p.x + radius * 3 + spacing + 10,    p.y + radius + 18 };
    drawStick(dl, cL, radius, lx, ly, "Left");
    drawStick(dl, cR, radius, rx, ry, "Right");

    ImGui::Dummy({ radius * 4 + spacing + 20, radius * 2 + 26 });
}

void GamepadPanel::renderTriggers(GamepadManager& g) {
    ImGui::Text("Triggers");
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    float lt = std::max(0.0f, g.getAxisValue(mIndex, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
    float rt = std::max(0.0f, g.getAxisValue(mIndex, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
    ImVec2 sz{ 28.0f, 100.0f };
    drawTrigger(dl, { p.x + 10,                 p.y }, sz, lt, "L2");
    drawTrigger(dl, { p.x + 10 + sz.x + 30,     p.y }, sz, rt, "R2");

    ImGui::Dummy({ sz.x * 2 + 60, sz.y + 20 });
}

void GamepadPanel::renderButtons(GamepadManager& g) {
    ImGui::Text("Buttons");
    ImVec2 cellSz{ 56.0f, 24.0f };
    int cols = 5;
    for (int i = 0; i < kStickButtonCount; ++i) {
        bool down = g.isButtonDown(mIndex, i);
        ImGui::PushStyleColor(ImGuiCol_Button,
            down ? ImVec4(0.25f, 0.75f, 0.95f, 1.0f)
                 : ImVec4(0.20f, 0.20f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            down ? ImVec4(0.05f, 0.05f, 0.08f, 1.0f)
                 : ImVec4(0.78f, 0.78f, 0.82f, 1.0f));
        ImGui::Button(buttonName(i), cellSz);
        ImGui::PopStyleColor(2);
        if ((i + 1) % cols != 0) ImGui::SameLine();
    }
}

void GamepadPanel::renderGyroSphere(GamepadManager& g, double dt) {
    ImGui::Text("Gyroscope");
    if (!g.hasGyro(mIndex)) {
        ImGui::TextDisabled("  (this controller has no gyro sensor)");
        return;
    }
    float gx, gy, gz;
    g.getGyro(mIndex, gx, gy, gz);  // deg/s, already calibrated

    // Integrate over dt with a soft decay so the cube returns to rest.
    mGyroAccumX += gx * static_cast<float>(dt);
    mGyroAccumY += gy * static_cast<float>(dt);
    mGyroAccumZ += gz * static_cast<float>(dt);
    constexpr float kDecay = 0.97f;
    mGyroAccumX *= kDecay;
    mGyroAccumY *= kDecay;
    mGyroAccumZ *= kDecay;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float size = 110.0f;
    ImVec2 center{ p.x + size * 0.5f + 10, p.y + size * 0.5f + 10 };
    dl->AddCircle(center, size * 0.5f + 6, IM_COL32(60, 60, 70, 255), 48, 1.0f);
    drawGyroCube(dl, center, size, mGyroAccumX, mGyroAccumY, mGyroAccumZ);

    ImGui::Dummy({ size + 20, size + 20 });
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("Live (deg/s)");
    ImGui::Text("  X: %+6.1f", gx);
    ImGui::Text("  Y: %+6.1f", gy);
    ImGui::Text("  Z: %+6.1f", gz);
    if (ImGui::SmallButton("Reset rotation")) {
        mGyroAccumX = mGyroAccumY = mGyroAccumZ = 0.0f;
    }
    ImGui::EndGroup();
}

void GamepadPanel::renderTouchpad(GamepadManager& g) {
    ImGui::Text("Touchpad");
    if (!g.hasTouchpad(mIndex)) {
        ImGui::TextDisabled("  (no touchpad on this controller)");
        return;
    }
    GamepadManager::TouchFinger f[2];
    int n = g.getTouchpadFingers(mIndex, f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float padW = 280.0f, padH = 130.0f;
    dl->AddRect(p, { p.x + padW, p.y + padH },
                IM_COL32(100, 100, 110, 255), 4.0f, 0, 1.5f);
    // finger colors
    ImU32 cols[2] = {
        IM_COL32( 90, 170, 255, 230),
        IM_COL32(255, 100, 120, 230),
    };
    for (int i = 0; i < n; ++i) {
        if (!f[i].down) continue;
        ImVec2 fp{ p.x + f[i].x * padW, p.y + f[i].y * padH };
        float r = 8.0f + f[i].pressure * 18.0f;
        dl->AddCircleFilled(fp, r, cols[i], 24);
        char lbl[4]; std::snprintf(lbl, sizeof(lbl), "F%d", i + 1);
        dl->AddText({ fp.x + r + 2, fp.y - 8 }, cols[i], lbl);
    }
    ImGui::Dummy({ padW + 10, padH + 10 });
}

void GamepadPanel::renderLED(GamepadManager& g, double now) {
    ImGui::Text("Light bar (LED)");
    if (!g.hasLED(mIndex)) {
        ImGui::TextDisabled("  (no addressable LED on this controller)");
        return;
    }
    ImGui::PushID("led");
    ImGui::SetNextItemWidth(200);
    ImGui::ColorEdit3("##ledColor", mLedRgb);
    ImGui::SameLine();
    if (ImGui::Button("Apply")) {
        g.setLED(mIndex,
                  (uint8_t)std::round(mLedRgb[0] * 255.0f),
                  (uint8_t)std::round(mLedRgb[1] * 255.0f),
                  (uint8_t)std::round(mLedRgb[2] * 255.0f));
    }
    ImGui::PopID();

    // While cycling, rotate the hue.
    if (mLedCycling) {
        double t = now - mLedCycleStart;
        if (t > 3.0) {
            mLedCycling = false;
        } else {
            float hue = static_cast<float>(std::fmod(t * 0.5, 1.0));
            float r, gg, b;
            ImGui::ColorConvertHSVtoRGB(hue, 1.0f, 1.0f, r, gg, b);
            mLedRgb[0] = r; mLedRgb[1] = gg; mLedRgb[2] = b;
            g.setLED(mIndex, uint8_t(r * 255), uint8_t(gg * 255), uint8_t(b * 255));
        }
    }
}

void GamepadPanel::renderTestButtons(GamepadManager& g, double now) {
    ImGui::Text("Tests");
    if (ImGui::Button("Rumble test")) {
        g.rumble(mIndex, 0.5f, 0.8f, 500);
    }
    ImGui::SameLine();
    if (ImGui::Button("Trigger rumble test")) {
        g.rumbleTriggers(mIndex, 0.7f, 0.7f, 500);
    }
    ImGui::SameLine();
    if (ImGui::Button("LED cycle 3s")) {
        mLedCycling    = true;
        mLedCycleStart = now;
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop rumble")) {
        g.stopRumble(mIndex);
    }
}

// --- Shipped mapping presets (kept relative so they pick up from the
//     exe-local resources/mappings dir; load errors surface as toasts). ---
namespace {
struct ShippedPreset {
    const char* label;
    const char* path;
};
const ShippedPreset kShippedPresets[] = {
    { "PS5 DualSense — VJ default", "resources/mappings/ps5_vj_default.bbfx-gamepad-mapping" },
    { "Xbox — DJ setup",            "resources/mappings/xbox_dj_setup.bbfx-gamepad-mapping"  },
    { "Switch Pro — Performance",   "resources/mappings/switchpro_performance.bbfx-gamepad-mapping" },
};
} // namespace

static void renderPresetDropdown(int* sel) {
    std::vector<const char*> items;
    for (auto& p : kShippedPresets) items.push_back(p.label);
    ImGui::SetNextItemWidth(260);
    ImGui::Combo("##gp_preset", sel, items.data(), static_cast<int>(items.size()));
    ImGui::SameLine();
    if (ImGui::Button("Apply preset")) {
        const auto& p = kShippedPresets[*sel];
        GamepadMappingProfile prof;
        std::string err;
        if (prof.loadFromFile(p.path, err)) {
            ToastSystem::instance().toast(
                std::string("Loaded mapping: ") + prof.name + " ("
                    + std::to_string(prof.mappings.size()) + " entries)",
                ToastSeverity::Info, 4.0f);
        } else {
            ToastSystem::instance().toast(
                std::string("Mapping load failed: ") + err,
                ToastSeverity::Error, 5.0f);
        }
    }
}

void GamepadPanel::renderCalibration(GamepadManager& g, double now) {
    ImGui::Text("Calibration");
    if (!g.hasGyro(mIndex)) {
        ImGui::TextDisabled("  (no gyro to calibrate)");
        return;
    }
    if (mCalibrating) {
        double remain = 3.0 - (now - mCalibStartSec);
        if (remain <= 0.0) {
            g.calibrateGyro(mIndex);
            mCalibrating = false;
            ToastSystem::instance().toast("Gyro calibrated", ToastSeverity::Info, 3.0f);
        } else {
            ImGui::Text("Keep the controller flat and still. Capturing in %.1fs...", remain);
            float f = 1.0f - static_cast<float>(remain / 3.0);
            ImGui::ProgressBar(f, { 240, 0 });
            if (ImGui::SmallButton("Cancel")) mCalibrating = false;
        }
    } else {
        if (ImGui::Button("Calibrate gyro")) {
            mCalibrating   = true;
            mCalibStartSec = now;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Pose the controller flat → press.");
    }

    // Shipped mapping presets dropdown + Apply.
    ImGui::Spacing();
    ImGui::TextDisabled("Mapping preset");
    static int sPresetSel = 0;
    renderPresetDropdown(&sPresetSel);
}

void GamepadPanel::renderLearn(GamepadManager& g) {
    ImGui::Text("Learn mode");
    if (mLearning) {
        updateLearnBest(g);
        ImGui::TextColored({1.0f, 0.8f, 0.2f, 1.0f},
            "Move any control on the gamepad...");
        if (!mLearnBestSource.empty()) {
            ImGui::Text("Current best : %s   (delta %.2f)",
                        mLearnBestSource.c_str(), mLearnBestDelta);
        }
        if (ImGui::Button("Confirm & assign")) {
            if (!mLearnBestSource.empty() && mLearn) {
                mLearn(mLearnBestSource);
            }
            mLearning = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            mLearning = false;
        }
    } else {
        if (ImGui::Button("Assign (learn)")) {
            snapshotLearnStart(g);
            mLearnBestSource.clear();
            mLearnBestDelta = 0.0f;
            mLearning       = true;
            mLearnStartSec  = ImGui::GetTime();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Detects the input with the strongest variation.");
    }
}

void GamepadPanel::snapshotLearnStart(GamepadManager& g) {
    mLearnStart = {};
    mLearnStart.axes[0] = g.getAxisValue(mIndex, SDL_GAMEPAD_AXIS_LEFTX);
    mLearnStart.axes[1] = g.getAxisValue(mIndex, SDL_GAMEPAD_AXIS_LEFTY);
    mLearnStart.axes[2] = g.getAxisValue(mIndex, SDL_GAMEPAD_AXIS_RIGHTX);
    mLearnStart.axes[3] = g.getAxisValue(mIndex, SDL_GAMEPAD_AXIS_RIGHTY);
    mLearnStart.axes[4] = g.getAxisValue(mIndex, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    mLearnStart.axes[5] = g.getAxisValue(mIndex, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    for (int i = 0; i < kStickButtonCount; ++i) {
        mLearnStart.buttons[i] = g.isButtonDown(mIndex, i);
    }
    if (g.hasGyro(mIndex)) {
        g.getGyro(mIndex, mLearnStart.gyroX, mLearnStart.gyroY, mLearnStart.gyroZ);
    }
    if (g.hasTouchpad(mIndex)) {
        GamepadManager::TouchFinger tf[2];
        g.getTouchpadFingers(mIndex, tf);
        mLearnStart.touch0X = tf[0].x;
        mLearnStart.touch0Y = tf[0].y;
    }
}

void GamepadPanel::updateLearnBest(GamepadManager& g) {
    static const char* kAxisTokens[6] = {
        "leftStickX", "leftStickY", "rightStickX", "rightStickY",
        "triggerL", "triggerR",
    };
    static const char* kButtonTokens[kStickButtonCount] = {
        "buttonA", "buttonB", "buttonX", "buttonY",
        "buttonBack", "buttonGuide", "buttonStart",
        "buttonL3", "buttonR3", "buttonL1", "buttonR1",
        "dpadUp", "dpadDown", "dpadLeft", "dpadRight",
    };
    auto consider = [&](const char* src, float delta) {
        float ad = std::fabs(delta);
        if (ad > mLearnBestDelta) {
            mLearnBestDelta  = ad;
            mLearnBestSource = src;
        }
    };
    for (int i = 0; i < 6; ++i) {
        int axisEnum = (i < 4)
            ? (SDL_GAMEPAD_AXIS_LEFTX + i)
            : (SDL_GAMEPAD_AXIS_LEFT_TRIGGER + (i - 4));
        float v = g.getAxisValue(mIndex, axisEnum);
        consider(kAxisTokens[i], v - mLearnStart.axes[i]);
    }
    for (int i = 0; i < kStickButtonCount; ++i) {
        bool v = g.isButtonDown(mIndex, i);
        if (v != mLearnStart.buttons[i]) consider(kButtonTokens[i], 1.0f);
    }
    if (g.hasGyro(mIndex)) {
        float x, y, z; g.getGyro(mIndex, x, y, z);
        consider("gyroX", (x - mLearnStart.gyroX) / 180.0f);
        consider("gyroY", (y - mLearnStart.gyroY) / 180.0f);
        consider("gyroZ", (z - mLearnStart.gyroZ) / 180.0f);
    }
    if (g.hasTouchpad(mIndex)) {
        GamepadManager::TouchFinger tf[2];
        g.getTouchpadFingers(mIndex, tf);
        consider("touchpad0X", tf[0].x - mLearnStart.touch0X);
        consider("touchpad0Y", tf[0].y - mLearnStart.touch0Y);
    }
}

} // namespace bbfx
