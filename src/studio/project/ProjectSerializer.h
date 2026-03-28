#pragma once
#include <string>
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
    bool save(const std::string& path);

    /// Deserialize file → recreate DAG + project state.
    bool load(const std::string& path, sol::state& lua);

    const std::string& getLastError() const { return mLastError; }

private:
    std::string mLastError;
};

} // namespace bbfx
