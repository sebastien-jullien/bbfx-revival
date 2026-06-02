#pragma once

#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include "../../video/TextureCrossfader.h"
#include <memory>
#include <string>

namespace bbfx {

class TheoraClipNode; // forward

/// VideoCrossfadeNode — DAG wrapper around `bbfx::TextureCrossfader` that
/// produces a single material smoothly blending the dynamic textures of two
/// linked TheoraClipNode sources. Reproduction of the BBFx 2006
/// `Video:crossfade(clipName1, clipName2)` pattern (cf. video.lua:39-44).
class VideoCrossfadeNode : public AnimationNode {
public:
    explicit VideoCrossfadeNode(const std::string& name);
    ~VideoCrossfadeNode() override = default;

    void update() override;
    void cleanup() override;
    std::string getTypeName() const override { return "VideoCrossfadeNode"; }

    const std::string& getGeneratedMaterialName() const { return mGeneratedMaterialName; }
    float getCurrentBeta() const { return mPrevBeta; }

    /// Test-only: force the crossfader to be (re)built with arbitrary texture names,
    /// bypassing the TheoraClipNode resolution. Used by dbg.test() since the bundled
    /// videos may produce shared/empty texture names that prevent the live path from
    /// firing. Real production code uses the regular update() flow.
    void _buildWithMockTextures(const std::string& texA, const std::string& texB);

private:
    ParamSpec mSpec;
    std::unique_ptr<TextureCrossfader> mCrossfader;
    std::string mGeneratedMaterialName;
    std::string mPrevTexA, mPrevTexB;
    float mPrevBeta = -1.0f; // -1 forces first apply
    float mAccumulatedTime = 0.0f;
    float mPrevDt = 1.0f / 60.0f;

    static int sCounter;

    AnimationNode* resolveClipNode(const std::string& portName);
};

} // namespace bbfx
