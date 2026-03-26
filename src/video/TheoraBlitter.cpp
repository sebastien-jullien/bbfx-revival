#include "TheoraBlitter.h"
#include <OgreImage.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <cstring>
#include <algorithm>
#include <iostream>

namespace bbfx {

bool TheoraBlitter::sTablesInit = false;
int TheoraBlitter::sY[256];
int TheoraBlitter::sRV[256];
int TheoraBlitter::sGU[256];
int TheoraBlitter::sGV[256];
int TheoraBlitter::sBU[256];

void TheoraBlitter::initTables() {
    if (sTablesInit) return;
    for (int i = 0; i < 256; i++) {
        sY[i]  = (int)((1.164f * (i - 16)) * 8192.0f);
        sRV[i] = (int)((1.596f * (i - 128)) * 8192.0f);
        sGU[i] = (int)((-0.391f * (i - 128)) * 8192.0f);
        sGV[i] = (int)((-0.813f * (i - 128)) * 8192.0f);
        sBU[i] = (int)((2.018f * (i - 128)) * 8192.0f);
    }
    sTablesInit = true;
}

TheoraBlitter::TheoraBlitter() {
    initTables();
}

TheoraBlitter::~TheoraBlitter() = default;

void TheoraBlitter::setup(const std::string& textureName, int width, int height, BlitMode mode) {
    mTextureName = textureName;
    mMaterialName = textureName + "_mat";
    mWidth = width;
    mHeight = height;
    mMode = mode;
    mBitmap.resize(width * height * 4, 0); // RGBA

    // Create OGRE texture
    mTexture = Ogre::TextureManager::getSingleton().createManual(
        mTextureName, "General",
        Ogre::TEX_TYPE_2D, width, height, 0,
        Ogre::PF_BYTE_RGBA,
        Ogre::TU_DYNAMIC_WRITE_ONLY_DISCARDABLE);

    // Create material
    mMaterial = Ogre::MaterialManager::getSingleton().create(mMaterialName, "General");
    auto* pass = mMaterial->getTechnique(0)->getPass(0);
    pass->createTextureUnitState(mTextureName);
    pass->setLightingEnabled(false);

    std::cout << "[theora_blitter] Setup: " << width << "x" << height << " " << textureName << std::endl;
}

void TheoraBlitter::decodeYCbCrToRGBA(th_ycbcr_buffer ycbcr) {
    // YCbCr 4:2:0 → RGBA
    uint8_t* dst = mBitmap.data();
    int yStride = ycbcr[0].stride;
    int cbStride = ycbcr[1].stride;
    int crStride = ycbcr[2].stride;
    uint8_t* yData = ycbcr[0].data;
    uint8_t* cbData = ycbcr[1].data;
    uint8_t* crData = ycbcr[2].data;

    for (int y = 0; y < mHeight; y++) {
        int yIdx = y * yStride;
        int cIdx = (y / 2) * cbStride;
        for (int x = 0; x < mWidth; x++) {
            int yVal = sY[yData[yIdx + x]];
            int cb = cbData[cIdx + x / 2];
            int cr = crData[cIdx + x / 2];

            int r = (yVal + sRV[cr]) >> 13;
            int g = (yVal + sGU[cb] + sGV[cr]) >> 13;
            int b = (yVal + sBU[cb]) >> 13;

            r = std::clamp(r, 0, 255);
            g = std::clamp(g, 0, 255);
            b = std::clamp(b, 0, 255);

            int pixelIdx = (y * mWidth + x) * 4;
            dst[pixelIdx + 0] = static_cast<uint8_t>(r);
            dst[pixelIdx + 1] = static_cast<uint8_t>(g);
            dst[pixelIdx + 2] = static_cast<uint8_t>(b);
            dst[pixelIdx + 3] = 255; // Alpha
        }
    }
}

void TheoraBlitter::blit(th_ycbcr_buffer ycbcr) {
    decodeYCbCrToRGBA(ycbcr);

    // Upload to OGRE texture
    auto buffer = mTexture->getBuffer();
    Ogre::PixelBox pixelBox(mWidth, mHeight, 1, Ogre::PF_BYTE_RGBA, mBitmap.data());
    buffer->blitFromMemory(pixelBox);
}

} // namespace bbfx
