// theora_reverse.cpp — Decode a Theora video, re-encode frames in reverse order
// Uses the SAME libtheora as our decoder to guarantee compatibility.
//
// Usage: theora_reverse input.ogg output.ogg

#include <theora/theoradec.h>
#include <theora/theoraenc.h>
#include <ogg/ogg.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>

struct YUVFrame {
    std::vector<unsigned char> y, cb, cr;
    int yStride, cbStride, crStride;
    int yWidth, yHeight, cbWidth, cbHeight;
};

static bool readOggPage(std::ifstream& file, ogg_sync_state& sync, ogg_page& page) {
    while (ogg_sync_pageout(&sync, &page) != 1) {
        char* buf = ogg_sync_buffer(&sync, 4096);
        file.read(buf, 4096);
        int bytes = (int)file.gcount();
        if (bytes == 0) return false;
        ogg_sync_wrote(&sync, bytes);
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " input.ogg output.ogg" << std::endl;
        return 1;
    }

    const char* inputFile = argv[1];
    const char* outputFile = argv[2];

    // ── DECODE all frames ──────────────────────────────────────────────────────
    std::ifstream in(inputFile, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Cannot open input: " << inputFile << std::endl;
        return 1;
    }

    ogg_sync_state syncIn;
    ogg_stream_state streamIn;
    ogg_sync_init(&syncIn);
    bool streamInit = false;

    th_info info;
    th_comment comment;
    th_setup_info* setup = nullptr;
    th_dec_ctx* decoder = nullptr;

    th_info_init(&info);
    th_comment_init(&comment);

    // Read headers
    int headersDone = 0;
    while (headersDone < 3) {
        ogg_page page;
        if (!readOggPage(in, syncIn, page)) {
            std::cerr << "Unexpected end of file during headers" << std::endl;
            return 1;
        }
        if (!streamInit) {
            ogg_stream_init(&streamIn, ogg_page_serialno(&page));
            streamInit = true;
        }
        ogg_stream_pagein(&streamIn, &page);

        ogg_packet pkt;
        while (ogg_stream_packetout(&streamIn, &pkt) == 1) {
            int ret = th_decode_headerin(&info, &comment, &setup, &pkt);
            if (ret < 0) {
                std::cerr << "Error in Theora headers" << std::endl;
                return 1;
            }
            if (ret > 0) headersDone++;
        }
    }

    decoder = th_decode_alloc(&info, setup);
    if (!decoder) {
        std::cerr << "Failed to create decoder" << std::endl;
        return 1;
    }

    std::cout << "Input: " << info.frame_width << "x" << info.frame_height
              << " pic=" << info.pic_width << "x" << info.pic_height
              << " fmt=" << info.pixel_fmt
              << " version=" << (int)info.version_major << "." << (int)info.version_minor << "." << (int)info.version_subminor
              << std::endl;

    // Decode all frames into memory
    std::vector<YUVFrame> frames;
    int frameCount = 0;

    while (true) {
        ogg_page page;
        bool gotPage = readOggPage(in, syncIn, page);
        if (gotPage) {
            ogg_stream_pagein(&streamIn, &page);
        }

        ogg_packet pkt;
        while (ogg_stream_packetout(&streamIn, &pkt) == 1) {
            if (pkt.bytes > 0 && (pkt.packet[0] & 0x80)) continue; // skip headers

            ogg_int64_t granpos = 0;
            int ret = th_decode_packetin(decoder, &pkt, &granpos);
            if (ret == 0) {
                th_ycbcr_buffer ycbcr;
                th_decode_ycbcr_out(decoder, ycbcr);

                YUVFrame frame;
                frame.yWidth = ycbcr[0].width;
                frame.yHeight = ycbcr[0].height;
                frame.yStride = ycbcr[0].stride;
                frame.cbWidth = ycbcr[1].width;
                frame.cbHeight = ycbcr[1].height;
                frame.cbStride = ycbcr[1].stride;
                // Copy Y plane
                frame.y.resize(frame.yHeight * frame.yStride);
                for (int r = 0; r < frame.yHeight; r++)
                    memcpy(&frame.y[r * frame.yStride], ycbcr[0].data + r * ycbcr[0].stride, frame.yStride);
                // Copy Cb plane
                frame.cb.resize(frame.cbHeight * frame.cbStride);
                for (int r = 0; r < frame.cbHeight; r++)
                    memcpy(&frame.cb[r * frame.cbStride], ycbcr[1].data + r * ycbcr[1].stride, frame.cbStride);
                // Copy Cr plane
                frame.cr.resize(frame.cbHeight * frame.cbStride);
                for (int r = 0; r < frame.cbHeight; r++)
                    memcpy(&frame.cr[r * frame.cbStride], ycbcr[2].data + r * ycbcr[2].stride, frame.cbStride);

                frames.push_back(std::move(frame));
                frameCount++;
                if (frameCount % 500 == 0)
                    std::cout << "  Decoded " << frameCount << " frames..." << std::endl;
            }
            // TH_DUPFRAME: skip
        }

        if (!gotPage) break;
    }

    std::cout << "Decoded " << frameCount << " frames total." << std::endl;

    th_decode_free(decoder);
    ogg_stream_clear(&streamIn);
    ogg_sync_clear(&syncIn);
    in.close();

    // ── ENCODE frames in reverse order ─────────────────────────────────────────
    th_info encInfo;
    th_info_init(&encInfo);
    encInfo.frame_width = info.frame_width;
    encInfo.frame_height = info.frame_height;
    encInfo.pic_width = info.pic_width;
    encInfo.pic_height = info.pic_height;
    encInfo.pic_x = info.pic_x;
    encInfo.pic_y = info.pic_y;
    encInfo.colorspace = info.colorspace;
    encInfo.pixel_fmt = info.pixel_fmt;
    encInfo.target_bitrate = 0;  // use quality mode
    encInfo.quality = 48;  // 0-63, higher = better
    encInfo.fps_numerator = info.fps_numerator;
    encInfo.fps_denominator = info.fps_denominator;
    encInfo.aspect_numerator = info.aspect_numerator;
    encInfo.aspect_denominator = info.aspect_denominator;
    encInfo.keyframe_granule_shift = 6;  // keyframe every 64 frames max

    th_enc_ctx* encoder = th_encode_alloc(&encInfo);
    if (!encoder) {
        std::cerr << "Failed to create encoder" << std::endl;
        return 1;
    }

    // Force keyframe every 30 frames
    int keyframeInterval = 30;
    th_encode_ctl(encoder, TH_ENCCTL_SET_KEYFRAME_FREQUENCY_FORCE, &keyframeInterval, sizeof(keyframeInterval));

    // Open output
    std::ofstream out(outputFile, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Cannot open output: " << outputFile << std::endl;
        return 1;
    }

    ogg_stream_state streamOut;
    ogg_stream_init(&streamOut, 1);

    // Write headers — must packetin each header before calling flushheader again
    {
        th_comment encComment;
        th_comment_init(&encComment);

        ogg_packet op;
        int ret;
        while ((ret = th_encode_flushheader(encoder, &encComment, &op)) > 0) {
            ogg_stream_packetin(&streamOut, &op);
        }
        if (ret < 0) {
            std::cerr << "Failed to flush headers (ret=" << ret << ")" << std::endl;
            return 1;
        }

        ogg_page outPage;
        while (ogg_stream_flush(&streamOut, &outPage)) {
            out.write(reinterpret_cast<const char*>(outPage.header), outPage.header_len);
            out.write(reinterpret_cast<const char*>(outPage.body), outPage.body_len);
        }
        th_comment_clear(&encComment);
    }

    // Encode frames in reverse
    std::cout << "Encoding " << frameCount << " frames in reverse..." << std::endl;
    for (int i = frameCount - 1; i >= 0; i--) {
        const YUVFrame& frame = frames[i];

        th_ycbcr_buffer ycbcr;
        ycbcr[0].width = frame.yWidth;
        ycbcr[0].height = frame.yHeight;
        ycbcr[0].stride = frame.yStride;
        ycbcr[0].data = const_cast<unsigned char*>(frame.y.data());
        ycbcr[1].width = frame.cbWidth;
        ycbcr[1].height = frame.cbHeight;
        ycbcr[1].stride = frame.cbStride;
        ycbcr[1].data = const_cast<unsigned char*>(frame.cb.data());
        ycbcr[2].width = frame.cbWidth;
        ycbcr[2].height = frame.cbHeight;
        ycbcr[2].stride = frame.cbStride;
        ycbcr[2].data = const_cast<unsigned char*>(frame.cr.data());

        if (th_encode_ycbcr_in(encoder, ycbcr) != 0) {
            std::cerr << "Encode error at frame " << (frameCount - 1 - i) << std::endl;
            continue;
        }

        ogg_packet pkt;
        while (th_encode_packetout(encoder, 0, &pkt) > 0) {
            ogg_stream_packetin(&streamOut, &pkt);
            ogg_page outPage;
            while (ogg_stream_pageout(&streamOut, &outPage)) {
                out.write(reinterpret_cast<const char*>(outPage.header), outPage.header_len);
                out.write(reinterpret_cast<const char*>(outPage.body), outPage.body_len);
            }
        }

        int done = frameCount - i;
        if (done % 500 == 0)
            std::cout << "  Encoded " << done << "/" << frameCount << " frames..." << std::endl;
    }

    // Flush remaining
    ogg_page outPage;
    while (ogg_stream_flush(&streamOut, &outPage)) {
        out.write(reinterpret_cast<const char*>(outPage.header), outPage.header_len);
        out.write(reinterpret_cast<const char*>(outPage.body), outPage.body_len);
    }

    th_encode_free(encoder);
    ogg_stream_clear(&streamOut);
    th_info_clear(&info);
    th_info_init(&info); // re-init for cleanup
    th_info_clear(&info);
    th_info_clear(&encInfo);
    th_comment_clear(&comment);
    if (setup) th_setup_free(setup);
    out.close();

    std::cout << "Done! Wrote " << outputFile << std::endl;
    return 0;
}
