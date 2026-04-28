#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/Animator.h"
#include "../../core/ParamSpec.h"
#include <OgreSceneManager.h>
#include <string>
#include <map>
#include <vector>

namespace bbfx {

/// TextureNode — applies a texture to linked SceneObjectNodes via entity port.
/// Follows the same entity-link pattern as ShaderFxNode/ParticleNode.
/// Suppressing the link restores the original materials (per sub-entity).
class TextureNode : public AnimationNode {
public:
    TextureNode(const std::string& name);
    ~TextureNode() override = default;
    void update() override;
    void cleanup() override;
    void setEnabled(bool en) override;
    void onLinkChanged() override;
    std::string getTypeName() const override { return "TextureNode"; }

    // Access to saved originals (for cascade texture restoration)
    const std::map<std::string, std::vector<std::string>>& getOriginalMaterials() const { return mOriginalMaterials; }
    const std::vector<std::string>& getCurrentTargets() const { return mCurrentTargets; }
    unsigned getApplySeq(const std::string& target) const {
        auto it = mApplySeq.find(target);
        return it != mApplySeq.end() ? it->second : 0;
    }

private:
    void resolveTargets();
    void applyToEntity(const std::string& targetName);
    void detachFromEntity(const std::string& targetName);

    ParamSpec mSpec;
    std::string mTextureName;
    std::string mLightingMode = "lit";
    std::map<std::string, std::vector<std::string>> mOriginalMaterials;
    std::vector<std::string> mCurrentTargets;
    std::map<std::string, unsigned> mApplySeq; // connection order per target (persists across enable/disable)
    bool mReenabling = false; // set on re-enable, cleared after resolveTargets

    static unsigned sApplySeqCounter; // global monotonic counter for connection ordering
};

} // namespace bbfx
