#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
namespace bbfx {
class SplitterNode : public AnimationNode {
public:
    explicit SplitterNode(const std::string& name);
    void update() override;
    std::string getTypeName() const override { return "SplitterNode"; }
private:
    ParamSpec mSpec;
    int mNumOutputs = 4;
};
} // namespace bbfx
