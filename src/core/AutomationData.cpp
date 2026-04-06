#include "AutomationData.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cassert>

namespace bbfx {

// ── InterpolationMode string conversion ─────────────────────────────────────

const char* interpolationModeToString(InterpolationMode mode) {
    switch (mode) {
        case InterpolationMode::Step:    return "step";
        case InterpolationMode::Linear:  return "linear";
        case InterpolationMode::Smooth:  return "smooth";
        case InterpolationMode::EaseIn:  return "ease_in";
        case InterpolationMode::EaseOut: return "ease_out";
        case InterpolationMode::Bezier:  return "bezier";
    }
    return "linear";
}

InterpolationMode interpolationModeFromString(const std::string& s) {
    if (s == "step")     return InterpolationMode::Step;
    if (s == "smooth")   return InterpolationMode::Smooth;
    if (s == "ease_in")  return InterpolationMode::EaseIn;
    if (s == "ease_out") return InterpolationMode::EaseOut;
    if (s == "bezier")   return InterpolationMode::Bezier;
    return InterpolationMode::Linear;
}

// ── Interpolation helpers ───────────────────────────────────────────────────

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static float interpolate(InterpolationMode mode, float prevVal, float nextVal, float t,
                          const Keyframe& prev, const Keyframe& next) {
    switch (mode) {
        case InterpolationMode::Step:
            return prevVal;

        case InterpolationMode::Linear:
            return lerp(prevVal, nextVal, t);

        case InterpolationMode::Smooth: {
            // Hermite interpolation (ease-in-out)
            float h00 = 2*t*t*t - 3*t*t + 1;
            float h01 = -2*t*t*t + 3*t*t;
            return h00 * prevVal + h01 * nextVal;
        }

        case InterpolationMode::EaseIn: {
            float et = t * t;
            return lerp(prevVal, nextVal, et);
        }

        case InterpolationMode::EaseOut: {
            float et = 1.0f - (1.0f - t) * (1.0f - t);
            return lerp(prevVal, nextVal, et);
        }

        case InterpolationMode::Bezier: {
            // Cubic bezier with tangent handles
            float dt = next.beat - prev.beat;
            if (dt <= 0.0f) return prevVal;

            float p0 = prevVal;
            float p1 = prevVal + prev.tangentOutValue;
            float p2 = nextVal + next.tangentInValue;
            float p3 = nextVal;

            // De Casteljau
            float u = 1.0f - t;
            return u*u*u*p0 + 3*u*u*t*p1 + 3*u*t*t*p2 + t*t*t*p3;
        }
    }
    return lerp(prevVal, nextVal, t);
}

// ── AutomationData::evaluate ────────────────────────────────────────────────

float AutomationData::evaluate(const AutomationLane& lane, float beat) const {
    const auto& kfs = lane.keyframes;
    if (kfs.empty()) return 0.0f;
    if (beat <= kfs.front().beat) return kfs.front().value;
    if (beat >= kfs.back().beat) return kfs.back().value;

    // Binary search for the first keyframe with beat > target
    auto it = std::lower_bound(kfs.begin(), kfs.end(), beat,
        [](const Keyframe& kf, float b) { return kf.beat < b; });

    if (it == kfs.begin()) return kfs.front().value;
    if (it == kfs.end()) return kfs.back().value;

    const Keyframe& next = *it;
    const Keyframe& prev = *(it - 1);

    float dt = next.beat - prev.beat;
    if (dt <= 0.0f) return prev.value;

    float t = (beat - prev.beat) / dt;
    return interpolate(prev.mode, prev.value, next.value, t, prev, next);
}

// ── insertKeyframe ──────────────────────────────────────────────────────────

int AutomationData::insertKeyframe(AutomationLane& lane, const Keyframe& kf) {
    auto it = std::lower_bound(lane.keyframes.begin(), lane.keyframes.end(), kf.beat,
        [](const Keyframe& k, float b) { return k.beat < b; });
    int idx = static_cast<int>(it - lane.keyframes.begin());
    lane.keyframes.insert(it, kf);
    assert(std::is_sorted(lane.keyframes.begin(), lane.keyframes.end(),
        [](const Keyframe& a, const Keyframe& b) { return a.beat < b.beat; }));
    return idx;
}

// ── thinRedundantKeyframes ───────────────────────────────────────────────────

int AutomationData::thinRedundantKeyframes(AutomationLane& lane, float threshold) {
    if (lane.keyframes.size() < 3) return 0;

    int removed = 0;
    auto& kfs = lane.keyframes;
    for (int i = static_cast<int>(kfs.size()) - 2; i >= 1; --i) {
        const auto& prev = kfs[i - 1];
        const auto& curr = kfs[i];
        const auto& next = kfs[i + 1];
        float dt = next.beat - prev.beat;
        if (dt <= 0.0f) continue;
        float t = (curr.beat - prev.beat) / dt;
        float interpolated = prev.value + (next.value - prev.value) * t;
        if (std::abs(curr.value - interpolated) < threshold) {
            kfs.erase(kfs.begin() + i);
            ++removed;
        }
    }
    return removed;
}

// ── Serialization ───────────────────────────────────────────────────────────

nlohmann::json AutomationData::toJson() const {
    nlohmann::json j;

    // Lanes
    nlohmann::json jLanes = nlohmann::json::array();
    for (const auto& lane : lanes) {
        nlohmann::json jLane;
        jLane["id"] = lane.id;
        jLane["nodeName"] = lane.nodeName;
        jLane["portName"] = lane.portName;
        jLane["displayName"] = lane.displayName;
        jLane["hue"] = lane.hue;
        jLane["muted"] = lane.muted;
        jLane["collapsed"] = lane.collapsed;
        jLane["minVal"] = lane.minVal;
        jLane["maxVal"] = lane.maxVal;

        nlohmann::json jKfs = nlohmann::json::array();
        for (const auto& kf : lane.keyframes) {
            nlohmann::json jKf;
            jKf["beat"] = kf.beat;
            jKf["value"] = kf.value;
            jKf["mode"] = interpolationModeToString(kf.mode);
            if (kf.mode == InterpolationMode::Bezier) {
                jKf["tangentIn"] = { kf.tangentInBeat, kf.tangentInValue };
                jKf["tangentOut"] = { kf.tangentOutBeat, kf.tangentOutValue };
            }
            jKfs.push_back(jKf);
        }
        jLane["keyframes"] = jKfs;
        jLanes.push_back(jLane);
    }
    j["lanes"] = jLanes;

    // Cue markers
    nlohmann::json jCues = nlohmann::json::array();
    for (const auto& cue : cueMarkers) {
        jCues.push_back({ {"beat", cue.beat}, {"name", cue.name} });
    }
    j["cueMarkers"] = jCues;

    // Trigger events
    nlohmann::json jTriggers = nlohmann::json::array();
    for (const auto& te : triggerEvents) {
        jTriggers.push_back({ {"beat", te.beat}, {"action", te.action} });
    }
    j["triggerEvents"] = jTriggers;

    // Loop region
    j["loopRegion"] = {
        {"active", loopRegion.active},
        {"startBeat", loopRegion.startBeat},
        {"endBeat", loopRegion.endBeat}
    };

    return j;
}

void AutomationData::fromJson(const nlohmann::json& j) {
    lanes.clear();
    cueMarkers.clear();
    triggerEvents.clear();
    loopRegion = {};

    if (j.contains("lanes")) {
        for (const auto& jLane : j["lanes"]) {
            AutomationLane lane;
            lane.id = jLane.value("id", "");
            lane.nodeName = jLane.value("nodeName", "");
            lane.portName = jLane.value("portName", "");
            lane.displayName = jLane.value("displayName", "");
            lane.hue = jLane.value("hue", 0.0f);
            lane.muted = jLane.value("muted", false);
            lane.collapsed = jLane.value("collapsed", false);
            lane.minVal = jLane.value("minVal", 0.0f);
            lane.maxVal = jLane.value("maxVal", 1.0f);

            if (jLane.contains("keyframes")) {
                for (const auto& jKf : jLane["keyframes"]) {
                    Keyframe kf;
                    kf.beat = jKf.value("beat", 0.0f);
                    kf.value = jKf.value("value", 0.0f);
                    kf.mode = interpolationModeFromString(jKf.value("mode", "linear"));
                    if (jKf.contains("tangentIn")) {
                        kf.tangentInBeat = jKf["tangentIn"][0].get<float>();
                        kf.tangentInValue = jKf["tangentIn"][1].get<float>();
                    }
                    if (jKf.contains("tangentOut")) {
                        kf.tangentOutBeat = jKf["tangentOut"][0].get<float>();
                        kf.tangentOutValue = jKf["tangentOut"][1].get<float>();
                    }
                    lane.keyframes.push_back(kf);
                }
            }
            lanes.push_back(std::move(lane));
        }
    }

    if (j.contains("cueMarkers")) {
        for (const auto& jCue : j["cueMarkers"]) {
            CueMarker cue;
            cue.beat = jCue.value("beat", 0.0f);
            cue.name = jCue.value("name", "");
            cueMarkers.push_back(cue);
        }
    }

    if (j.contains("triggerEvents")) {
        for (const auto& jTe : j["triggerEvents"]) {
            TriggerEvent te;
            te.beat = jTe.value("beat", 0.0f);
            te.action = jTe.value("action", "");
            triggerEvents.push_back(te);
        }
    }

    if (j.contains("loopRegion")) {
        const auto& jLoop = j["loopRegion"];
        loopRegion.active = jLoop.value("active", false);
        loopRegion.startBeat = jLoop.value("startBeat", 0.0f);
        loopRegion.endBeat = jLoop.value("endBeat", 8.0f);
    }
}

} // namespace bbfx
