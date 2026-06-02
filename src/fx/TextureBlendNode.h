#pragma once

#include "../core/AnimationNode.h"
#include "../core/ParamSpec.h"
#include <OgreMaterial.h>
#include <string>

namespace bbfx {

/// TextureBlendNode — composes 2 textures via a 3rd masking texture.
/// Direct equivalent of the BBFx 2006 `TextureFader` 3-layer pattern
/// (cf. textureset.lua:152-153).
///
/// Generates a runtime material with 3 TextureUnitState chained:
///   - TUS 0 = tex_a (base)
///   - TUS 1 = mask (its alpha = blend factor)
///   - TUS 2 = tex_b (blended over A via LBX_BLEND_CURRENT_ALPHA = mask alpha)
///
/// Animatable per-layer transforms (scroll U/V, rotate, scale) and a
/// vertical mask offset reproduce the historic "sweep" effect.
class TextureBlendNode : public AnimationNode {
public:
    enum class BlendMode { Alpha, Add, Multiply, Screen };

    explicit TextureBlendNode(const std::string& name);
    ~TextureBlendNode() override = default;

    void update() override;
    void onFrameAdvance() override { mIntegrationArmed = true; }   // integrate the *_speed ports once per frame
    void cleanup() override;
    std::string getTypeName() const override { return "TextureBlendNode"; }

    const std::string& getGeneratedMaterialName() const { return mGeneratedMaterialName; }
    BlendMode getBlendMode() const { return mBlendMode; }

private:
    ParamSpec mSpec;
    Ogre::MaterialPtr mGeneratedMaterial;
    std::string mGeneratedMaterialName;

    // Cached previous values — delta detection avoids per-frame OGRE re-apply.
    std::string mPrevTexA, mPrevTexB, mPrevMask;
    BlendMode mBlendMode = BlendMode::Alpha;
    BlendMode mPrevBlendMode = static_cast<BlendMode>(-1);
    bool mPrevMaskInvert = false;
    float mPrevScrollUA = 999, mPrevScrollVA = 999, mPrevRotateA = 999;
    float mPrevScaleUA  = -1,  mPrevScaleVA  = -1;
    float mPrevScrollUB = 999, mPrevScrollVB = 999, mPrevRotateB = 999;
    float mPrevScaleUB  = -1,  mPrevScaleVB  = -1;
    float mPrevMaskOffsetU = 999;
    float mPrevMaskOffsetV = 999;

    // Continuous-scroll integrators (= the 2006 `TimePulseController` on each TextureScrollControllerValue).
    // Each frame, `mScrollAccum*` += dt * <port>_speed, then we feed `accum + port_offset` to the TUS.
    // Lets layer A and layer B scroll at arbitrary speeds (and OPPOSITE signs — the 2006 "two textures
    // crossing through the mask" effect requires u_a = +speed, u_b = -speed*factor).
    float mScrollAccumUA = 0.0f, mScrollAccumVA = 0.0f;
    float mScrollAccumUB = 0.0f, mScrollAccumVB = 0.0f;
    float mRotateAccumA  = 0.0f, mRotateAccumB  = 0.0f;
    float mMaskAccumU    = 0.0f, mMaskAccumV    = 0.0f;
    bool  mIntegrationArmed = false;   // set by onFrameAdvance() — integrate once per frame, not per propagation re-entry

    static int sCounter;

    void buildMaterialIfNeeded();
    void applyBlendMode();
    void applyTUSTransforms(int tusIndex,
                            float scrollU, float scrollV,
                            float rotate, float scaleU, float scaleV,
                            float& prevU, float& prevV, float& prevR,
                            float& prevSU, float& prevSV);
};

} // namespace bbfx
