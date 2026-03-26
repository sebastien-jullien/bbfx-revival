#include "TextureCrossfader.h"
#include <iostream>

namespace bbfx {

TextureCrossfader::TextureCrossfader(const std::string& materialName,
                                     const std::string& tex1,
                                     const std::string& tex2)
{
    mMaterial = Ogre::MaterialManager::getSingleton().create(materialName, "General");
    auto* pass = mMaterial->getTechnique(0)->getPass(0);
    pass->setLightingEnabled(false);

    // First texture (source)
    auto* tu1 = pass->createTextureUnitState(tex1);
    tu1->setColourOperation(Ogre::LBO_REPLACE);

    // Second texture (destination) with manual blend
    auto* tu2 = pass->createTextureUnitState(tex2);
    tu2->setColourOperationEx(Ogre::LBX_BLEND_MANUAL,
                               Ogre::LBS_TEXTURE, Ogre::LBS_CURRENT,
                               Ogre::ColourValue::White, Ogre::ColourValue::White,
                               0.0f);

    std::cout << "[crossfader] Created: " << materialName
              << " (" << tex1 << " / " << tex2 << ")" << std::endl;
}

TextureCrossfader::~TextureCrossfader() = default;

void TextureCrossfader::crossfade(float beta) {
    mBeta = std::clamp(beta, 0.0f, 1.0f);

    auto* pass = mMaterial->getTechnique(0)->getPass(0);
    if (pass->getNumTextureUnitStates() >= 2) {
        auto* tu2 = pass->getTextureUnitState(1);
        tu2->setColourOperationEx(Ogre::LBX_BLEND_MANUAL,
                                   Ogre::LBS_TEXTURE, Ogre::LBS_CURRENT,
                                   Ogre::ColourValue::White, Ogre::ColourValue::White,
                                   mBeta);
    }
}

} // namespace bbfx
