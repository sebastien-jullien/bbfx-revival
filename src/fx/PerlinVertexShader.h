#pragma once

#include "SoftwareVertexShader.h"
#include <OgreController.h>

namespace bbfx {

class PerlinVertexShader : public SoftwareVertexShader {
public:
    explicit PerlinVertexShader(const string& meshName);
    virtual ~PerlinVertexShader();
    void renderOneFrame(float dt) override;

protected:
    float mTime = 0.0f;
    float mDisplacement = 0.1f;
    float mDensity = 4.0f;
    float mTimeDensity = 2.0f;
};

} // namespace bbfx
