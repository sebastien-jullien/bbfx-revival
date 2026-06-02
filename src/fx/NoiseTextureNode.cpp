#include "NoiseTextureNode.h"
#include <OgreTextureManager.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgrePixelFormat.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace bbfx {

int NoiseTextureNode::sCounter = 0;

int NoiseTextureNode::hash2D(int x, int y, int seed) {
    uint32_t h = static_cast<uint32_t>(x * 374761393u + y * 668265263u + seed * 982451653u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<int>(h);
}

float NoiseTextureNode::perlinNoise2D(float x, float y, int seed) {
    auto fade = [](float t) { return t * t * t * (t * (t * 6 - 15) + 10); };
    auto lerp = [](float a, float b, float t) { return a + t * (b - a); };
    auto grad = [seed](int xi, int yi, float dx, float dy) -> float {
        int h = hash2D(xi, yi, seed) & 7;
        const float gx[8] = { 1, -1,  0,  0,  1, -1,  1, -1 };
        const float gy[8] = { 0,  0,  1, -1,  1,  1, -1, -1 };
        return gx[h] * dx + gy[h] * dy;
    };
    int xi = (int)std::floor(x);
    int yi = (int)std::floor(y);
    float xf = x - xi;
    float yf = y - yi;
    float u = fade(xf);
    float v = fade(yf);
    float n00 = grad(xi, yi, xf, yf);
    float n10 = grad(xi + 1, yi, xf - 1, yf);
    float n01 = grad(xi, yi + 1, xf, yf - 1);
    float n11 = grad(xi + 1, yi + 1, xf - 1, yf - 1);
    float nx0 = lerp(n00, n10, u);
    float nx1 = lerp(n01, n11, u);
    return lerp(nx0, nx1, v) * 0.5f + 0.5f;  // [0,1]
}

float NoiseTextureNode::simplexNoise2D(float x, float y, int seed) {
    // N8 — vrai simplex 2D (Gustavson) au lieu d'un Perlin ×1.2. Gradients via hash2D.
    const float F2 = 0.3660254037844386f;  // (sqrt(3)-1)/2
    const float G2 = 0.21132486540518713f; // (3-sqrt(3))/6
    float s = (x + y) * F2;
    int i = (int)std::floor(x + s);
    int j = (int)std::floor(y + s);
    float t = (i + j) * G2;
    float X0 = i - t, Y0 = j - t;
    float x0 = x - X0, y0 = y - Y0;
    int i1, j1;
    if (x0 > y0) { i1 = 1; j1 = 0; } else { i1 = 0; j1 = 1; }
    float x1 = x0 - i1 + G2,        y1 = y0 - j1 + G2;
    float x2 = x0 - 1.0f + 2.0f*G2, y2 = y0 - 1.0f + 2.0f*G2;
    auto grad = [seed](int xi, int yi, float dx, float dy) -> float {
        int h = hash2D(xi, yi, seed) & 7;
        const float gx[8] = { 1, -1,  0,  0,  1, -1,  1, -1 };
        const float gy[8] = { 0,  0,  1, -1,  1,  1, -1, -1 };
        return gx[h] * dx + gy[h] * dy;
    };
    auto corner = [&](float xx, float yy, int ii, int jj) -> float {
        float tt = 0.5f - xx*xx - yy*yy;
        if (tt < 0.0f) return 0.0f;
        tt *= tt;
        return tt * tt * grad(ii, jj, xx, yy);
    };
    float n = corner(x0, y0, i, j)
            + corner(x1, y1, i + i1, j + j1)
            + corner(x2, y2, i + 1, j + 1);
    return 70.0f * n * 0.5f + 0.5f; // ~[0,1]
}

float NoiseTextureNode::worleyNoise2D(float x, float y, int seed) {
    int xi = (int)std::floor(x);
    int yi = (int)std::floor(y);
    float minDist = 1e9f;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int cx = xi + dx;
            int cy = yi + dy;
            int h = hash2D(cx, cy, seed);
            float fx = (float)((h & 0xFFFF) / 65535.0f);
            float fy = (float)(((h >> 16) & 0xFFFF) / 65535.0f);
            float px = cx + fx;
            float py = cy + fy;
            float ddx = px - x;
            float ddy = py - y;
            float d2 = ddx * ddx + ddy * ddy;
            if (d2 < minDist) minDist = d2;
        }
    }
    return std::min(1.0f, std::sqrt(minDist));
}

float NoiseTextureNode::voronoiNoise2D(float x, float y, int seed) {
    int xi = (int)std::floor(x);
    int yi = (int)std::floor(y);
    int closestCellHash = 0;
    float minDist = 1e9f;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int cx = xi + dx;
            int cy = yi + dy;
            int h = hash2D(cx, cy, seed);
            float fx = (float)((h & 0xFFFF) / 65535.0f);
            float fy = (float)(((h >> 16) & 0xFFFF) / 65535.0f);
            float px = cx + fx;
            float py = cy + fy;
            float ddx = px - x;
            float ddy = py - y;
            float d2 = ddx * ddx + ddy * ddy;
            if (d2 < minDist) { minDist = d2; closestCellHash = h; }
        }
    }
    return (float)((closestCellHash & 0xFFFF) / 65535.0f); // colored by cell id
}

NoiseTextureNode::NoiseTextureNode(const std::string& name)
    : AnimationNode(name)
{
    sCounter++;
    mTexName = "NoiseTexture/" + std::to_string(sCounter) + "/" + getName();

    ParamDef typeDef;
    typeDef.name = "noise_type"; typeDef.label = "Type"; typeDef.type = ParamType::ENUM;
    typeDef.stringVal = "perlin";
    typeDef.choices = {"perlin", "worley", "simplex", "voronoi"};
    mSpec.addParam(typeDef);

    ParamDef resDef;
    resDef.name = "resolution"; resDef.label = "Resolution";
    resDef.type = ParamType::INT;
    resDef.intVal = 256; resDef.minVal = 64; resDef.maxVal = 2048;
    mSpec.addParam(resDef);

    ParamDef scaleDef;
    scaleDef.name = "scale"; scaleDef.label = "Scale";
    scaleDef.type = ParamType::FLOAT;
    scaleDef.floatVal = 4.0f; scaleDef.minVal = 0.1f; scaleDef.maxVal = 32.0f;
    mSpec.addParam(scaleDef);

    ParamDef octDef;
    octDef.name = "octaves"; octDef.label = "Octaves";
    octDef.type = ParamType::INT;
    octDef.intVal = 4; octDef.minVal = 1; octDef.maxVal = 8;
    mSpec.addParam(octDef);

    ParamDef seedDef;
    seedDef.name = "seed"; seedDef.label = "Seed";
    seedDef.type = ParamType::INT;
    seedDef.intVal = 0; seedDef.minVal = 0; seedDef.maxVal = 100000;
    mSpec.addParam(seedDef);

    // N8 — lacunarity/persistence du fBm exposés (avant : membres hardcodés 2.0/0.5).
    ParamDef lacDef;
    lacDef.name = "lacunarity"; lacDef.label = "Lacunarity";
    lacDef.type = ParamType::FLOAT;
    lacDef.floatVal = 2.0f; lacDef.minVal = 1.0f; lacDef.maxVal = 4.0f;
    mSpec.addParam(lacDef);

    ParamDef perDef;
    perDef.name = "persistence"; perDef.label = "Persistence";
    perDef.type = ParamType::FLOAT;
    perDef.floatVal = 0.5f; perDef.minVal = 0.1f; perDef.maxVal = 0.9f;
    mSpec.addParam(perDef);

    // v3.5.2 cleanup post-Sprint S5: expose the RTT name so MaterialBridgeNode
    // (Lot T) can pull it dynamically via `material_source` port. Pattern aligned
    // with GrayscaleNode (Lot U).
    ParamDef outDef;
    outDef.name = "texture_out"; outDef.label = "Texture (out)";
    outDef.type = ParamType::STRING;
    outDef.stringVal = mTexName;
    outDef.readOnly = true;
    outDef.tooltip = "Read-only: name of the procedural noise RTT. Routable to MaterialBridgeNode auto-wrap.";
    mSpec.addParam(outDef);

    setParamSpec(&mSpec);

    addInput(new AnimationPort("time_offset",   0.0f));
    addInput(new AnimationPort("displacement_x", 0.0f));
    addInput(new AnimationPort("displacement_y", 0.0f));

    // v3.5.2 Sprint S7 Lot AA — texture_ready output (border pulse green when active).
    addOutput(new AnimationPort("texture_ready", 0.0f));

    // v3.5.2 Sprint S7 Lot Z — port tooltips
    setPortTooltip("time_offset",   "Adds to the noise time-axis (wave / scroll). Animatable for evolution effects.");
    setPortTooltip("displacement_x","Adds to the noise X coordinate (pans the noise field horizontally).");
    setPortTooltip("displacement_y","Adds to the noise Y coordinate (pans the noise field vertically).");
    setPortTooltip("texture_ready","Pulses 1.0 each frame the noise texture is re-rendered (0.0 if static).");
}

NoiseTextureNode::~NoiseTextureNode() {
    cleanup();
}

void NoiseTextureNode::ensureTexture() {
    if (!mTex.isNull() && (int)mTex->getWidth() == mResolution) return;
    if (!mTex.isNull()) {
        Ogre::TextureManager::getSingleton().remove(mTexName);
        mTex.setNull();
    }
    mTex = Ogre::TextureManager::getSingleton().createManual(
        mTexName,
        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
        Ogre::TEX_TYPE_2D,
        mResolution, mResolution, 0,
        Ogre::PF_BYTE_RGBA,
        Ogre::TU_DYNAMIC_WRITE_ONLY_DISCARDABLE);
}

void NoiseTextureNode::renderToTexture(float timeOffset, float dispX, float dispY) {
    if (mTex.isNull()) return;
    auto buf = mTex->getBuffer();
    buf->lock(Ogre::HardwareBuffer::HBL_DISCARD);
    const auto& pb = buf->getCurrentLock();
    auto* dst = static_cast<uint8_t*>(pb.data);
    for (int y = 0; y < mResolution; ++y) {
        uint8_t* row = dst + y * pb.rowPitch * 4; // bytes per row
        for (int x = 0; x < mResolution; ++x) {
            float u = (x + dispX * mResolution) * mScale / mResolution;
            float v = (y + dispY * mResolution) * mScale / mResolution + timeOffset;
            float val = 0.0f;
            switch (mNoiseType) {
            case NoiseType::Perlin: {
                float amp = 1.0f, freq = 1.0f, totalAmp = 0.0f;
                for (int o = 0; o < mOctaves; ++o) {
                    val += amp * perlinNoise2D(u * freq, v * freq, mSeed + o);
                    totalAmp += amp;
                    amp *= mPersistence;
                    freq *= mLacunarity;
                }
                if (totalAmp > 0.0f) val /= totalAmp;
                break;
            }
            case NoiseType::Worley:
                val = worleyNoise2D(u, v, mSeed);
                break;
            case NoiseType::Simplex: {
                // N8 — vrai simplex avec fBm (octaves + lacunarity/persistence).
                float amp = 1.0f, freq = 1.0f, totalAmp = 0.0f;
                for (int o = 0; o < mOctaves; ++o) {
                    val += amp * simplexNoise2D(u * freq, v * freq, mSeed + 7 + o);
                    totalAmp += amp;
                    amp *= mPersistence;
                    freq *= mLacunarity;
                }
                if (totalAmp > 0.0f) val /= totalAmp;
                break;
            }
            case NoiseType::Voronoi:
                val = voronoiNoise2D(u, v, mSeed);
                break;
            }
            uint8_t b = static_cast<uint8_t>(std::clamp(val * 255.0f, 0.0f, 255.0f));
            row[x * 4 + 0] = b;
            row[x * 4 + 1] = b;
            row[x * 4 + 2] = b;
            row[x * 4 + 3] = 255;
        }
    }
    buf->unlock();
}

void NoiseTextureNode::update() {
    if (!mEnabled) return;

    auto* tParam = mSpec.getParam("noise_type");
    if (tParam) {
        if      (tParam->stringVal == "worley")  mNoiseType = NoiseType::Worley;
        else if (tParam->stringVal == "simplex") mNoiseType = NoiseType::Simplex;
        else if (tParam->stringVal == "voronoi") mNoiseType = NoiseType::Voronoi;
        else                                      mNoiseType = NoiseType::Perlin;
    }
    auto* rParam = mSpec.getParam("resolution");
    if (rParam) mResolution = std::clamp(rParam->intVal, 64, 2048);
    auto* sParam = mSpec.getParam("scale");
    if (sParam) mScale = sParam->floatVal;
    auto* oParam = mSpec.getParam("octaves");
    if (oParam) mOctaves = std::clamp(oParam->intVal, 1, 8);
    auto* seedParam = mSpec.getParam("seed");
    if (seedParam) mSeed = seedParam->intVal;
    if (auto* lacP = mSpec.getParam("lacunarity"))  mLacunarity  = std::clamp(lacP->floatVal, 1.0f, 4.0f);
    if (auto* perP = mSpec.getParam("persistence")) mPersistence = std::clamp(perP->floatVal, 0.1f, 0.9f);

    ensureTexture();

    auto& in = getInputs();
    auto getf = [&in](const char* n, float d) {
        auto it = in.find(n);
        return (it != in.end()) ? it->second->getValue() : d;
    };
    float to = getf("time_offset", 0.0f);
    float dx = getf("displacement_x", 0.0f);
    float dy = getf("displacement_y", 0.0f);

    bool params_changed = (mPrevType != mNoiseType
                        || mPrevResolution != mResolution
                        || mPrevScale != mScale
                        || mPrevOctaves != mOctaves
                        || mPrevSeed != mSeed
                        || mPrevLacunarity != mLacunarity
                        || mPrevPersistence != mPersistence);
    bool anim_changed = (mPrevTimeOffset != to || mPrevDispX != dx || mPrevDispY != dy);

    bool re_rendered = false;
    if (params_changed || anim_changed) {
        renderToTexture(to, dx, dy);
        mPrevType = mNoiseType;
        mPrevResolution = mResolution;
        mPrevScale = mScale;
        mPrevOctaves = mOctaves;
        mPrevSeed = mSeed;
        mPrevLacunarity = mLacunarity;
        mPrevPersistence = mPersistence;
        mPrevTimeOffset = to;
        mPrevDispX = dx;
        mPrevDispY = dy;
        re_rendered = true;
    }

    // Refresh `texture_out` mirror (constant value, but kept in sync defensively
    // — covers any future scenario where mTexName might rotate).
    if (auto* outMir = mSpec.getParam("texture_out")) outMir->stringVal = mTexName;

    // v3.5.2 Sprint S7 Lot AA — pulse texture_ready 1.0 when texture re-rendered.
    auto& outs = getOutputs();
    if (auto it = outs.find("texture_ready"); it != outs.end()) {
        it->second->setValue(re_rendered ? 1.0f : 0.0f);
    }

    fireUpdate();
}

void NoiseTextureNode::cleanup() {
    if (!mTex.isNull()) {
        Ogre::TextureManager::getSingleton().remove(mTexName);
        mTex.setNull();
    }
}

} // namespace bbfx
