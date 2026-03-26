#pragma once

#include <OgreMaterial.h>
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreTextureUnitState.h>
#include <string>

namespace bbfx {

class TextureCrossfader {
public:
    TextureCrossfader(const std::string& materialName, const std::string& tex1, const std::string& tex2);
    ~TextureCrossfader();

    void crossfade(float beta);
    float getBeta() const { return mBeta; }

private:
    Ogre::MaterialPtr mMaterial;
    float mBeta = 0.0f;
};

} // namespace bbfx
