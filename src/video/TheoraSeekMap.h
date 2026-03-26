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

    // Serialize/deserialize as simple text format
    bool serialize(const std::string& filepath) const;
    bool deserialize(const std::string& filepath);

    // Generate a portable cache filename
    static std::string cacheFilename(const std::string& videoFilename);

private:
    std::vector<SeekEntry> mEntries;
    std::vector<size_t> mKeyFrameIndices;
};

} // namespace bbfx
