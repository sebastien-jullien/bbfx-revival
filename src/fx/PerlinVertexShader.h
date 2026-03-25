#pragma once

#include "SoftwareVertexShader.h"

namespace bbfx {

using namespace Ogre;

class PerlinVertexShader : public SoftwareVertexShader {
public:
    PerlinVertexShader(const String& meshName, const String& cloneName);
    virtual ~PerlinVertexShader();
    void renderOneFrame(Real dt) override;

protected:
    float time = 0.0f;
    float displacement = 0.1f;
    float density = 50.0f;
    float timeDensity = 5.0f;

    float* _clearNormals(VertexData* data);
    void _normalizeNormals(VertexData* data, float* normals);
    void _applyNoise(VertexData* data, const CpuMeshData& cpuData, float* normals);
};

} // namespace bbfx
