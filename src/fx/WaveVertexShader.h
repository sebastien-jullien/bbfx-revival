#pragma once

#include "SoftwareVertexShader.h"
#include "../core/AnimationNode.h"
#include <vector>

namespace bbfx {

using namespace Ogre;

class WaveVertexShader : public SoftwareVertexShader, public AnimationNode {
public:
    WaveVertexShader(const String& meshName, const String& cloneName);
    virtual ~WaveVertexShader();
    void renderOneFrame(Real dt) override;
    void update() override;
    void cleanup() override;

    void setStudioNames(const std::string& entName, const std::string& snName) {
        mStudioEntityName = entName;
        mStudioSceneNodeName = snName;
    }

    float getAmplitude() const { return amplitude; }
    void setAmplitude(float v) { amplitude = v; }
    float getFrequency() const { return frequency; }
    void setFrequency(float v) { frequency = v; }
    float getSpeed() const { return speed; }
    void setSpeed(float v) { speed = v; }
    int getAxis() const { return axis; }
    void setAxis(int v) { axis = v; }

protected:
    float time = 0.0f;
    float amplitude = 5.0f;
    float frequency = 2.0f;
    float speed = 1.0f;
    int axis = 1; // 0=X, 1=Y, 2=Z

    // Pre-allocated buffers
    std::vector<float> mDstPos;
    std::vector<float> mNormals;

    // Deferred entity creation (same pattern as PerlinFxNode)
    std::string mStudioEntityName;
    std::string mStudioSceneNodeName;
    std::string mCloneMeshName;
    bool mEntityCreated = false;

    void createDeferredEntity();

    void _applyWave(VertexData* data, const CpuMeshData& cpuData);
};

} // namespace bbfx
