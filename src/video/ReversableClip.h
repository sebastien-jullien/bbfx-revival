#pragma once

#include "TheoraClip.h"

namespace bbfx {

class ReversableClip : public TheoraClip {
public:
    ReversableClip(const std::string& forwardFile, const std::string& reverseFile,
                   const std::string& instanceTag = "");
    ~ReversableClip() override;

    void doReverse();
    bool isReversed() const { return mReversed; }

private:
    std::string mForwardFile;
    std::string mReverseFile;
    bool mReversed = false;
};

} // namespace bbfx
