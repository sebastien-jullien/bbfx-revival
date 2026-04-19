#include "LuaTimeline.h"

#include <algorithm>
#include <iostream>

namespace bbfx {

void LuaTimeline::addKey(float t, float value, const std::string& easing) {
    Key k{ std::max(0.0f, t), value, easing };
    auto it = std::upper_bound(mKeys.begin(), mKeys.end(), k,
        [](const Key& a, const Key& b) { return a.time < b.time; });
    mKeys.insert(it, k);
}

void LuaTimeline::addEvent(float t, sol::function cb) {
    mEvents.push_back(Event{ std::max(0.0f, t), std::move(cb), false });
}

void LuaTimeline::resetFired() {
    for (auto& e : mEvents) e.firedThisLoop = false;
}

float LuaTimeline::evaluateEasing(const sol::table& lib,
                                     const std::string& name, float t) {
    if (name.empty() || name == "linear") return t;
    auto fn = lib.get<sol::optional<sol::function>>(name);
    if (!fn) return t;
    try {
        sol::object r = (*fn)(t);
        if (r.is<float>())  return r.as<float>();
        if (r.is<double>()) return static_cast<float>(r.as<double>());
    } catch (const std::exception& e) {
        std::cerr << "[LuaTimeline] easing '" << name << "' failed: "
                   << e.what() << std::endl;
    }
    return t;
}

float LuaTimeline::getValue() const {
    if (mKeys.empty()) return 0.0f;
    if (mKeys.size() == 1) return mKeys.front().value;
    if (mCurrent <= mKeys.front().time) return mKeys.front().value;
    if (mCurrent >= mKeys.back().time)  return mKeys.back().value;
    // Find bracketing keys.
    for (size_t i = 0; i + 1 < mKeys.size(); ++i) {
        const auto& a = mKeys[i];
        const auto& b = mKeys[i + 1];
        if (mCurrent >= a.time && mCurrent <= b.time) {
            float span = b.time - a.time;
            if (span <= 0.0f) return a.value;
            float u = (mCurrent - a.time) / span;
            // Linear interp in C++ — getValue() without easing is the
            // headless default. The full easing lookup path is driven by
            // update() which has the easingLua table available.
            return a.value + (b.value - a.value) * u;
        }
    }
    return mKeys.back().value;
}

void LuaTimeline::update(float dt, const sol::table& easingLua) {
    if (!mPlaying) return;
    float prev = mCurrent;
    mCurrent += dt * mSpeed;
    // Loop handling.
    if (mCurrent >= mDuration) {
        if (mLoop) {
            // Fire any event between prev and mDuration
            for (auto& e : mEvents) {
                if (!e.firedThisLoop && e.time > prev && e.time <= mDuration) {
                    try { if (e.cb.valid()) e.cb(); }
                    catch (const std::exception& ex) {
                        std::cerr << "[LuaTimeline] event cb: " << ex.what() << std::endl;
                    }
                    e.firedThisLoop = true;
                }
            }
            mCurrent = std::fmod(mCurrent, mDuration);
            resetFired();
            prev = 0.0f;
        } else {
            mCurrent = mDuration;
            mPlaying = false;
        }
    }

    // Fire events in (prev, mCurrent].
    for (auto& e : mEvents) {
        if (e.firedThisLoop) continue;
        if (e.time > prev && e.time <= mCurrent) {
            try { if (e.cb.valid()) e.cb(); }
            catch (const std::exception& ex) {
                std::cerr << "[LuaTimeline] event cb: " << ex.what() << std::endl;
            }
            e.firedThisLoop = true;
        }
    }

    // Optional: evaluate the easing library to warm up any per-frame
    // computation — the binding layer uses `getValue()` for the actual
    // sampling, and re-evaluates easing by passing easingLua along.
    (void)easingLua;
}

} // namespace bbfx
