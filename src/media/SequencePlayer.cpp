#include "SequencePlayer.h"
#include "ImageLoader.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>

#include <OgreHardwarePixelBuffer.h>
#include <OgrePixelFormat.h>
#include <OgreTexture.h>
#include <OgreTextureManager.h>

#ifdef BBFX_HAS_STB
// Implementation lives in src/media/stb_image_impl.cpp.
#  include <stb_image.h>
#endif

namespace bbfx {

SequencePlayer::SequencePlayer() = default;

SequencePlayer::~SequencePlayer() { release(); }

void SequencePlayer::release() {
    for (auto& n : mFrames) ImageLoader::release(n);
    mFrames.clear();
    mCurrent = 0;
    mPlaying = false;
    mElapsed = 0.0f;
}

int SequencePlayer::frameCount() const { return static_cast<int>(mFrames.size()); }

const char* SequencePlayer::backendName() const { return mBackend; }

std::string SequencePlayer::getTextureName() const {
    if (mFrames.empty()) return {};
    int idx = mCurrent;
    if (idx < 0 || idx >= static_cast<int>(mFrames.size())) idx = 0;
    return mFrames[idx];
}

void SequencePlayer::setFPS(float fps) {
    if (fps > 0.0f) mFps = fps;
}

void SequencePlayer::play()  { if (!mFrames.empty()) mPlaying = true; }
void SequencePlayer::pause() { mPlaying = false; }
void SequencePlayer::stop() {
    mPlaying = false;
    mCurrent = 0;
    mElapsed = 0.0f;
}

void SequencePlayer::update(float dt) {
    if (!mPlaying || mFrames.empty()) return;
    mElapsed += dt;
    float framePeriod = 1.0f / std::max(1e-3f, mFps);
    while (mElapsed >= framePeriod) {
        mElapsed -= framePeriod;
        mCurrent += 1;
        if (mCurrent >= static_cast<int>(mFrames.size())) {
            if (mLoop) mCurrent = 0;
            else {
                mCurrent = static_cast<int>(mFrames.size()) - 1;
                mPlaying = false;
                break;
            }
        }
    }
}

bool SequencePlayer::loadSequence(const std::string& dir, const std::string& pattern,
                                       int start, int end) {
    release();
    if (!std::filesystem::is_directory(dir)) {
        std::cerr << "[SequencePlayer] loadSequence: '" << dir
                   << "' is not a directory" << std::endl;
        return false;
    }
    char buf[512];
    int n = (end >= start) ? (end - start + 1) : 0;
    for (int i = start; i <= end; ++i) {
        std::snprintf(buf, sizeof(buf), pattern.c_str(), i);
        std::filesystem::path p = std::filesystem::path(dir) / buf;
        if (!std::filesystem::exists(p)) continue;
        auto texName = ImageLoader::load(p.string());
        if (!texName.empty()) mFrames.push_back(texName);
    }
    mBackend = "ogre";
    return !mFrames.empty();
}

bool SequencePlayer::loadGif(const std::string& path) {
    release();
#ifndef BBFX_HAS_STB
    (void)path;
    std::cerr << "[SequencePlayer] loadGif: stb_image not compiled in "
                 "(BBFX_HAS_STB); rebuild with stb installed." << std::endl;
    mBackend = "Null";
    return false;
#else
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        std::cerr << "[SequencePlayer] loadGif: cannot open " << path << std::endl;
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);

    int w = 0, h = 0, frames = 0, comp = 0;
    int* delays = nullptr;
    auto* data = stbi_load_gif_from_memory(buf.data(), static_cast<int>(buf.size()),
                                             &delays, &w, &h, &frames, &comp, 4);
    if (!data || frames <= 0) {
        if (data) stbi_image_free(data);
        std::cerr << "[SequencePlayer] loadGif: decode failed for " << path << std::endl;
        return false;
    }

    auto& tm = Ogre::TextureManager::getSingleton();
    std::string stem = std::filesystem::path(path).stem().string();
    size_t stride = static_cast<size_t>(w) * h * 4;
    for (int i = 0; i < frames; ++i) {
        std::string name = "bbfx_gif_" + stem + "_" + std::to_string(i);
        try {
            if (auto existing = tm.getByName(name); existing) tm.remove(existing);
            auto tex = tm.createManual(name,
                                         Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                                         Ogre::TEX_TYPE_2D, w, h, 0,
                                         Ogre::PF_A8R8G8B8, Ogre::TU_DEFAULT);
            auto pb = tex->getBuffer();
            pb->lock(Ogre::HardwareBuffer::HBL_DISCARD);
            auto box = pb->getCurrentLock();
            // stb gives RGBA, OGRE expects BGRA (PF_A8R8G8B8 on little-endian).
            const uint8_t* src = data + static_cast<size_t>(i) * stride;
            uint8_t* dst = reinterpret_cast<uint8_t*>(box.data);
            for (size_t p = 0; p < stride; p += 4) {
                dst[p + 0] = src[p + 2];
                dst[p + 1] = src[p + 1];
                dst[p + 2] = src[p + 0];
                dst[p + 3] = src[p + 3];
            }
            pb->unlock();
            mFrames.push_back(name);
        } catch (const std::exception& e) {
            std::cerr << "[SequencePlayer] loadGif texture create failed: "
                       << e.what() << std::endl;
        }
    }
    // Use the average delay (ms) as FPS hint.
    if (delays && frames > 0) {
        double avg = 0;
        for (int i = 0; i < frames; ++i) avg += delays[i];
        avg /= frames;
        if (avg > 0) mFps = static_cast<float>(1000.0 / avg);
    }
    stbi_image_free(data);
    // delays is freed by stb when data is freed — do not free separately.
    mBackend = "stb";
    return !mFrames.empty();
#endif
}

} // namespace bbfx
