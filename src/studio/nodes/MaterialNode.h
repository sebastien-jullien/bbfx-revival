#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/Animator.h"
#include "../../core/ParamSpec.h"
#include <string>
#include <map>
#include <vector>

namespace bbfx {

/// MaterialNode — applies a named OGRE material to linked SceneObjectNodes via entity port.
/// Same entity-link pattern as TextureNode/ShaderFxNode/ParticleNode.
/// Suppressing the link restores the original materials (per sub-entity).
class MaterialNode : public AnimationNode {
public:
    MaterialNode(const std::string& name);
    ~MaterialNode() override = default;
    void update() override;
    void cleanup() override;
    void setEnabled(bool en) override;
    void onLinkChanged() override;
    std::string getTypeName() const override { return "MaterialNode"; }

private:
    void resolveTargets();
    void applyToEntity(const std::string& targetName);
    void detachFromEntity(const std::string& targetName);

    ParamSpec mSpec;
    std::string mMaterialName;
    std::map<std::string, std::vector<std::string>> mOriginalMaterials;
    std::vector<std::string> mCurrentTargets;
};

} // namespace bbfx
