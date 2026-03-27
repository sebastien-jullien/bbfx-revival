#include "TheoraClip.h"
#include <iostream>

namespace bbfx {

TheoraClip::TheoraClip(const std::string& filename) {
    mReader = std::make_unique<TheoraReader>(filename);
    mReader->readHeaders();

    mBlitter = std::make_unique<TheoraBlitter>();
    std::string texName = "theora_" + filename;
    // Clean texture name
    for (auto& c : texName) {
        if (c == '/' || c == '\\' || c == '.') c = '_';
    }
    mBlitter->setup(texName, mReader->getWidth(), mReader->getHeight());

    std::cout << "[theora_clip] Loaded: " << filename
              << " (" << mReader->getWidth() << "x" << mReader->getHeight() << ")" << std::endl;
}

TheoraClip::~TheoraClip() {
    stop();
}

void TheoraClip::play() {
    if (mPlaying.load()) return;

    mFrameTimer = 0.0f;

    // If thread is still running (e.g. after pause), just resume it
    if (mRunning.load() && mThread.joinable()) {
        mPlaying.store(true);
        mFrameCond.notify_all();
        std::cout << "[theora_clip] Playing" << std::endl;
        return;
    }

    // Fresh start (first play or after stop/EOF)
    mPlaying.store(true);
    mRunning.store(true);
    mFrameReady.store(false);
    mThread = std::jthread([this](std::stop_token st) { run(st); });
    std::cout << "[theora_clip] Playing" << std::endl;
}

void TheoraClip::pause() {
    mPlaying.store(false);
    mFrameCond.notify_all();  // unblock decode thread if waiting
    std::cout << "[theora_clip] Paused" << std::endl;
}

void TheoraClip::stop() {
    mPlaying.store(false);
    mRunning.store(false);
    mFrameCond.notify_all();

    if (mThread.joinable()) {
        mThread.request_stop();
        mThread.join();
    }

    // Reset position so next play() restarts from beginning
    {
        std::lock_guard lock(mTimeMutex);
        mTime = 0.0f;
        mFrameTimer = 0.0f;
    }
    mReader->seekToTime(0.0f);
    mFrameReady.store(false);
    std::cout << "[theora_clip] Stopped (reset to start)" << std::endl;
}

void TheoraClip::run(std::stop_token stopToken) {
    while (!stopToken.stop_requested() && mRunning.load()) {
        if (!mPlaying.load() || mSeeking.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        {
            std::unique_lock lock(mFrameMutex);

            // Re-check after acquiring lock — seek or stop may have occurred
            if (!mPlaying.load() || mSeeking.load() || stopToken.stop_requested() || !mRunning.load())
                continue;

            try {
                if (!mReader->readFrame(mYCbCr)) {
                    if (mLoop.load()) {
                        mReader->seekToTime(getLoopStart());
                        std::lock_guard tlock(mTimeMutex);
                        mTime = getLoopStart();
                        continue;
                    }
                    mPlaying.store(false);
                    mRunning.store(false);
                    break;
                }
            } catch (const OggEOF&) {
                if (mLoop.load()) {
                    mReader->seekToTime(getLoopStart());
                    std::lock_guard tlock(mTimeMutex);
                    mTime = getLoopStart();
                    continue;
                }
                mPlaying.store(false);
                mRunning.store(false);
                break;
            } catch (const OggException& e) {
                std::cerr << "[theora_clip] Error: " << e.what() << std::endl;
                mPlaying.store(false);
                mRunning.store(false);
                break;
            }

            mFrameReady.store(true);

            // Wait for render thread to consume the frame
            mFrameCond.wait(lock, [this, &stopToken] {
                return !mFrameReady.load() || !mRunning.load() || !mPlaying.load()
                    || mSeeking.load() || stopToken.stop_requested();
            });
        }
    }
}

void TheoraClip::frameUpdate(float dt) {
    if (mPlaying.load()) {
        std::lock_guard lock(mTimeMutex);
        mTime += dt;
        mFrameTimer += dt;
    }

    float frameDuration = 1.0f / mReader->getFrameRate();

    // Consume frame only when enough time has passed (frame pacing)
    if (mFrameTimer >= frameDuration && mFrameReady.load()) {
        mFrameTimer -= frameDuration;
        // Prevent accumulator runaway if render was stalled
        if (mFrameTimer > frameDuration * 2.0f) mFrameTimer = 0.0f;

        std::lock_guard lock(mFrameMutex);
        if (mPlaying.load()) {
            mBlitter->blit(mYCbCr);
            mFramesRendered++;
        }
        mFrameReady.store(false);
        mFrameCond.notify_one();
    } else if (!mPlaying.load() && mFrameReady.load()) {
        // Consume pending frame when paused to unblock decode thread
        std::lock_guard lock(mFrameMutex);
        mFrameReady.store(false);
        mFrameCond.notify_one();
    }
}

void TheoraClip::setTime(float t) {
    // Signal decode thread to stop touching mReader
    mSeeking.store(true);
    mFrameCond.notify_all(); // unblock if waiting in cond

    // Acquire mFrameMutex — guaranteed safe once thread exits readFrame
    {
        std::lock_guard lock(mFrameMutex);
        mFrameReady.store(false);
        mReader->seekToTime(t);
    }
    {
        std::lock_guard lock(mTimeMutex);
        mTime = t;
        mFrameTimer = 0.0f;
    }

    // Resume decode thread
    mSeeking.store(false);
    if (mPlaying.load()) {
        mFrameCond.notify_all();
    }
}

} // namespace bbfx
