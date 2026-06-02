#pragma once

#include "../core/AnimationNode.h"
#include "../core/ParamSpec.h"
#include <OgreTexture.h>
#include <string>
#include <vector>

namespace bbfx {

/// SpectrogramTextureNode — produces a 2D RTT containing a scrolling waterfall
/// spectrum of the audio source. Each frame writes the latest column on the
/// right, shifts older columns left. Colormap LUT is applied CPU-side.
///
/// Approach: when an AudioCaptureNode is connected via the `audio` entity-link
/// port, the node reads its sample buffer and computes a simplified magnitude
/// spectrum (sliding window energy bands). When no source is connected, a
/// synthetic test signal is generated based on `time_offset` (useful for
/// dbg.test() automation).
class SpectrogramTextureNode : public AnimationNode {
public:
    enum class FreqScale { Linear, Log, Mel };
    enum class IntensityScale { Linear, Log, Db };
    enum class Colormap { Grayscale, Viridis, Plasma, Magma };

    explicit SpectrogramTextureNode(const std::string& name);
    ~SpectrogramTextureNode() override;

    void update() override;
    void cleanup() override;
    std::string getTypeName() const override { return "SpectrogramTextureNode"; }

    Ogre::TexturePtr getTexture() const { return mTex; }
    const std::string& getTextureName() const { return mTexName; }
    int getScrollOffset() const { return mScrollOffset; }
    Colormap getColormap() const { return mColormap; }

private:
    ParamSpec mSpec;
    Ogre::TexturePtr mTex;
    std::string mTexName;

    int  mWidth = 256;
    int  mHeight = 128;
    FreqScale mFreqScale = FreqScale::Log;
    IntensityScale mIntensityScale = IntensityScale::Db;
    Colormap mColormap = Colormap::Grayscale;
    int  mScrollOffset = 0;

    // v3.5.2 final: per-instance ring-buffer shadow (replaces thread_local).
    // Layout: mShadow[row][ringSlot]. Write pointer = mScrollOffset.
    std::vector<std::vector<float>> mShadow;

    static int sCounter;

    void ensureTexture();
    void writeColumn(int colX, const std::vector<float>& bins);
    static void applyColormap(float intensity, Colormap cm,
                              uint8_t& r, uint8_t& g, uint8_t& b);
    AnimationNode* resolveAudio();
    void buildSyntheticBins(int height, std::vector<float>& bins, float timeOffset);
};

} // namespace bbfx
