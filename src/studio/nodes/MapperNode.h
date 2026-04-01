#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
namespace bbfx {
class MapperNode : public AnimationNode {
public:
    explicit MapperNode(const std::string& name);
    void update() override;
    std::string getTypeName() const override { return "MapperNode"; }
private:
    ParamSpec mSpec;
};
} // namespace bbfx
