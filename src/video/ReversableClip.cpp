#include "ReversableClip.h"
#include <iostream>

namespace bbfx {

ReversableClip::ReversableClip(const std::string& forwardFile, const std::string& reverseFile)
    : TheoraClip(forwardFile), mForwardFile(forwardFile), mReverseFile(reverseFile)
{
    // Discard the reader from TheoraClip (it ran buildSeekMap).
    // Create a pristine forward reader that loads seek map from cache.
    mReader = std::make_unique<TheoraReader>(forwardFile);
    mReader->readHeaders();

    std::cout << "[theora_clip] ReversableClip: " << forwardFile << " / " << reverseFile << std::endl;
}

ReversableClip::~ReversableClip() = default;

void ReversableClip::doReverse() {
    bool wasPlaying = mPlaying.load();

    // Fully stop the decode thread
    mPlaying.store(false);
    mRunning.store(false);
    mFrameCond.notify_all();
    if (mThread.joinable()) {
        mThread.request_stop();
        mThread.join();
    }

    mReversed = !mReversed;

    // Create a FRESH reader each time to avoid stale decoder/stream state
    const std::string& file = mReversed ? mReverseFile : mForwardFile;
    mReader = std::make_unique<TheoraReader>(file);
    mReader->readHeaders();

    // Seek to mirror position: reversed time = otherDuration - currentTime
    float duration = mReader->getDuration();
    float seekTarget = duration - mTime;
    if (seekTarget < 0.0f) seekTarget = 0.0f;
    if (seekTarget > duration) seekTarget = duration;
    std::cout << "[theora_clip] doReverse: mTime=" << mTime
              << " duration=" << duration << " seekTarget=" << seekTarget << std::endl;
    mReader->seekToTime(seekTarget);
    mTime = seekTarget;
    mFrameReady.store(false);
    mBlitter->resetDiagnostics();

    // Restart playback
    if (wasPlaying) {
        play();
    }

    std::cout << "[theora_clip] Reverse: " << (mReversed ? "ON" : "OFF") << std::endl;
}

} // namespace bbfx
