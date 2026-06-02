#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/Animator.h"
#include "../../core/ParamSpec.h"
#include <string>
#include <map>
#include <set>
#include <vector>

namespace bbfx {

/// MaterialBridgeNode — universal material/texture → mesh router (Lot T, v3.5.2 S5).
///
/// Bridges any `material_out` mirror produced by upstream nodes (TextureBlendNode,
/// VideoCrossfadeNode, NoiseTextureNode, SpectrogramTextureNode, VideoSlicerNode,
/// TheoraClipNode post-Lot V) onto a SceneObjectNode 3D mesh.
///
/// Two input modes:
///   1. Static ParamSpec `material_in` — set via `dbg.set_param`.
///   2. Dynamic `material_source` port — linked to an upstream node; this node
///      polls the upstream's ParamSpec `material_out` mirror each update().
///
/// Auto-wrap: if `material_in` is a TEXTURE name (not a Material), wraps it
/// in `MatBridge_<node>_<tex>_<lighting>` with a single TextureUnitState.
///
/// Pattern is identical to TextureNode/MaterialNode (resolveTargets +
/// applyToEntity + detachFromEntity + cascade via mApplySeq).
class MaterialBridgeNode : public AnimationNode {
public:
    MaterialBridgeNode(const std::string& name);
    ~MaterialBridgeNode() override = default;
    void update() override;
    void cleanup() override;
    void setEnabled(bool en) override;
    void onLinkChanged() override;
    std::string getTypeName() const override { return "MaterialBridgeNode"; }

    // Public accessors used by other Texture/Material/Bridge nodes for cascade
    // restoration (find real originals when this node has already wrapped them).
    const std::map<std::string, std::vector<std::string>>& getOriginalMaterials() const { return mOriginalMaterials; }
    const std::vector<std::string>& getCurrentTargets() const { return mCurrentTargets; }
    unsigned getApplySeq(const std::string& target) const {
        auto it = mApplySeq.find(target);
        return it != mApplySeq.end() ? it->second : 0;
    }

    // Lot AU.24 — public so a peer Texture/Material/MaterialBridge node can
    // request a re-apply when this node becomes the cascade predecessor after
    // a more-recent peer is detached (Pattern 4 hand-back). Idempotent.
    void applyToEntity(const std::string& targetName);

private:
    void resolveTargets();
    void detachFromEntity(const std::string& targetName);

    /// Returns the effective `material_in` value, considering the `material_source`
    /// dynamic input first (if linked), then falling back to the static ParamSpec.
    std::string resolveMaterialIn() const;

    /// True if `name` is a known OGRE Material (false if it's a texture or unknown).
    static bool isMaterial(const std::string& name);
    /// True if `name` is a known OGRE Texture.
    static bool isTexture(const std::string& name);

    /// Build (or fetch from cache) the auto-wrap material for a texture name.
    /// Format: `MatBridge_<nodeName>_<texName>_<lighting>`.
    std::string buildAutoWrapMaterial(const std::string& texName,
                                      const std::string& lightingMode);

    ParamSpec mSpec;
    std::string mLastResolvedMaterial;          // last value applied (cache)
    std::string mLastLightingMode;              // last lighting mode (for invalidation)
    std::map<std::string, std::vector<std::string>> mOriginalMaterials;
    std::vector<std::string> mCurrentTargets;
    std::map<std::string, unsigned> mApplySeq;  // connection-order counter per target
    bool mReenabling = false;
    std::set<std::string> mCreatedMaterials;    // N7 — matériaux MatBridge_* créés, à remove() au cleanup
};

} // namespace bbfx
