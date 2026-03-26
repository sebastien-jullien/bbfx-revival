#pragma once

#include "../core/AnimationNode.h"
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>

namespace bbfx {

class ColorShiftNode : public AnimationNode {
public:
    explicit ColorShiftNode(const string& materialName);
    virtual ~ColorShiftNode();
    void update() override;

private:
    string mMaterialName;
    static Ogre::ColourValue hsvToRgb(float h, float s, float v);
};

} // namespace bbfx
