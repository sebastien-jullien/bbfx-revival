#pragma once

#include "../core/AnimationNode.h"
#include "../core/ParamSpec.h"
#include <OgreTexture.h>
#include <string>

namespace bbfx {

/// GrayscaleNode — runtime BT.709 luminance desaturation of a source texture
/// (Lot U, v3.5.2 S5).
///
/// Reads `source_texture` (any OGRE Texture — file-loaded, RTT, or live like
/// NoiseTextureNode/SpectrogramTextureNode/TheoraClipNode) and produces a
/// dynamic RTT-style texture containing the desaturated copy.
///
///   gray = dot(rgb, vec3(0.2126, 0.7152, 0.0722))    // Rec.709 luminance
///   out  = mix(rgb, vec3(gray), u_mix) * u_intensity
///
/// Render-once pattern : re-renders only when source name, mix value, or
/// intensity value changes. Output texture name exposed via the `texture_out`
/// ParamSpec mirror — directly consumable by MaterialBridgeNode (Lot T) which
/// auto-wraps texture names into materials.
///
/// CPU runtime path : the RTT is filled CPU-side via `blitToMemory` + `lock`.
/// The GPU shader path that was scaffolded in v3.5.2 Lot U was REMOVED in
/// Sprint S7 Lot AA (BBFx/Grayscale material + grayscale.frag deleted) — the
/// CPU path covers the typical VJ scope and the GPU material was never
/// branched. Re-introduction is deferred to v3.6+ if profiling proves a need.
class GrayscaleNode : public AnimationNode {
public:
    explicit GrayscaleNode(const std::string& name);
    ~GrayscaleNode() override;

    void update() override;
    void cleanup() override;
    std::string getTypeName() const override { return "GrayscaleNode"; }

    Ogre::TexturePtr getTexture() const { return mTex; }
    const std::string& getTextureName() const { return mTexName; }

    // Test hook — exposes the most-recent (mix, intensity, sourceName) used
    // for a render. Lets dbg.test() prove the render path executed without
    // requiring a pixel-readback in headless mode.
    bool   wasRendered() const { return mLastRendered; }
    float  lastMix() const { return mPrevMix; }
    float  lastIntensity() const { return mPrevIntensity; }
    const std::string& lastSourceName() const { return mPrevSource; }

private:
    void ensureTexture(unsigned w, unsigned h);
    void renderToTexture();

    ParamSpec        mSpec;
    Ogre::TexturePtr mTex;
    std::string      mTexName;

    // Last rendered state (used for dirty-tracking: re-render only on change).
    std::string mPrevSource;
    float       mPrevMix       = -1.0f;
    float       mPrevIntensity = -1.0f;
    unsigned    mPrevW = 0, mPrevH = 0;
    bool        mLastRendered = false;

    static int sCounter;
};

} // namespace bbfx
