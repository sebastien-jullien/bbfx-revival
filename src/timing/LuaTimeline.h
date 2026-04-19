#pragma once

#include <string>
#include <vector>

#include <sol/sol.hpp>

namespace bbfx {

/// v3.5 Lot N — generic keyframe + event timeline exposed to Lua.
///
/// A timeline stores an ordered list of (time, value) keyframes and a
/// separate list of (time, callback) events. It advances when `update(dt)`
/// is called, interpolates between adjacent keys using a named easing
/// function (from lua/plugin/easing.lua), and fires every event whose
/// time is crossed during the step (once per loop cycle).
///
/// Timelines are value-type; the Lua binding layer owns instances via a
/// shared_ptr wrapper so handles survive across frames.
class LuaTimeline {
public:
    struct Key {
        float        time;
        float        value;
        std::string  easing;  // name from easing.lua; empty => linear
    };
    struct Event {
        float         time;
        sol::function cb;
        bool          firedThisLoop = false;
    };

    LuaTimeline() = default;

    // ── Configuration --------------------------------------------------
    void setDuration(float seconds)  { mDuration = std::max(0.001f, seconds); }
    void setLoop    (bool enabled)    { mLoop = enabled; }
    void setSpeed   (float s)         { mSpeed = s; }

    // ── Content --------------------------------------------------------
    /// Insert a keyframe; keeps the internal list sorted by time.
    void addKey(float t, float value, const std::string& easing);
    void addEvent(float t, sol::function cb);
    void clearKeys()   { mKeys.clear(); }
    void clearEvents() { mEvents.clear(); }

    // ── Transport ------------------------------------------------------
    void play()           { mPlaying = true; }
    void pause()          { mPlaying = false; }
    void stop()           { mPlaying = false; mCurrent = 0.0f; resetFired(); }
    void seek(float t)    { mCurrent = std::max(0.0f, std::min(t, mDuration)); }
    bool isPlaying() const { return mPlaying; }
    float getCurrentTime() const { return mCurrent; }
    float getDuration()    const { return mDuration; }
    float getSpeed()       const { return mSpeed; }

    // ── Sampling -------------------------------------------------------
    /// Value at the current play head, interpolated with the easing of
    /// the second-adjacent key. Returns 0 if no keys exist.
    float getValue() const;

    /// Advance the play head by dt seconds (scaled by speed); fires any
    /// events whose `time` lies in the interval [prev, current].
    /// `easingLua` is a sol::table with the same shape as
    /// `lua/plugin/easing.lua` — the caller passes it once at init.
    void update(float dt, const sol::table& easingLua);

    // Lua binding helpers
    static float evaluateEasing(const sol::table& lib,
                                 const std::string& name, float t);

private:
    void resetFired();

    std::vector<Key>   mKeys;    // sorted by time
    std::vector<Event> mEvents;
    float  mDuration = 10.0f;
    float  mCurrent  = 0.0f;
    float  mSpeed    = 1.0f;
    bool   mLoop     = false;
    bool   mPlaying  = false;
};

} // namespace bbfx
