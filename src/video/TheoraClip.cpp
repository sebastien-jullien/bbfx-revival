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
    mPlaying.store(true);
    mRunning.store(true);

    mThread = std::jthread([this](std::stop_token st) { run(st); });
    std::cout << "[theora_clip] Playing" << std::endl;
}

void TheoraClip::pause() {
    mPlaying.store(false);
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
}

void TheoraClip::run(std::stop_token stopToken) {
    while (!stopToken.stop_requested() && mRunning.load()) {
        if (!mPlaying.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        std::unique_lock lock(mFrameMutex);

        try {
            if (!mReader->readFrame(mYCbCr)) {
                // End of file
                mPlaying.store(false);
                break;
            }
        } catch (const OggEOF&) {
            mPlaying.store(false);
            break;
        } catch (const OggException& e) {
            std::cerr << "[theora_clip] Error: " << e.what() << std::endl;
            mPlaying.store(false);
            break;
        }

        mFrameReady.store(true);

        // Wait for the render thread to consume the frame
        mFrameCond.wait(lock, [this, &stopToken] {
            return !mFrameReady.load() || !mRunning.load() || stopToken.stop_requested();
        });
    }
}

void TheoraClip::frameUpdate(float dt) {
    if (!mPlaying.load()) return;

    {
        std::lock_guard lock(mTimeMutex);
        mTime += dt;
    }

    if (mFrameReady.load()) {
        std::lock_guard lock(mFrameMutex);
        // Blit the decoded frame to texture
        mBlitter->blit(mYCbCr);
        mFrameReady.store(false);
        mFramesRendered++;
        // Signal the decode thread to produce next frame
        mFrameCond.notify_one();
    }
}

void TheoraClip::setTime(float t) {
    std::lock_guard lock(mTimeMutex);
    mTime = t;
    mReader->seekToTime(t);
}

} // namespace bbfx
