#include "TheoraSeekMap.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace bbfx {

void TheoraSeekMap::addFrame(int64_t granulePos, long fileOffset, float time) {
    mEntries.push_back({granulePos, fileOffset, time});
}

void TheoraSeekMap::addKeyFrame(size_t index) {
    mKeyFrameIndices.push_back(index);
}

const SeekEntry* TheoraSeekMap::findNearest(float time) const {
    if (mEntries.empty()) return nullptr;
    const SeekEntry* best = &mEntries[0];
    for (auto& e : mEntries) {
        if (std::abs(e.time - time) < std::abs(best->time - time)) {
            best = &e;
        }
    }
    return best;
}

const SeekEntry* TheoraSeekMap::findKeyframeBefore(float time) const {
    if (mKeyFrameIndices.empty()) return nullptr;
    const SeekEntry* best = nullptr;
    for (auto idx : mKeyFrameIndices) {
        if (idx < mEntries.size() && mEntries[idx].time <= time) {
            best = &mEntries[idx];
        }
    }
    return best ? best : &mEntries[mKeyFrameIndices[0]];
}

bool TheoraSeekMap::serialize(const std::string& filepath) const {
    std::ofstream out(filepath);
    if (!out.is_open()) return false;
    out << mEntries.size() << "\n";
    for (auto& e : mEntries) {
        out << e.granulePos << " " << e.fileOffset << " " << e.time << "\n";
    }
    out << mKeyFrameIndices.size() << "\n";
    for (auto idx : mKeyFrameIndices) {
        out << idx << "\n";
    }
    return true;
}

bool TheoraSeekMap::deserialize(const std::string& filepath) {
    std::ifstream in(filepath);
    if (!in.is_open()) return false;
    size_t count;
    in >> count;
    mEntries.resize(count);
    for (size_t i = 0; i < count; i++) {
        in >> mEntries[i].granulePos >> mEntries[i].fileOffset >> mEntries[i].time;
    }
    size_t kfCount;
    in >> kfCount;
    mKeyFrameIndices.resize(kfCount);
    for (size_t i = 0; i < kfCount; i++) {
        in >> mKeyFrameIndices[i];
    }
    return true;
}

std::string TheoraSeekMap::cacheFilename(const std::string& videoFilename) {
    auto tempDir = std::filesystem::temp_directory_path();
    std::string safeName = videoFilename;
    std::replace(safeName.begin(), safeName.end(), '/', '_');
    std::replace(safeName.begin(), safeName.end(), '\\', '_');
    std::replace(safeName.begin(), safeName.end(), ':', '_');
    return (tempDir / ("bbfx_seekmap_" + safeName + ".dat")).string();
}

} // namespace bbfx
