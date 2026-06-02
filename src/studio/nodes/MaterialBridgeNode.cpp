#include "MaterialBridgeNode.h"
#include "SceneObjectNode.h"
#include "TextureNode.h"
#include "MaterialNode.h"
#include <OgreEntity.h>
#include <OgreSubEntity.h>
#include <OgreMaterialManager.h>
#include <OgreTextureManager.h>
#include <OgreMaterial.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreTextureUnitState.h>
#include <iostream>

namespace bbfx {

MaterialBridgeNode::MaterialBridgeNode(const std::string& name)
    : AnimationNode(name)
{
    // Entity port — links to the SceneObjectNode(s) we apply onto.
    addInput(new AnimationPort("entity", 0.0f, true));
    // Material source port — entity-link to an upstream node whose ParamSpec
    // exposes a `material_out` mirror string (TextureBlend, VideoCrossfade, etc.).
    addInput(new AnimationPort("material_source", 0.0f, true));

    ParamDef matInDef;
    matInDef.name = "material_in";
    matInDef.label = "Material (in, static)";
    matInDef.type = ParamType::STRING;
    matInDef.stringVal = "";
    matInDef.tooltip = "Material name to apply on linked SceneObjectNode. "
                       "If a texture name is given instead of a material, an auto-wrap "
                       "material 'MatBridge_<node>_<tex>_<lighting>' is generated.";
    mSpec.addParam(matInDef);

    ParamDef tgtDef;
    tgtDef.name = "target_entity";
    tgtDef.label = "Target Entity (mirror)";
    tgtDef.type = ParamType::STRING;
    tgtDef.stringVal = "";
    tgtDef.readOnly = true;
    tgtDef.tooltip = "Read-only: name of the first SceneObjectNode connected via 'entity' port.";
    mSpec.addParam(tgtDef);

    ParamDef lightDef;
    lightDef.name = "lighting_mode";
    lightDef.label = "Lighting";
    lightDef.type = ParamType::ENUM;
    lightDef.stringVal = "unlit";
    lightDef.choices = {"unlit", "lit", "emissive"};
    lightDef.tooltip = "unlit = pure texture (ideal for video/RTT). "
                       "lit = diffuse + ambient enabled. "
                       "emissive = self-illumination = (1,1,1) (glows in dark).";
    mSpec.addParam(lightDef);

    setParamSpec(&mSpec);

    // Sprint S6 Lot W + S8 Lot AC: port tooltips for InspectorPanel hover help.
    setPortTooltip("entity",
        "Link to a SceneObjectNode (mesh sub-entities), FullscreenOverlayNode (camera_locked / screen_aligned), "
        "or BillboardLayerNode (3D billboard) target. The resolved `material_in` is applied to the target.");
    setPortTooltip("material_source",
        "Optional dynamic input: link to any node exposing a 'material_out' / 'texture_out' / "
        "'current_texture' mirror. Pulled each frame, overrides static material_in.");
}

// ── Helpers ──────────────────────────────────────────────────────────────────

bool MaterialBridgeNode::isMaterial(const std::string& name) {
    if (name.empty()) return false;
    auto m = Ogre::MaterialManager::getSingleton().getByName(name);
    return m.get() != nullptr;
}

bool MaterialBridgeNode::isTexture(const std::string& name) {
    if (name.empty()) return false;
    auto t = Ogre::TextureManager::getSingleton().getByName(name);
    return t.get() != nullptr;
}

std::string MaterialBridgeNode::resolveMaterialIn() const {
    // 1) If `material_source` port is linked, pull from upstream's ParamSpec mirror.
    auto* animator = Animator::instance();
    if (animator) {
        auto& inputs = const_cast<MaterialBridgeNode*>(this)->getInputs();
        auto it = inputs.find("material_source");
        if (it != inputs.end()) {
            auto sources = animator->getSourceNodes(it->second);
            for (auto* src : sources) {
                if (!src) continue;
                auto* spec = src->getParamSpec();
                if (!spec) continue;
                // Probe well-known mirror names produced by v3.5.2 nodes.
                static const char* kMirrors[] = {
                    "material_out",     // TextureBlend, VideoCrossfade, VideoSlicer, TheoraClip(post-V)
                    "texture_out",      // GrayscaleNode (Lot U, post auto-wrap), NoiseTextureNode could expose
                    "texture",          // SpectrogramTextureNode, NoiseTextureNode (legacy mirror)
                    "current_texture",  // TextureCycleNode
                };
                for (const char* mname : kMirrors) {
                    auto* p = spec->getParam(mname);
                    if (p && !p->stringVal.empty()) return p->stringVal;
                }
            }
        }
    }
    // 2) Fallback to the static ParamSpec.
    auto* p = mSpec.getParam("material_in");
    return (p ? p->stringVal : std::string());
}

std::string MaterialBridgeNode::buildAutoWrapMaterial(const std::string& texName,
                                                      const std::string& lightingMode) {
    std::string matName = "MatBridge_" + getName() + "_" + texName + "_" + lightingMode;
    auto& mm = Ogre::MaterialManager::getSingleton();
    auto mat = mm.getByName(matName);
    if (!mat) {
        mCreatedMaterials.insert(matName); // N7 — pour remove() au cleanup
        mat = mm.create(matName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        auto* pass = mat->getTechnique(0)->getPass(0);
        pass->createTextureUnitState(texName);
        if (lightingMode == "unlit") {
            pass->setLightingEnabled(false);
        } else if (lightingMode == "emissive") {
            pass->setLightingEnabled(true);
            pass->setDiffuse(1, 1, 1, 1);
            pass->setAmbient(1.0f, 1.0f, 1.0f);
            pass->setSelfIllumination(1.0f, 1.0f, 1.0f);
        } else { // "lit"
            pass->setLightingEnabled(true);
            pass->setDiffuse(1, 1, 1, 1);
            pass->setAmbient(1.0f, 1.0f, 1.0f);
        }
    }
    return matName;
}

// ── DAG plumbing ─────────────────────────────────────────────────────────────

void MaterialBridgeNode::resolveTargets() {
    auto* animator = Animator::instance();
    if (!animator) return;

    auto& inputs = getInputs();
    auto it = inputs.find("entity");
    if (it == inputs.end()) return;

    auto sources = animator->getSourceNodes(it->second);
    std::vector<std::string> newTargets;
    for (auto* src : sources) {
        if (src) newTargets.push_back(src->getName());
    }

    // Detach from targets no longer linked
    for (auto& old : mCurrentTargets) {
        bool stillLinked = false;
        for (auto& n : newTargets) { if (n == old) { stillLinked = true; break; } }
        if (!stillLinked) {
            detachFromEntity(old);
            mApplySeq.erase(old);
        }
    }

    if (mEnabled) {
        for (auto& t : newTargets) {
            bool isNew = true;
            for (auto& old : mCurrentTargets) { if (old == t) { isNew = false; break; } }
            if (!isNew) continue;

            if (mReenabling) {
                unsigned mySeq = mApplySeq.count(t) ? mApplySeq[t] : 0;
                bool laterActive = false;
                for (auto& nodeName : animator->getRegisteredNodeNames()) {
                    auto* otherNode = animator->getRegisteredNode(nodeName);
                    if (!otherNode || otherNode == this || !otherNode->isEnabled()) continue;
                    const auto& tn = otherNode->getTypeName();
                    if (tn != "MaterialBridgeNode" && tn != "TextureNode" && tn != "MaterialNode") continue;
                    if (auto* mb = dynamic_cast<MaterialBridgeNode*>(otherNode)) {
                        for (auto& ot : mb->getCurrentTargets()) {
                            if (ot == t && mb->getApplySeq(t) > mySeq) { laterActive = true; break; }
                        }
                    } else if (auto* tx = dynamic_cast<TextureNode*>(otherNode)) {
                        for (auto& ot : tx->getCurrentTargets()) {
                            if (ot == t && tx->getApplySeq(t) > mySeq) { laterActive = true; break; }
                        }
                    }
                    if (laterActive) break;
                }
                if (laterActive) continue;
            } else {
                mApplySeq[t] = nextCascadeApplySeq();    // Lot AU.24 — shared cascade counter
            }

            applyToEntity(t);
        }
    }

    mCurrentTargets = newTargets;
    mReenabling = false;

    // Mirror first target name for quick UI inspection.
    auto* tgtMirror = mSpec.getParam("target_entity");
    if (tgtMirror) tgtMirror->stringVal = mCurrentTargets.empty() ? "" : mCurrentTargets.front();
}

void MaterialBridgeNode::applyToEntity(const std::string& targetName) {
    auto* animator = Animator::instance();
    if (!animator) return;

    auto* targetNode = animator->getRegisteredNode(targetName);
    if (!targetNode) return;

    std::string requested = resolveMaterialIn();
    if (requested.empty()) {
        // Empty input: caller should detach via update()'s guard, not here.
        return;
    }

    // v3.5.2 Sprint S8 Lot AC — multi-target support :
    // FullscreenOverlayNode + BillboardLayerNode also accepted as targets.
    // Implementation = write into target's ParamSpec `material` (with backup).
    const std::string& tn = targetNode->getTypeName();
    if (tn == "FullscreenOverlayNode" || tn == "BillboardLayerNode") {
        auto* spec = targetNode->getParamSpec();
        if (!spec) return;
        auto* matParam = spec->getParam("material");
        if (!matParam) return;

        // Compute the actual material to apply. Anything that isn't already an existing
        // material is treated as a TEXTURE name and auto-wrapped into a single-TUS material
        // — OGRE loads the texture on demand. (Don't gate on isTexture(): it returns false
        // for textures not yet declared/loaded, which used to make us set a bogus material
        // name → BaseWhite fallback on the target.)
        auto* lightParam = mSpec.getParam("lighting_mode");
        std::string lightMode = lightParam ? lightParam->stringVal : std::string("unlit");
        std::string toApply = isMaterial(requested) ? requested
                                                    : buildAutoWrapMaterial(requested, lightMode);

        // Backup original ParamSpec material once per target.
        if (mOriginalMaterials.find(targetName) == mOriginalMaterials.end()) {
            // Skip backing up materials that came from another bridge to avoid
            // restoring an intermediate wrap. Probe `MatBridge_` prefix.
            std::string backup = matParam->stringVal;
            if (backup.rfind("MatBridge_", 0) == 0 || backup.rfind("FullscreenOverlay/wrap/", 0) == 0
                || backup.rfind("BillboardLayer/wrap/", 0) == 0) {
                backup = "BaseWhite"; // safe default
            }
            mOriginalMaterials[targetName] = { backup };
        }

        matParam->stringVal = toApply;
        mLastResolvedMaterial = toApply;
        mLastLightingMode     = lightMode;
        return;
    }

    if (tn != "SceneObjectNode") return;

    auto* soNode = dynamic_cast<SceneObjectNode*>(targetNode);
    if (!soNode || !soNode->getEntity()) return;

    auto* entity = soNode->getEntity();

    // Save originals (skip materials that came from another Texture/Material/MaterialBridge node;
    // resolve back to the real originals via the upstream node that owns them).
    if (mOriginalMaterials.find(targetName) == mOriginalMaterials.end()) {
        std::vector<std::string> origMats;
        for (unsigned s = 0; s < entity->getNumSubEntities(); ++s) {
            std::string matN = entity->getSubEntity(s)->getMaterialName();
            const bool fromBridge = matN.rfind("MatBridge_", 0) == 0;
            const bool fromTexNode = matN.rfind("TexNode_",   0) == 0;
            if (fromBridge || fromTexNode) {
                // Find the real original from the upstream node's saved originals.
                for (auto& nn : animator->getRegisteredNodeNames()) {
                    auto* on = animator->getRegisteredNode(nn);
                    if (!on || on == this) continue;
                    if (auto* mb = dynamic_cast<MaterialBridgeNode*>(on)) {
                        auto oit = mb->getOriginalMaterials().find(targetName);
                        if (oit != mb->getOriginalMaterials().end() && s < oit->second.size()) {
                            matN = oit->second[s]; break;
                        }
                    } else if (auto* tx = dynamic_cast<TextureNode*>(on)) {
                        auto oit = tx->getOriginalMaterials().find(targetName);
                        if (oit != tx->getOriginalMaterials().end() && s < oit->second.size()) {
                            matN = oit->second[s]; break;
                        }
                    }
                }
            }
            origMats.push_back(matN);
        }
        mOriginalMaterials[targetName] = origMats;
    }

    // Compute the actual material to apply. Anything that isn't already an existing
    // material is treated as a TEXTURE name and auto-wrapped (OGRE loads it on demand).
    auto* lightParam = mSpec.getParam("lighting_mode");
    std::string lightMode = lightParam ? lightParam->stringVal : std::string("unlit");

    std::string toApply = isMaterial(requested) ? requested
                                                : buildAutoWrapMaterial(requested, lightMode);

    mLastResolvedMaterial = toApply;
    mLastLightingMode     = lightMode;

    for (unsigned s = 0; s < entity->getNumSubEntities(); ++s) {
        entity->getSubEntity(s)->setMaterialName(toApply);
    }
}

void MaterialBridgeNode::detachFromEntity(const std::string& targetName) {
    auto* animator = Animator::instance();
    if (!animator) return;

    auto* targetNode = animator->getRegisteredNode(targetName);
    if (!targetNode) return;

    // v3.5.2 Sprint S8 Lot AC — multi-target support : FullscreenOverlay +
    // BillboardLayer restore via ParamSpec write-back.
    const std::string& tn = targetNode->getTypeName();
    if (tn == "FullscreenOverlayNode" || tn == "BillboardLayerNode") {
        auto* spec = targetNode->getParamSpec();
        if (!spec) return;
        auto* matParam = spec->getParam("material");
        auto origIt = mOriginalMaterials.find(targetName);
        if (matParam && origIt != mOriginalMaterials.end() && !origIt->second.empty()) {
            matParam->stringVal = origIt->second.front();
        }
        if (origIt != mOriginalMaterials.end()) mOriginalMaterials.erase(origIt);
        return;
    }

    if (tn != "SceneObjectNode") return;

    auto* soNode = dynamic_cast<SceneObjectNode*>(targetNode);
    if (!soNode || !soNode->getEntity()) return;

    auto* entity = soNode->getEntity();

    // Lot AU.24 — Pattern 4 cross-class cascade, full hand-back (symmetric with
    // TextureNode + MaterialNode). Pick the highest-seq ENABLED peer targeting
    // the same entity ; predecessor → explicit re-apply (their update() gates
    // on param change) ; successor → no-op ; nobody → restore originals.
    AnimationNode* winner = nullptr;
    unsigned winnerSeq = 0;
    unsigned mySeq = mApplySeq.count(targetName) ? mApplySeq[targetName] : 0;
    for (auto& nn : animator->getRegisteredNodeNames()) {
        auto* on = animator->getRegisteredNode(nn);
        if (!on || on == this || !on->isEnabled()) continue;
        const auto& tn = on->getTypeName();
        if (tn != "MaterialBridgeNode" && tn != "TextureNode" && tn != "MaterialNode") continue;

        // Probe via inputs.entity → does this peer target the same entity?
        auto& oi = on->getInputs();
        auto eIt = oi.find("entity");
        if (eIt == oi.end()) continue;
        auto srcs = animator->getSourceNodes(eIt->second);
        bool targetsSame = false;
        for (auto* s : srcs) if (s && s->getName() == targetName) { targetsSame = true; break; }
        if (!targetsSame) continue;

        unsigned otherSeq = 0;
        if (auto* mb = dynamic_cast<MaterialBridgeNode*>(on))   otherSeq = mb->getApplySeq(targetName);
        else if (auto* tx = dynamic_cast<TextureNode*>(on))     otherSeq = tx->getApplySeq(targetName);
        else if (auto* m  = dynamic_cast<MaterialNode*>(on))    otherSeq = m->getApplySeq(targetName);
        if (otherSeq > winnerSeq) {
            winnerSeq = otherSeq;
            winner = on;
        }
    }

    auto origIt = mOriginalMaterials.find(targetName);
    if (!winner) {
        if (origIt != mOriginalMaterials.end()) {
            auto& origMats = origIt->second;
            for (unsigned s = 0; s < entity->getNumSubEntities() && s < origMats.size(); ++s) {
                entity->getSubEntity(s)->setMaterialName(origMats[s]);
            }
        }
    } else if (winnerSeq > mySeq) {
        // Successor already master — no-op.
    } else {
        if (auto* tx = dynamic_cast<TextureNode*>(winner))             tx->applyToEntity(targetName);
        else if (auto* m  = dynamic_cast<MaterialNode*>(winner))       m->applyToEntity(targetName);
        else if (auto* mb = dynamic_cast<MaterialBridgeNode*>(winner)) mb->applyToEntity(targetName);
    }
    if (origIt != mOriginalMaterials.end()) mOriginalMaterials.erase(origIt);
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

void MaterialBridgeNode::setEnabled(bool en) {
    AnimationNode::setEnabled(en);
    if (!en) {
        for (auto& t : mCurrentTargets) detachFromEntity(t);
    } else {
        mReenabling = true;
        // Force re-apply on next resolveTargets()
        mCurrentTargets.clear();
        mLastResolvedMaterial.clear();
    }
}

void MaterialBridgeNode::onLinkChanged() {
    resolveTargets();
}

void MaterialBridgeNode::update() {
    // Resolve targets first (handles new/removed entity links).
    resolveTargets();
    if (!mEnabled) { fireUpdate(); return; }

    // If material_in (or material_source) became empty, detach (graceful disable).
    std::string requested = resolveMaterialIn();
    if (requested.empty()) {
        if (!mLastResolvedMaterial.empty()) {
            for (auto& t : mCurrentTargets) detachFromEntity(t);
            mLastResolvedMaterial.clear();
        }
        fireUpdate();
        return;
    }

    // Detect changes (material name OR lighting mode) and re-apply if needed.
    auto* lightParam = mSpec.getParam("lighting_mode");
    std::string lightMode = lightParam ? lightParam->stringVal : std::string("unlit");

    // The material we WOULD apply for `requested` (same logic as applyToEntity).
    std::string would = isMaterial(requested)
                      ? requested
                      : ("MatBridge_" + getName() + "_" + requested + "_" + lightMode);
    bool dirty = (would != mLastResolvedMaterial) || (lightMode != mLastLightingMode);

    if (dirty) {
        for (auto& t : mCurrentTargets) applyToEntity(t);
    }

    fireUpdate();
}

void MaterialBridgeNode::cleanup() {
    for (auto& t : mCurrentTargets) detachFromEntity(t);
    mCurrentTargets.clear();
    mOriginalMaterials.clear();
    mApplySeq.clear();
    mLastResolvedMaterial.clear();
    mLastLightingMode.clear();
    // N7 — libère les matériaux MatBridge_* auto-wrap créés.
    auto& mm = Ogre::MaterialManager::getSingleton();
    for (auto& m : mCreatedMaterials) {
        if (mm.getByName(m)) mm.remove(m);
    }
    mCreatedMaterials.clear();
}

} // namespace bbfx
