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

    // Record data start position
    mDataStartOffset = static_cast<long>(mFile.tellg());

    std::cout << "[theora] " << mInfo.frame_width << "x" << mInfo.frame_height
              << " @ " << getFrameRate() << " fps" << std::endl;

    // Load or build seek map
    requireSeekMap();
}

bool TheoraReader::readFrame(th_ycbcr_buffer ycbcr) {
    ogg_packet packet;
    try {
        readPacket(packet);
    } catch (const OggEOF&) {
        return false;
    }

    if (th_decode_packetin(mDecoder, &packet, nullptr) != 0) {
        return false;
    }

    if (th_decode_ycbcr_out(mDecoder, ycbcr) != 0) {
        return false;
    }

    mFrameCount++;
    return true;
}

void TheoraReader::seekToTime(float time) {
    auto* entry = mSeekMap.findKeyframeBefore(time);
    if (!entry) {
        rawseek(mDataStartOffset);
        return;
    }

    rawseek(entry->fileOffset);

    // Advance to target time
    th_ycbcr_buffer ycbcr;
    while (true) {
        ogg_packet packet;
        try {
            readPacket(packet);
        } catch (const OggEOF&) {
            break;
        }

        ogg_int64_t granpos = 0;
        if (th_decode_packetin(mDecoder, &packet, &granpos) == 0) {
            float frameTime = static_cast<float>(th_granule_time(mDecoder, granpos));
            if (frameTime >= time) {
                th_decode_ycbcr_out(mDecoder, ycbcr);
                break;
            }
        }
    }
}

void TheoraReader::buildSeekMap() {
    long savedPos = static_cast<long>(mFile.tellg());
    rawseek(mDataStartOffset);

    size_t frameIdx = 0;
    ogg_packet packet;

    try {
        while (true) {
            long offset = static_cast<long>(mFile.tellg());
            readPacket(packet);

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

    // Serialize to cache
    auto cacheFile = TheoraSeekMap::cacheFilename(mFilename);
    mSeekMap.serialize(cacheFile);
    std::cout << "[theora] Seek map built: " << frameIdx << " frames, saved to " << cacheFile << std::endl;
}

void TheoraReader::requireSeekMap() {
    auto cacheFile = TheoraSeekMap::cacheFilename(mFilename);
    if (std::filesystem::exists(cacheFile) && mSeekMap.deserialize(cacheFile)) {
        std::cout << "[theora] Seek map loaded from cache: " << cacheFile << std::endl;
        return;
    }
    buildSeekMap();
}

int TheoraReader::getWidth() const { return mInfo.frame_width; }
int TheoraReader::getHeight() const { return mInfo.frame_height; }
float TheoraReader::getFrameRate() const {
    if (mInfo.fps_denominator == 0) return 30.0f;
    return static_cast<float>(mInfo.fps_numerator) / static_cast<float>(mInfo.fps_denominator);
}

} // namespace bbfx
