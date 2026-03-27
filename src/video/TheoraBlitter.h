#pragma once

#include <OgreTexture.h>
#include <OgreTextureManager.h>
#include <OgreMaterial.h>
#include <OgreMaterialManager.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgrePixelFormat.h>
#include <theora/theoradec.h>
#include <string>
#include <vector>
#include <cstdint>

namespace bbfx {

class TheoraBlitter {
public:
    enum BlitMode { RAW, ALPHA, BLEND };

    TheoraBlitter();
    ~TheoraBlitter();

    void setup(const std::string& textureName, int width, int height, BlitMode mode = RAW);
    void blit(th_ycbcr_buffer ycbcr);

    const std::string& getTextureName() const { return mTextureName; }
    const std::string& getMaterialName() const { return mMaterialName; }

private:
    void decodeYCbCrToRGBA(th_ycbcr_buffer ycbcr);

    std::string mTextureName;
    std::string mMaterialName;
    Ogre::TexturePtr mTexture;
    Ogre::MaterialPtr mMaterial;
    int mWidth = 0;
    int mHeight = 0;
    BlitMode mMode = RAW;
    std::vector<uint8_t> mBitmap; // RGBA buffer

    // YUV→RGB lookup tables
    static bool sTablesInit;
    static int sY[256];
    static int sRV[256];
    static int sGU[256];
    static int sGV[256];
    static int sBU[256];
    static void initTables();
    int mBlitCount = 0;
public:
    void resetDiagnostics() { mBlitCount = 0; }
};

} // namespace bbfx
