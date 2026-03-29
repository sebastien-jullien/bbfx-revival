#pragma once
#include <string>
#include <vector>
#include <sol/forward.hpp>

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
    struct ChordData { std::string name; float startBeat, endBeat, hue; };
    struct FaderData { std::string nodeName, portName; };
    struct QuickAccessData { std::string label, target; };

    struct ProjectState {
        std::vector<NodePosition> nodePositions;
        std::vector<ChordData> chords;
        float bpm = 120.0f;
        std::string timeSignature = "4/4";
        std::string triggerChords[16];
        FaderData faders[8];
        QuickAccessData quickAccess[8];
    };

    bool save(const std::string& path, const ProjectState& state = {});

    /// Deserialize file → recreate DAG + project state.
    bool load(const std::string& path, sol::state& lua, ProjectState* outState = nullptr);

    const std::string& getLastError() const { return mLastError; }

private:
    std::string mLastError;
};

} // namespace bbfx
