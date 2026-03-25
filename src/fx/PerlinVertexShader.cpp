#include "PerlinVertexShader.h"

namespace bbfx {

PerlinVertexShader::PerlinVertexShader(const string& meshName)
    : SoftwareVertexShader(meshName) {}

PerlinVertexShader::~PerlinVertexShader() = default;

void PerlinVertexShader::renderOneFrame(float dt) {
    mTime += dt * mTimeDensity;
    // Perlin noise deformation would be applied here
    // Stub: actual vertex manipulation requires loaded mesh data
}

} // namespace bbfx
