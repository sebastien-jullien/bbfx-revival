#include "ReversableClip.h"
#include <iostream>

namespace bbfx {

ReversableClip::ReversableClip(const std::string& forwardFile, const std::string& reverseFile)
    : TheoraClip(forwardFile)
{
    mForwardReader = std::move(mReader);

    mReverseReader = std::make_unique<TheoraReader>(reverseFile);
    mReverseReader->readHeaders();

    mReader = std::make_unique<TheoraReader>(forwardFile);
    mReader->readHeaders();

    std::cout << "[theora_clip] ReversableClip: " << forwardFile << " / " << reverseFile << std::endl;
}

ReversableClip::~ReversableClip() = default;

void ReversableClip::doReverse() {
    bool wasPlaying = isPlaying();
    if (wasPlaying) pause();

    mReversed = !mReversed;

    // Swap readers - the base class mReader points to the active one
    if (mReversed) {
        mReader.swap(mReverseReader);
    } else {
        mReader.swap(mForwardReader);
    }

    std::cout << "[theora_clip] Reverse: " << (mReversed ? "ON" : "OFF") << std::endl;

    if (wasPlaying) play();
}

} // namespace bbfx
