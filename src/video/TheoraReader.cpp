#include "TheoraReader.h"
#include <iostream>
#include <filesystem>

namespace bbfx {

TheoraReader::TheoraReader(const std::string& filename)
    : OggReader(filename)
{
    th_info_init(&mInfo);
    th_comment_init(&mComment);
}

TheoraReader::~TheoraReader() {
    if (mDecoder) th_decode_free(mDecoder);
    if (mSetup) th_setup_free(mSetup);
    th_comment_clear(&mComment);
    th_info_clear(&mInfo);
}

void TheoraReader::readHeaders() {
    ogg_packet packet;
    int headerCount = 0;

    // Read the 3 Theora header packets
    while (headerCount < 3) {
        readPacket(packet);
        int ret = th_decode_headerin(&mInfo, &mComment, &mSetup, &packet);
        if (ret < 0) {
            throw OggException("Error decoding Theora headers");
        }
        if (ret > 0) headerCount++;
    }

    // Initialize decoder
    mDecoder = th_decode_alloc(&mInfo, mSetup);
    if (!mDecoder) {
        throw OggException("Failed to allocate Theora decoder");
    }

    // Record data start position (accounts for OGG sync buffering)
    mDataStartOffset = consumedOffset();

    std::cout << "[theora] " << mInfo.frame_width << "x" << mInfo.frame_height
              << " @ " << getFrameRate() << " fps"
              << " pic=" << mInfo.pic_width << "x" << mInfo.pic_height
              << " +(" << mInfo.pic_x << "," << mInfo.pic_y << ")"
              << " fmt=" << mInfo.pixel_fmt << std::endl;

    // Load or build seek map
    requireSeekMap();
}

bool TheoraReader::readFrame(th_ycbcr_buffer ycbcr) {
    ogg_packet packet;
    while (true) {
        try {
            readPacket(packet);
        } catch (const OggEOF&) {
            return false;
        }

        // Skip Theora header packets (high bit set in first byte)
        if (packet.bytes > 0 && (packet.packet[0] & 0x80))
            continue;

        ogg_int64_t granpos = 0;
        int ret = th_decode_packetin(mDecoder, &packet, &granpos);
        if (ret == TH_DUPFRAME) {
            // Duplicate/dropped frame — display buffer unchanged, skip to next
            continue;
        }
        if (ret != 0) {
            return false;
        }

        if (th_decode_ycbcr_out(mDecoder, ycbcr) != 0) {
            return false;
        }

        // Round 13 — bump frame index and use seekmap for exact time.
        // Each successful readFrame advances exactly 1 non-dup frame. Time of
        // that frame = seekmap.mEntries[mFrameIndex].time (= granule_time
        // recorded at seekmap build), which is the GROUND TRUTH source time.
        // Falls back to linear if frame index is out of seekmap range.
        mFrameIndex++;
        if (const auto* entry = mSeekMap.getEntryAt(mFrameIndex)) {
            mLastDecodedTime = entry->time;
        } else {
            mLastDecodedTime += 1.0f / getFrameRate();
        }

        if (mLogNextFrame) {
            mLogNextFrame = false;
            std::cout << "[theora_frame] Y: " << ycbcr[0].width << "x" << ycbcr[0].height
                      << " stride=" << ycbcr[0].stride
                      << " Cb: " << ycbcr[1].width << "x" << ycbcr[1].height
                      << " stride=" << ycbcr[1].stride
                      << " data=" << (ycbcr[0].data ? "OK" : "NULL") << std::endl;
        }

        mFrameCount++;
        return true;
    }
}

void TheoraReader::seekToTime(float time) {
    auto* entry = mSeekMap.findKeyframeBefore(time);
    long seekOffset = entry ? entry->fileOffset : mDataStartOffset;
    float keyframeTime = entry ? entry->time : 0.0f;
    // Round 13 — frame index of the keyframe (= where mFrameIndex starts after seek).
    int kfIndex = mSeekMap.findKeyframeIndexBefore(time);

    rawseek(seekOffset);

    // Reset Theora decoder to clear any stale state from before the seek
    if (mDecoder) {
        th_decode_free(mDecoder);
        mDecoder = th_decode_alloc(&mInfo, mSetup);
    }

    // Advance to target time by decoding frames from the keyframe.
    // The page at seekOffset may contain P-frame packets before the actual
    // keyframe. Skip those so the fresh decoder always starts from a keyframe.
    float frameDuration = 1.0f / getFrameRate();
    th_ycbcr_buffer ycbcr;
    int framesDecoded = 0;
    bool reachedTarget = false;
    bool foundKeyframe = false;
    while (true) {
        ogg_packet packet;
        try {
            readPacket(packet);
        } catch (const OggEOF&) {
            break;
        }

        // Skip Theora header packets (high bit set in first byte)
        if (packet.bytes > 0 && (packet.packet[0] & 0x80))
            continue;

        // Skip P-frames before the first keyframe — a fresh decoder
        // cannot decode them correctly (no reference frame yet)
        if (!foundKeyframe) {
            if (th_packet_iskeyframe(&packet) > 0) {
                foundKeyframe = true;
            } else {
                continue;
            }
        }

        ogg_int64_t granpos = 0;
        int ret = th_decode_packetin(mDecoder, &packet, &granpos);
        if (ret == 0) {
            // Linéaire : granule_time est inexploitable pour le reverse stream
            // (renvoie 0 ou -1 pour beaucoup de frames non-keyframes générés par
            // theora_reverse.exe), donc on garde l'estimation framesDecoded × 1/fps.
            float frameTime = keyframeTime + framesDecoded * frameDuration;
            framesDecoded++;
            if (frameTime >= time) {
                th_decode_ycbcr_out(mDecoder, ycbcr);
                reachedTarget = true;
                // Round 13 — set mFrameIndex to the EXACT landed frame's index
                // in seekmap, and read its time from seekmap for ground truth.
                mFrameIndex = (kfIndex >= 0) ? kfIndex + framesDecoded - 1 : -1;
                if (const auto* e = mSeekMap.getEntryAt(mFrameIndex)) {
                    mLastDecodedTime = e->time;
                } else {
                    mLastDecodedTime = frameTime;
                }
                std::cout << "[theora_seek] target=" << time << " kf_t=" << keyframeTime
                          << " offset=" << seekOffset << " decoded=" << framesDecoded
                          << " kfIdx=" << kfIndex << " frameIdx=" << mFrameIndex
                          << " landed_t=" << mLastDecodedTime << " OK" << std::endl;
                mLogNextFrame = true;
                return;
            }
        }
    }
    mLogNextFrame = true;
    std::cout << "[theora_seek] target=" << time << " kf_t=" << keyframeTime
              << " offset=" << seekOffset << " decoded=" << framesDecoded
              << (reachedTarget ? " OK" : " FAILED") << std::endl;
}

bool TheoraReader::getCurrentFrame(th_ycbcr_buffer ycbcr) {
    if (!mDecoder) return false;
    return th_decode_ycbcr_out(mDecoder, ycbcr) == 0;
}

void TheoraReader::seekToFrameIndex(int targetIdx) {
    if (mSeekMap.empty()) return;
    int total = static_cast<int>(mSeekMap.size());
    if (targetIdx < 0) targetIdx = 0;
    if (targetIdx >= total) targetIdx = total - 1;

    // Find the largest keyframe index ≤ targetIdx.
    int kfIdx = mSeekMap.findKeyframeIndexBefore(
        mSeekMap.getEntryAt(targetIdx) ? mSeekMap.getEntryAt(targetIdx)->time : 0.0f);
    if (kfIdx < 0 || kfIdx > targetIdx) {
        // Fallback: linear scan of keyframes <= targetIdx.
        kfIdx = 0;
    }

    const auto* kfEntry = mSeekMap.getEntryAt(kfIdx);
    if (!kfEntry) return;

    rawseek(kfEntry->fileOffset);
    if (mDecoder) {
        th_decode_free(mDecoder);
        mDecoder = th_decode_alloc(&mInfo, mSetup);
    }

    // Decode forward counting non-dup frames until we reach targetIdx.
    int currentIdx = kfIdx - 1;
    bool foundKeyframe = false;
    th_ycbcr_buffer ycbcr;
    while (currentIdx < targetIdx) {
        ogg_packet packet;
        try {
            readPacket(packet);
        } catch (const OggEOF&) {
            break;
        }
        if (packet.bytes > 0 && (packet.packet[0] & 0x80)) continue;
        if (!foundKeyframe) {
            if (th_packet_iskeyframe(&packet) > 0) {
                foundKeyframe = true;
            } else {
                continue;
            }
        }
        ogg_int64_t granpos = 0;
        int ret = th_decode_packetin(mDecoder, &packet, &granpos);
        if (ret == 0) {
            currentIdx++;
            if (currentIdx == targetIdx) {
                th_decode_ycbcr_out(mDecoder, ycbcr);
                mFrameIndex = targetIdx;
                if (const auto* e = mSeekMap.getEntryAt(targetIdx)) {
                    mLastDecodedTime = e->time;
                }
                mLogNextFrame = true;
                std::cout << "[theora_seek_idx] targetIdx=" << targetIdx
                          << " kfIdx=" << kfIdx
                          << " landed_t=" << mLastDecodedTime << " OK" << std::endl;
                return;
            }
        }
    }
    std::cout << "[theora_seek_idx] targetIdx=" << targetIdx
              << " kfIdx=" << kfIdx
              << " FAILED (currentIdx=" << currentIdx << ")" << std::endl;
}

void TheoraReader::buildSeekMap() {
    long savedPos = consumedOffset();
    rawseek(mDataStartOffset);

    size_t frameIdx = 0;
    ogg_packet packet;

    try {
        while (true) {
            readPacket(packet);
            // Use the page offset — the file position where the OGG page
            // containing this packet starts.  readPacket may have called
            // readPage (which updates mLastPageOffset) or may have returned
            // a cached packet from the same page — either way, mLastPageOffset
            // is the correct page for this packet.
            long offset = mLastPageOffset;

            ogg_int64_t granpos = 0;
            if (th_decode_packetin(mDecoder, &packet, &granpos) == 0) {
                float time = static_cast<float>(th_granule_time(mDecoder, granpos));
                mSeekMap.addFrame(granpos, offset, time);

                if (th_packet_iskeyframe(&packet) > 0) {
                    mSeekMap.addKeyFrame(frameIdx);
                }
                frameIdx++;
            }
        }
    } catch (const OggEOF&) {
        // End of file — seek map complete
    }

    rawseek(savedPos);

    // Reset decoder to clean state after scanning all frames
    if (mDecoder) {
        th_decode_free(mDecoder);
        mDecoder = th_decode_alloc(&mInfo, mSetup);
    }

    // Serialize to cache
    auto cacheFile = TheoraSeekMap::cacheFilename(mFilename);
    mSeekMap.serialize(cacheFile, getFileSize());
    std::cout << "[theora] Seek map built: " << frameIdx << " frames, saved to " << cacheFile << std::endl;
}

size_t TheoraReader::getFileSize() const {
    try {
        return static_cast<size_t>(std::filesystem::file_size(mFilename));
    } catch (...) {
        return 0;
    }
}

void TheoraReader::requireSeekMap() {
    auto cacheFile = TheoraSeekMap::cacheFilename(mFilename);
    size_t fileSize = getFileSize();
    if (std::filesystem::exists(cacheFile) && mSeekMap.deserialize(cacheFile, fileSize)) {
        std::cout << "[theora] Seek map loaded from cache: " << cacheFile << std::endl;
        return;
    }
    buildSeekMap();
}

int TheoraReader::getWidth() const { return mInfo.frame_width; }
int TheoraReader::getHeight() const { return mInfo.frame_height; }
float TheoraReader::getDuration() const {
    if (mSeekMap.empty()) return 0.0f;
    auto* last = mSeekMap.findNearest(1e9f);
    return last ? last->time : 0.0f;
}
float TheoraReader::getFrameRate() const {
    if (mInfo.fps_denominator == 0) return 30.0f;
    return static_cast<float>(mInfo.fps_numerator) / static_cast<float>(mInfo.fps_denominator);
}

} // namespace bbfx
