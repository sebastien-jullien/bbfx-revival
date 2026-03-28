#pragma once

#include "../core/AnimationNode.h"
#include "PerlinVertexShader.h"
#include <memory>

namespace bbfx {

class PerlinFxNode : public AnimationNode {
public:
    PerlinFxNode(const string& meshName, const string& cloneName);
    virtual ~PerlinFxNode();
    void update() override;
    std::string getTypeName() const override { return "PerlinFxNode"; }
    void enable();
    void disable();

private:
    std::unique_ptr<PerlinVertexShader> mShader;
};

} // namespace bbfx
