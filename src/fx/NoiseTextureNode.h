#pragma once

#include "../core/AnimationNode.h"
#include "../core/ParamSpec.h"
#include <OgreTexture.h>
#include <string>
#include <vector>

namespace bbfx {

/// NoiseTextureNode — generates a procedural noise texture (Perlin / Worley /
/// Simplex / Voronoi) into a managed RTT-style texture. The texture is filled
/// CPU-side (a follow-up iteration can swap to a GPU shader path).
class NoiseTextureNode : public AnimationNode {
public:
    enum class NoiseType { Perlin, Worley, Simplex, Voronoi };

    explicit NoiseTextureNode(const std::string& name);
    ~NoiseTextureNode() override;

    void update() override;
    void cleanup() override;
    std::string getTypeName() const override { return "NoiseTextureNode"; }

    Ogre::TexturePtr getTexture() const { return mTex; }
    const std::string& getTextureName() const { return mTexName; }
    NoiseType getNoiseType() const { return mNoiseType; }

    // N8 — exposé public pour testabilité (fonction pure déterministe).
    static float simplexNoise2D(float x, float y, int seed);

private:
    ParamSpec mSpec;
    Ogre::TexturePtr mTex;
    std::string mTexName;

    NoiseType mNoiseType = NoiseType::Perlin;
    int   mResolution = 256;
    float mScale       = 4.0f;
    int   mOctaves     = 4;
    float mLacunarity  = 2.0f;
    float mPersistence = 0.5f;
    int   mSeed        = 0;

    NoiseType mPrevType = static_cast<NoiseType>(-1);
    int       mPrevResolution = -1;
    float     mPrevScale = -1, mPrevTimeOffset = -999, mPrevDispX = -999, mPrevDispY = -999;
    int       mPrevOctaves = -1;
    int       mPrevSeed = -1;
    float     mPrevLacunarity = -1, mPrevPersistence = -1; // N8 — dirty-tracking
    static int sCounter;

    void ensureTexture();
    void renderToTexture(float timeOffset, float dispX, float dispY);
    static float perlinNoise2D(float x, float y, int seed);
    static float worleyNoise2D(float x, float y, int seed);
    static float voronoiNoise2D(float x, float y, int seed);
    static int   hash2D(int x, int y, int seed);
};

} // namespace bbfx
