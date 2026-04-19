#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace bbfx {

/// v3.5 Lot Q — ffmpeg subprocess video decoder.
///
/// Spawns `ffmpeg -i <source> -f rawvideo -pix_fmt rgba -` as a child
/// process, pipes raw RGBA frames through stdout into a background
/// thread, and uploads frames into an OGRE texture on the main thread.
///
/// Calling code drives the clip via play/pause/seek/setSpeed/setLoop +
/// update(dt). The bridge gracefully degrades to an inert handle (open
/// returns false, texture name empty) if the ffmpeg binary is not found
/// on PATH.
///
/// Supported containers / codecs : every combination ffmpeg itself
/// decodes — MP4 (H.264/H.265), MOV, MKV, AVI, WebM, etc.
class FFmpegBridge {
public:
    FFmpegBridge();
    ~FFmpegBridge();

    /// Locate an ffmpeg executable on PATH (or common fallback dirs).
    /// Returns empty string if not found; Bridge::open() uses this to
    /// short-circuit gracefully.
    static std::string findFFmpegBinary();

    /// Open a video file. Fills in reported `width`, `height`, and `fps`.
    /// Returns false on failure — callers should treat the handle as
    /// unusable (inert, no texture updates).
    bool open(const std::string& source, int width, int height, double fps);

    void close();
    bool isOpen() const { return mRunning; }

    // Transport
    void play();
    void pause();
    void stop();
    void seek(double seconds);
    void setSpeed(double mult);
    void setLoop(bool on);

    bool isPlaying() const { return mPlaying; }
    double getSpeed() const { return mSpeed; }
    bool   isLoop()   const { return mLoop; }

    /// Poll the frame reader thread and — if a fresh frame is available —
    /// upload it into the OGRE texture. Call once per frame (main thread).
    void update(float dt);

    /// Name of the OGRE texture carrying the current frame. Empty if
    /// the bridge never successfully opened a source.
    const std::string& getTextureName() const { return mTextureName; }

    int  getWidth()  const { return mWidth;  }
    int  getHeight() const { return mHeight; }
    double getFPS()  const { return mFps;    }

private:
    // Reader thread entry point — consumes stdout of the ffmpeg child
    // and pushes decoded frames into the ring buffer.
    void readerLoop();
    bool startSubprocess(const std::string& source, double startSeconds);
    void killSubprocess();

    std::string  mSource;
    std::string  mTextureName;
    std::string  mFfmpegPath;

    int    mWidth   = 1280;
    int    mHeight  = 720;
    double mFps     = 30.0;
    double mSpeed   = 1.0;
    double mPosition = 0.0;    // seconds, updated on each frame consumed
    double mFrameAccum = 0.0;  // seconds since last frame uploaded

    std::atomic<bool> mRunning{false};
    std::atomic<bool> mPlaying{false};
    bool mLoop = false;

    std::thread mReaderThread;

    // Ring buffer — one latest-frame slot (if we fall behind we drop,
    // the bridge is about latency not perfect sync).
    std::mutex mFrameMutex;
    std::vector<uint8_t> mPendingFrame;
    bool mFramePending = false;

    // Platform-specific handles kept opaque (uintptr_t on Windows for
    // PROCESS_INFORMATION-derived handles, -1 on POSIX).
    uintptr_t mProcessHandle = 0;
    uintptr_t mStdoutRead    = 0;
};

} // namespace bbfx
