#include "OggReader.h"
#include <cstring>

namespace bbfx {

OggReader::OggReader(const std::string& filename)
    : mFilename(filename)
{
    mFile.open(filename, std::ios::binary);
    if (!mFile.is_open()) {
        throw OggException("Cannot open file: " + filename);
    }

    ogg_sync_init(&mSyncState);
}

OggReader::~OggReader() {
    if (mStreamInitialized) {
        ogg_stream_clear(&mStreamState);
    }
    ogg_sync_clear(&mSyncState);
    if (mFile.is_open()) mFile.close();
}

int OggReader::readBuffer() {
    char* buffer = ogg_sync_buffer(&mSyncState, BUFFER_SIZE);
    mFile.read(buffer, BUFFER_SIZE);
    auto bytes = static_cast<int>(mFile.gcount());
    if (bytes == 0) throw OggEOF();
    ogg_sync_wrote(&mSyncState, bytes);
    return bytes;
}

int OggReader::readPage(ogg_page& page) {
    while (ogg_sync_pageout(&mSyncState, &page) != 1) {
        readBuffer();
    }

    if (!mStreamInitialized) {
        ogg_stream_init(&mStreamState, ogg_page_serialno(&page));
        mStreamInitialized = true;
    }
    ogg_stream_pagein(&mStreamState, &page);
    return 0;
}

int OggReader::readPacket(ogg_packet& packet) {
    while (ogg_stream_packetout(&mStreamState, &packet) != 1) {
        ogg_page page;
        readPage(page);
    }
    return 0;
}

void OggReader::rawseek(long offset) {
    mFile.clear();
    mFile.seekg(offset, std::ios::beg);
    ogg_sync_reset(&mSyncState);
    if (mStreamInitialized) {
        ogg_stream_reset(&mStreamState);
    }
}

void OggReader::seekPacket(long packetNo) {
    rawseek(packetNo);
}

} // namespace bbfx
