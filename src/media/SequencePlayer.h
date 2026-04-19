#pragma once

#include <memory>
#include <string>
#include <vector>

namespace bbfx {

/// v3.5 Lot Q — GIF / image-sequence playback.
///
/// A SequencePlayer holds an ordered list of OGRE textures and advances
/// through them at a configurable FPS. The currently visible frame's
/// name is surfaced via `getTextureName()` so any material or custom
/// ImGui image can sample it.
class SequencePlayer {
public:
    SequencePlayer();
    ~SequencePlayer();

    /// Load an animated GIF. Returns false if the stb_image backend is
    /// not compiled in (BBFX_HAS_STB) or the file cannot be decoded.
    bool loadGif(const std::string& path);

    /// Load a numeric image sequence. Builds `dir/<pattern>` where
    /// `pattern` contains `%d` or `%0Nd` substitutions filled by i in
    /// `[start, end]`. Missing frames are skipped silently.
    bool loadSequence(const std::string& dir, const std::string& pattern,
                        int start, int end);

    void setFPS(float fps);
    float getFPS() const { return mFps; }

    void setLoop(bool loop) { mLoop = loop; }
    bool isLoop() const { return mLoop; }

    void play();
    void pause();
    void stop();
    bool isPlaying() const { return mPlaying; }

    /// Advance the play head by `dt` seconds. Call once per frame from
    /// the engine loop. Main-thread only.
    void update(float dt);

    /// Name of the currently visible OGRE texture. Empty if no frames.
    std::string getTextureName() const;

    int  frameCount() const;
    int  currentIndex() const { return mCurrent; }

    /// Free all frames.
    void release();

    /// Backend name for introspection : "stb" / "ogre" / "Null".
    const char* backendName() const;

private:
    std::vector<std::string> mFrames;   // OGRE texture names
    float  mFps      = 12.0f;
    float  mElapsed  = 0.0f;
    int    mCurrent  = 0;
    bool   mPlaying  = false;
    bool   mLoop     = true;
    const char* mBackend = "Null";
};

} // namespace bbfx
