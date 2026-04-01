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
    void cleanup() override;
    std::string getTypeName() const override { return "ColorShiftNode"; }
    void addMaterial(const std::string& matName) { mMaterialNames.push_back(matName); }

private:
    std::vector<std::string> mMaterialNames;
    struct SavedColors { Ogre::ColourValue emissive, diffuse, ambient; };
    std::vector<SavedColors> mOriginalColors;
    bool mSavedOriginal = false;
    static Ogre::ColourValue hsvToRgb(float h, float s, float v);
};

} // namespace bbfx
