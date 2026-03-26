#include "ColorShiftNode.h"
#include <cmath>

namespace bbfx {

ColorShiftNode::ColorShiftNode(const string& materialName)
    : AnimationNode("ColorShiftNode")
    , mMaterialName(materialName)
{
    addInput(new AnimationPort("hue_shift", 0.0f));
    addInput(new AnimationPort("saturation", 1.0f));
    addInput(new AnimationPort("brightness", 1.0f));
}

ColorShiftNode::~ColorShiftNode() = default;

Ogre::ColourValue ColorShiftNode::hsvToRgb(float h, float s, float v) {
    // Normalize hue to 0-360
    h = std::fmod(h, 360.0f);
    if (h < 0.0f) h += 360.0f;

    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r, g, b;
    if (h < 60.0f)       { r = c; g = x; b = 0; }
    else if (h < 120.0f) { r = x; g = c; b = 0; }
    else if (h < 180.0f) { r = 0; g = c; b = x; }
    else if (h < 240.0f) { r = 0; g = x; b = c; }
    else if (h < 300.0f) { r = x; g = 0; b = c; }
    else                 { r = c; g = 0; b = x; }

    return Ogre::ColourValue(r + m, g + m, b + m, 1.0f);
}

void ColorShiftNode::update() {
    auto& inputs = getInputs();
    float hue = inputs.at("hue_shift")->getValue();
    float sat = inputs.at("saturation")->getValue();
    float bright = inputs.at("brightness")->getValue();

    auto mat = Ogre::MaterialManager::getSingleton().getByName(mMaterialName);
    if (mat) {
        Ogre::ColourValue col = hsvToRgb(hue, sat, bright);
        mat->getTechnique(0)->getPass(0)->setDiffuse(col);
    }

    fireUpdate();
}

} // namespace bbfx
