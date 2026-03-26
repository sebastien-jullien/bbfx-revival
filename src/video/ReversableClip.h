#pragma once

#include "TheoraClip.h"
#include <memory>

namespace bbfx {

class ReversableClip : public TheoraClip {
public:
    ReversableClip(const std::string& forwardFile, const std::string& reverseFile);
    ~ReversableClip() override;

    void doReverse();
    bool isReversed() const { return mReversed; }

private:
    std::unique_ptr<TheoraReader> mForwardReader;
    std::unique_ptr<TheoraReader> mReverseReader;
    bool mReversed = false;
};

} // namespace bbfx
