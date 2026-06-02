#pragma once

#include "../bbfx.h"
#include <OgreTexture.h>
#include <OgreTextureManager.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgrePixelFormat.h>

namespace bbfx {

class TextureBlitter {
public:
    explicit TextureBlitter(const string& textureName);
    virtual ~TextureBlitter();
    /// Remplit la texture. pattern : 0=solid, 1=checker, 2=gradient horizontal,
    /// 3=gradient vertical, 4=stripes horizontales, 5=stripes verticales.
    /// rgba ∈ [0,1] est la couleur principale ; le fond des motifs est noir.
    void blit(float r = 0.0f, float g = 0.0f, float b = 1.0f, float a = 1.0f, int pattern = 0);

protected:
    string mTextureName;
    Ogre::TexturePtr mTexture;
};

} // namespace bbfx
