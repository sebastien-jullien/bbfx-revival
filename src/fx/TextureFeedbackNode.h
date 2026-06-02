#pragma once

#include "../core/AnimationNode.h"
#include "../core/ParamSpec.h"
#include <OgreTexture.h>
#include <OgreMaterial.h>
#include <string>
#include <vector>
#include <cstdint>

namespace bbfx {

/// TextureFeedbackNode — non-linear frame feedback (frame N-1 becomes a texture
/// for frame N). Generates a material with 2 TUS (current + previous) ready to
/// be wired through a feedback fragment shader. The animatable ports cover
/// decay, displacement (vec2), zoom and rotate. The RTTs are pre-allocated and
/// reused; a clear() trigger blits black into the previous-frame buffer.
///
/// M6 — la boucle de feedback est réalisée côté CPU : la texture source (port
/// `source_texture`) est lue (downsamplée) chaque frame, mélangée avec
/// l'accumulateur précédent décru (decay) et transformé (zoom/déplacement/rotation),
/// puis uploadée dans une texture dynamique liée au matériau. Cela produit de vraies
/// traînées sans dépendre d'un compositor/render-to-texture (qui corrompt le FBO
/// partagé avec ImGui). Les RTTs historiques restent pour compat tests.
class TextureFeedbackNode : public AnimationNode {
public:
    enum class BlendMode { Additive, Screen, Max };

    explicit TextureFeedbackNode(const std::string& name);
    ~TextureFeedbackNode() override;

    void update() override;
    void cleanup() override;
    std::string getTypeName() const override { return "TextureFeedbackNode"; }

    Ogre::TexturePtr getPrevFrameRTT() const { return mPrevFrameRTT; }
    Ogre::TexturePtr getAccumRTT()     const { return mAccumRTT; }
    const std::string& getGeneratedMaterialName() const { return mGeneratedMaterialName; }
    BlendMode getBlendMode() const { return mBlendMode; }
    bool wasClearedThisFrame() const { return mWasClearedThisFrame; }

    /// Helper for tests: simulate a frame swap (to verify RTT lifecycle).
    void _swapForTest();

private:
    ParamSpec mSpec;
    Ogre::TexturePtr mPrevFrameRTT;
    Ogre::TexturePtr mAccumRTT;
    Ogre::MaterialPtr mGeneratedMaterial;
    std::string mPrevFrameRTTName;
    std::string mAccumRTTName;
    std::string mGeneratedMaterialName;

    BlendMode mBlendMode = BlendMode::Screen;
    bool mPrevClearState = false;
    bool mWasClearedThisFrame = false;

    int mResolutionW = 1280;
    int mResolutionH = 720;

    // M6 — accumulateur de feedback CPU + texture dynamique d'affichage.
    static constexpr int kFbW = 256;
    static constexpr int kFbH = 256;
    Ogre::TexturePtr      mFeedbackTex;
    std::string           mFeedbackTexName;
    std::vector<uint8_t>  mAccum;    // RGBA, kFbW*kFbH*4 — état du feedback
    std::vector<uint8_t>  mSrcBuf;   // RGBA, scratch source downsamplée
    bool                  mAccumInit = false;
    float mFbDecay = 0.95f, mFbZoom = 1.0f, mFbDx = 0, mFbDy = 0, mFbRot = 0; // params feedback courants

    static int sCounter;

    void initRTTs();
    void destroyRTTs();
    void clearRTT(Ogre::TexturePtr rtt);
    void buildMaterialIfNeeded();
    void ensureFeedbackTexture();
    // M6 — un pas de feedback CPU : lit `srcTexName`, mélange dans mAccum, upload.
    void stepFeedback(const std::string& srcTexName, float decay, float zoom,
                      float dx, float dy, float rot);
};

} // namespace bbfx
