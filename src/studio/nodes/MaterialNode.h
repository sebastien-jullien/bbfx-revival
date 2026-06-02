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
///
/// v3.5.2 Sprint S8 Lot AD — migrated on the Pattern 4 cascade `mApplySeq`
/// counter (CLAUDE.md DAG Patterns). Multiple Material/Texture/MaterialBridge
/// nodes targeting the same SceneObjectNode now have deterministic priority :
/// the last-connected (highest cascade seq snapshot) wins ; on detach/disable,
/// peers with a higher seq retain priority. Lot AU.24 — counter is now shared
/// via bbfx::nextCascadeApplySeq() so the order is well-defined across classes.
class MaterialNode : public AnimationNode {
public:
    MaterialNode(const std::string& name);
    ~MaterialNode() override = default;
    void update() override;
    void cleanup() override;
    void setEnabled(bool en) override;
    void onLinkChanged() override;
    std::string getTypeName() const override { return "MaterialNode"; }

    // Sprint S8 Lot AD — cascade introspection (used by peer Texture/Material/
    // Bridge nodes' detachFromEntity to compare priority).
    unsigned getApplySeq(const std::string& target) const {
        auto it = mApplySeq.find(target);
        return it != mApplySeq.end() ? it->second : 0;
    }
    const std::map<std::string, std::vector<std::string>>& getOriginalMaterials() const { return mOriginalMaterials; }
    const std::vector<std::string>& getCurrentTargets() const { return mCurrentTargets; }

    // Lot AU.24 — public so a peer Texture/Material/MaterialBridge node can
    // request a re-apply when this node becomes the cascade predecessor after
    // a more-recent peer is detached (Pattern 4 hand-back). Idempotent.
    void applyToEntity(const std::string& targetName);

private:
    void resolveTargets();
    void detachFromEntity(const std::string& targetName);

    ParamSpec mSpec;
    std::string mMaterialName;
    std::map<std::string, std::vector<std::string>> mOriginalMaterials;
    std::vector<std::string> mCurrentTargets;
    std::map<std::string, unsigned> mApplySeq;   // Sprint S8 Lot AD — Pattern 4 cascade snapshot per target
    bool mReenabling = false;
};

} // namespace bbfx
