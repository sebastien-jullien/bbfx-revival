#pragma once

#include <string>
#include <vector>
#include <sol/forward.hpp>

namespace bbfx {

/// VJ Set Editor — ordered list of scenes/segments for live performance.
/// Each segment references a project file or chord state with BPM and transition.
class SetEditorPanel {
public:
    explicit SetEditorPanel(sol::state& lua);

    void render();

    struct Segment {
        std::string name = "Untitled";
        std::string source;         // .bbfx-project path or inline chord ref
        int durationBars = 32;
        float bpm = 120.0f;
        std::string transition = "cut"; // cut, crossfade, fade_in, fade_out
        int transitionBeats = 4;
        std::string notes;
    };

    const std::vector<Segment>& getSegments() const { return mSegments; }
    int getCurrentSegment() const { return mCurrentSegment; }
    void nextSegment();
    void prevSegment();

    void saveSet(const std::string& path);
    void loadSet(const std::string& path);

private:
    sol::state& mLua;
    std::vector<Segment> mSegments;
    int mCurrentSegment = 0;
    bool mPlaying = false;
};

} // namespace bbfx
