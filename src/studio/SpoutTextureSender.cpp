#include "SpoutTextureSender.h"
#include <iostream>

#ifdef BBFX_HAS_SPOUT
// NOTE: Include Spout SDK headers here when SDK is available.
// #include <SpoutLibrary.h>
#endif

namespace bbfx {

SpoutTextureSender::SpoutTextureSender() {}

SpoutTextureSender::~SpoutTextureSender() {
    release();
}

void SpoutTextureSender::setName(const std::string& name) {
    mName = name;
}

bool SpoutTextureSender::init(int width, int height) {
    mWidth  = width;
    mHeight = height;
#ifdef BBFX_HAS_SPOUT
    // Spout SDK initialisation.
    // mSender = GetSpout();
    // if (!mSender) { std::cerr << "[Spout] Failed to get Spout library" << std::endl; return false; }
    // mInitialised = reinterpret_cast<SPOUTLIBRARY*>(mSender)->CreateSender(mName.c_str(), width, height);
    mInitialised = false; // Replace with SDK call when present.
#else
    std::cout << "[Spout] SDK not available — output disabled (BBFX_HAS_SPOUT not defined)" << std::endl;
    mInitialised = false;
#endif
    return mInitialised;
}

bool SpoutTextureSender::sendTexture(unsigned int textureId, int width, int height) {
    if (!mInitialised) return false;
#ifdef BBFX_HAS_SPOUT
    // return reinterpret_cast<SPOUTLIBRARY*>(mSender)->SendTexture(textureId, GL_TEXTURE_2D, width, height);
    (void)textureId; (void)width; (void)height;
    return false;
#else
    (void)textureId; (void)width; (void)height;
    return false;
#endif
}

void SpoutTextureSender::release() {
    if (!mInitialised) return;
#ifdef BBFX_HAS_SPOUT
    // reinterpret_cast<SPOUTLIBRARY*>(mSender)->ReleaseSender();
    // reinterpret_cast<SPOUTLIBRARY*>(mSender)->Release();
    // mSender = nullptr;
#endif
    mInitialised = false;
    std::cout << "[Spout] Sender released: " << mName << std::endl;
}

} // namespace bbfx
