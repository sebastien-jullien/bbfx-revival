#pragma once

#include <ogg/ogg.h>
#include <vector>
#include <string>
#include <cstdint>

namespace bbfx {

struct SeekEntry {
    int64_t granulePos;
    long fileOffset;
    float time;
};

class TheoraSeekMap {
public:
    TheoraSeekMap() = default;

    void addFrame(int64_t granulePos, long fileOffset, float time);
    void addKeyFrame(size_t index);

    bool empty() const { return mEntries.empty(); }
    size_t size() const { return mEntries.size(); }

    // Find the seek entry nearest to the given time
    const SeekEntry* findNearest(float time) const;
    // Find the nearest keyframe at or before the given time
    const SeekEntry* findKeyframeBefore(float time) const;
    // Lot AV.5 round 13 — find the INDEX (into mEntries) of the keyframe at or
    // before the given time. Returns -1 if no keyframe. Used to track the
    // current decoder frame index so getLastDecodedTime can look up the EXACT
    // recorded time of each frame instead of relying on linear-approx
    // (kf_t + framesDecoded × 1/fps) which drifts with dup frames.
    int findKeyframeIndexBefore(float time) const;
    // Get entry by frame index (0..size-1). Returns nullptr if out of range.
    const SeekEntry* getEntryAt(size_t index) const {
        return index < mEntries.size() ? &mEntries[index] : nullptr;
    }

    // Serialize/deserialize as simple text format (includes file size for staleness detection)
    bool serialize(const std::string& filepath, size_t sourceFileSize = 0) const;
    bool deserialize(const std::string& filepath, size_t sourceFileSize = 0);

    // Generate a portable cache filename
    static std::string cacheFilename(const std::string& videoFilename);

private:
    std::vector<SeekEntry> mEntries;
    std::vector<size_t> mKeyFrameIndices;
    size_t mSourceFileSize = 0;
};

} // namespace bbfx
