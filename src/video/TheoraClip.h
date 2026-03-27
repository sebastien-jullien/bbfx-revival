#pragma once

#include "TheoraReader.h"
#include "TheoraBlitter.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include <memory>

namespace bbfx {

class TheoraClip {
public:
    explicit TheoraClip(const std::string& filename);
    virtual ~TheoraClip();

    void play();
    void pause();
    void stop();
    void frameUpdate(float dt);

    void setTime(float t);
    float getTime() const { return mTime; }
    bool isPlaying() const { return mPlaying.load(); }
    bool isFrameReady() const { return mFrameReady.load(); }
    void setLoop(bool loop) { mLoop.store(loop); }
    bool isLooping() const { return mLoop.load(); }

    const std::string& getMaterialName() const { return mBlitter->getMaterialName(); }
    int getWidth() const { return mReader->getWidth(); }
    int getHeight() const { return mReader->getHeight(); }

protected:
    void run(std::stop_token stopToken);
    virtual float getLoopStart() const { return 0.0f; }

    std::unique_ptr<TheoraReader> mReader;
    std::unique_ptr<TheoraBlitter> mBlitter;

    std::jthread mThread;
    std::mutex mFrameMutex;
    std::mutex mTimeMutex;
    std::condition_variable mFrameCond;

    std::atomic<bool> mPlaying{false};
    std::atomic<bool> mRunning{false};
    std::atomic<bool> mFrameReady{false};
    std::atomic<bool> mSeeking{false};
    std::atomic<bool> mLoop{false};

    float mTime = 0.0f;
    float mDecodeTime = 0.0f;
    float mFrameTimer = 0.0f;  // accumulator for frame pacing
    th_ycbcr_buffer mYCbCr;
    int mFramesRendered = 0;
    int mFramesDropped = 0;
};

} // namespace bbfx
