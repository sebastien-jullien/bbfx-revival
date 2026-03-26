#pragma once

#include "SoftwareVertexShader.h"

namespace bbfx {

using namespace Ogre;

class PerlinVertexShader : public SoftwareVertexShader {
public:
    PerlinVertexShader(const String& meshName, const String& cloneName);
    virtual ~PerlinVertexShader();
    void renderOneFrame(Real dt) override;

    float getDisplacement() const { return displacement; }
    void setDisplacement(float v) { displacement = v; }
    float getDensity() const { return density; }
    void setDensity(float v) { density = v; }
    float getTimeDensity() const { return timeDensity; }
    void setTimeDensity(float v) { timeDensity = v; }

protected:
    float time = 0.0f;
    float displacement = 0.1f;
    float density = 50.0f;
    float timeDensity = 5.0f;

    // Pre-allocated buffers to avoid per-frame heap allocations
    std::vector<float> mDstPos;
    std::vector<float> mNormals;

    void _applyNoise(VertexData* data, const CpuMeshData& cpuData);
};

} // namespace bbfx
