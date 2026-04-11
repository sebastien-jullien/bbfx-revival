#pragma once
#include "../../core/AutomationData.h"
#include <string>
#include <vector>
#include <map>
#include <sol/forward.hpp>
#include <nlohmann/json_fwd.hpp>

namespace bbfx {

/// Serializes and deserializes the complete BBFx Studio project state
/// to/from a `.bbfx-project` JSON file.
///
/// Format (JSON, indented, git-friendly):
///   version, graph (nodes + links + positions), chords, timeline, bindings, media.
class ProjectSerializer {
public:
    ProjectSerializer() = default;

    /// Serialize current DAG + project state → file.
    /// nodePositions: optional editor positions for each node.
    /// chords: optional chord blocks from timeline.
    /// bpm, timeSignature: timeline settings.
    /// triggers, faders, quickAccess: performance mode state.
    struct NodePosition { std::string name; float x, y; };
    struct ChordData {
        std::string name;
        float startBeat, endBeat, hue;
        std::map<std::string, float> snapshot;
        float transitionBeats = 1.0f;
    };
    struct FaderData {
        std::string nodeName, portName;
        float minVal = 0.0f, maxVal = 1.0f;
    };
    struct QuickAccessData { std::string label, target; };
    struct TriggerSlotData {
        std::string label;
        std::string action;
        bool momentary = false;
        float hue = 0.0f;
        std::vector<std::string> macroActions; // v3.3: multi-action sequence
    };

    struct ProjectState {
        std::vector<NodePosition> nodePositions;
        std::vector<ChordData> chords;
        float bpm = 120.0f;
        std::string timeSignature = "4/4";
        std::string triggerChords[16]; // legacy (v3.2.3)
        FaderData faders[8];
        QuickAccessData quickAccess[8];
        AutomationData automation;
        std::vector<std::string> compositorStack; // v3.2.4
        std::vector<std::vector<TriggerSlotData>> triggerPages; // v3.2.4
        std::map<std::string, std::map<std::string, float>> chordSnapshots; // v3.3: chord name → port values
    };

    bool save(const std::string& path, const ProjectState& state = {});

    /// Deserialize file → recreate DAG + project state.
    bool load(const std::string& path, sol::state& lua, ProjectState* outState = nullptr);

    const std::string& getLastError() const { return mLastError; }

private:
    std::string mLastError;
};

} // namespace bbfx
