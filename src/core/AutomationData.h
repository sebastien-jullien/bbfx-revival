#pragma once

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json_fwd.hpp>

namespace bbfx {

enum class InterpolationMode { Step, Linear, Smooth, EaseIn, EaseOut, Bezier };

struct Keyframe {
    float beat = 0.0f;
    float value = 0.0f;
    InterpolationMode mode = InterpolationMode::Linear;
    float tangentInBeat = -0.5f, tangentInValue = 0.0f;
    float tangentOutBeat = 0.5f, tangentOutValue = 0.0f;
};

struct AutomationLane {
    std::string id;
    std::string nodeName;
    std::string portName;
    std::string displayName;
    float hue = 0.0f;
    bool muted = false;
    bool collapsed = false;
    bool armed = false;
    float minVal = 0.0f;
    float maxVal = 1.0f;
    std::vector<Keyframe> keyframes;  // Always sorted by beat
};

struct CueMarker {
    float beat = 0.0f;
    std::string name;
};

struct TriggerEvent {
    float beat = 0.0f;
    std::string action;
};

struct LoopRegion {
    bool active = false;
    float startBeat = 0.0f;
    float endBeat = 8.0f;
};

class AutomationData {
public:
    std::vector<AutomationLane> lanes;
    std::vector<CueMarker> cueMarkers;
    std::vector<TriggerEvent> triggerEvents;
    LoopRegion loopRegion;

    /// Evaluate the interpolated value for a lane at a given beat position.
    float evaluate(const AutomationLane& lane, float beat) const;

    /// Serialization
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

    /// Insert a keyframe maintaining sorted order. Returns the insertion index.
    static int insertKeyframe(AutomationLane& lane, const Keyframe& kf);

    /// Remove redundant intermediate keyframes that can be linearly interpolated.
    /// Returns the number of keyframes removed.
    static int thinRedundantKeyframes(AutomationLane& lane, float threshold = 0.02f);
};

// String conversion for InterpolationMode
const char* interpolationModeToString(InterpolationMode mode);
InterpolationMode interpolationModeFromString(const std::string& s);

} // namespace bbfx
