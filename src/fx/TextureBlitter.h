#pragma once

#include "../bbfx.h"
#include <OgreTexture.h>
#include <OgreTextureManager.h>

namespace bbfx {

class TextureBlitter {
public:
    explicit TextureBlitter(const string& textureName);
    virtual ~TextureBlitter();
    void blit();

protected:
    string mTextureName;
    Ogre::TexturePtr mTexture;
};

} // namespace bbfx
