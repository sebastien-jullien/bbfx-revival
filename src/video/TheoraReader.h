#pragma once

#include "OggReader.h"
#include "TheoraSeekMap.h"
#include <theora/theoradec.h>
#include <string>

namespace bbfx {

class TheoraReader : public OggReader {
public:
    explicit TheoraReader(const std::string& filename);
    ~TheoraReader() override;

    void readHeaders();
    bool readFrame(th_ycbcr_buffer ycbcr);
    void seekToTime(float time);
    void buildSeekMap();

    int getWidth() const;
    int getHeight() const;
    float getFrameRate() const;

private:
    void requireSeekMap();

    th_info mInfo;
    th_comment mComment;
    th_setup_info* mSetup = nullptr;
    th_dec_ctx* mDecoder = nullptr;
    TheoraSeekMap mSeekMap;
    int mFrameCount = 0;
};

} // namespace bbfx
