#pragma once

#include <ogg/ogg.h>
#include <string>
#include <fstream>
#include <stdexcept>

namespace bbfx {

class OggEOF : public std::runtime_error {
public:
    OggEOF() : std::runtime_error("End of Ogg stream") {}
};

class OggException : public std::runtime_error {
public:
    explicit OggException(const std::string& msg) : std::runtime_error(msg) {}
};

class OggReader {
public:
    explicit OggReader(const std::string& filename);
    virtual ~OggReader();

    int readBuffer();
    int readPage(ogg_page& page);
    int readPacket(ogg_packet& packet);
    void rawseek(long offset);
    void seekPacket(long packetNo);

    // File offset up to which OGG sync has consumed data.
    // Unlike mFile.tellg(), this accounts for buffered-but-unconsumed bytes.
    long consumedOffset();

    const std::string& getFilename() const { return mFilename; }
    long getDataStartOffset() const { return mDataStartOffset; }

protected:
    std::string mFilename;
    std::ifstream mFile;
    ogg_sync_state mSyncState;
    ogg_stream_state mStreamState;
    bool mStreamInitialized = false;
    long mDataStartOffset = 0;
    long mLastPageOffset = 0;  // file offset of last page extracted by readPage

    static constexpr int BUFFER_SIZE = 4096;
};

} // namespace bbfx
