#pragma once

#include <string>
#include <vector>
#include <functional>
#include <sol/forward.hpp>

namespace bbfx {

/// VJ Set Editor — ordered list of scenes/segments for live performance.
/// Each segment references a project file or chord state with BPM and transition.
class SetEditorPanel {
public:
    explicit SetEditorPanel(sol::state& lua);

    void render();

    /// Called every frame from StudioApp — drives auto-advance playback.
    void update(float deltaTime);

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
    bool isPlaying() const { return mPlaying; }
    void nextSegment();
    void prevSegment();

    void saveSet(const std::string& path);
    void loadSet(const std::string& path);

    /// Callback to load a .bbfx-project file (delegated to StudioApp).
    using LoadProjectFn = std::function<void(const std::string& projectPath)>;
    void setLoadProjectCallback(LoadProjectFn fn) { mLoadProjectCb = std::move(fn); }

    /// Callback to set the global BPM (via RootTimeNode or equivalent).
    using SetBpmFn = std::function<void(float bpm)>;
    void setBpmCallback(SetBpmFn fn) { mSetBpmCb = std::move(fn); }

    /// Callback to set crossfade alpha (0 = previous segment fully visible, 1 = new segment).
    using SetCrossfadeAlphaFn = std::function<void(float alpha)>;
    void setCrossfadeAlphaCallback(SetCrossfadeAlphaFn fn) { mCrossfadeAlphaCb = std::move(fn); }

private:
    void advanceToSegment(int index);
    void stopPlayback();

    sol::state& mLua;
    std::vector<Segment> mSegments;
    int mCurrentSegment = 0;
    bool mPlaying = false;
    double mElapsedBeats = 0.0;      // beats elapsed in current segment
    bool mInTransition = false;       // true during crossfade/fade transition
    double mTransitionBeats = 0.0;    // beats elapsed in transition phase

    // Callbacks
    LoadProjectFn mLoadProjectCb;
    SetBpmFn mSetBpmCb;
    SetCrossfadeAlphaFn mCrossfadeAlphaCb;
};

} // namespace bbfx
