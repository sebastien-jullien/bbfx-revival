#include "MaterialNode.h"
#include "TextureNode.h"
#include "MaterialBridgeNode.h"
#include "SceneObjectNode.h"
#include <OgreEntity.h>
#include <OgreSubEntity.h>
#include <iostream>

namespace bbfx {

MaterialNode::MaterialNode(const std::string& name)
    : AnimationNode(name)
{
    addInput(new AnimationPort("entity", 0.0f, true));

    ParamDef matDef;
    matDef.name = "material";
    matDef.label = "Material";
    matDef.type = ParamType::MATERIAL;
    matDef.stringVal = "";
    mSpec.addParam(matDef);

    ParamDef tgtDef;
    tgtDef.name = "target_entity";
    tgtDef.label = "Target Entity";
    tgtDef.type = ParamType::STRING;
    tgtDef.stringVal = "";
    tgtDef.readOnly = true; // N1 — mirror read-only de la cible résolue via le port `entity`
    tgtDef.tooltip = "Cible(s) résolue(s) via le port entity-link (read-only).";
    mSpec.addParam(tgtDef);

    setParamSpec(&mSpec);
}

void MaterialNode::resolveTargets() {
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

    // Detach targets no longer linked + drop their cascade seq.
    for (auto& old : mCurrentTargets) {
        bool stillLinked = false;
        for (auto& n : newTargets) { if (n == old) { stillLinked = true; break; } }
        if (!stillLinked) {
            detachFromEntity(old);
            mApplySeq.erase(old);
        }
    }

    if (mEnabled) {
        // Apply new targets (mApplySeq incrementing) — Sprint S8 Lot AD Pattern 4.
        for (auto& t : newTargets) {
            bool isNew = true;
            for (auto& old : mCurrentTargets) { if (old == t) { isNew = false; break; } }
            if (isNew) {
                if (!mReenabling) mApplySeq[t] = nextCascadeApplySeq();    // Lot AU.24 — shared cascade counter
                applyToEntity(t);
            } else {
                // Already-known target — still re-apply to refresh material if param changed.
                applyToEntity(t);
            }
        }
    }

    mCurrentTargets = newTargets;
    mReenabling = false;

    // N1 — peuple le mirror read-only target_entity avec la liste résolue.
    if (auto* mir = mSpec.getParam("target_entity")) {
        std::string joined;
        for (auto& t : mCurrentTargets) { if (!joined.empty()) joined += ", "; joined += t; }
        mir->stringVal = joined;
    }
}

void MaterialNode::applyToEntity(const std::string& targetName) {
    auto* animator = Animator::instance();
    if (!animator) return;

    auto* targetNode = animator->getRegisteredNode(targetName);
    if (!targetNode || targetNode->getTypeName() != "SceneObjectNode") return;

    auto* soNode = dynamic_cast<SceneObjectNode*>(targetNode);
    if (!soNode || !soNode->getEntity()) return;

    auto* matParam = mSpec.getParam("material");
    if (!matParam || matParam->stringVal.empty()) return;
    mMaterialName = matParam->stringVal;

    auto* entity = soNode->getEntity();

    // Save original materials per sub-entity (skip other TextureNode/MaterialNode materials)
    if (mOriginalMaterials.find(targetName) == mOriginalMaterials.end()) {
        std::vector<std::string> origMats;
        for (unsigned s = 0; s < entity->getNumSubEntities(); ++s) {
            std::string matN = entity->getSubEntity(s)->getMaterialName();
            if (matN.find("TexNode_") == 0) {
                // Find real originals from the TextureNode that applied this
                auto* anim = Animator::instance();
                if (anim) {
                    for (auto& nn : anim->getRegisteredNodeNames()) {
                        auto* on = anim->getRegisteredNode(nn);
                        if (on && on->getTypeName() == "TextureNode") {
                            auto* tn = dynamic_cast<TextureNode*>(on);
                            if (tn) {
                                auto oit = tn->getOriginalMaterials().find(targetName);
                                if (oit != tn->getOriginalMaterials().end() && s < oit->second.size()) {
                                    matN = oit->second[s];
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            origMats.push_back(matN);
        }
        mOriginalMaterials[targetName] = origMats;
    }

    // Apply material to all sub-entities
    for (unsigned s = 0; s < entity->getNumSubEntities(); ++s) {
        entity->getSubEntity(s)->setMaterialName(mMaterialName);
    }
}

void MaterialNode::detachFromEntity(const std::string& targetName) {
    auto* animator = Animator::instance();
    if (!animator) return;

    auto* targetNode = animator->getRegisteredNode(targetName);
    if (!targetNode || targetNode->getTypeName() != "SceneObjectNode") return;

    auto* soNode = dynamic_cast<SceneObjectNode*>(targetNode);
    if (!soNode || !soNode->getEntity()) return;

    auto* entity = soNode->getEntity();

    // Lot AU.24 — Pattern 4 cross-class cascade, full hand-back (symmetric
    // with TextureNode + MaterialBridgeNode). Pick the enabled peer with the
    // highest seq targeting the same entity ; if it's a successor (seq>mySeq)
    // leave the entity alone, if it's a predecessor (seq<mySeq) hand the
    // entity back to it explicitly (peer's update() won't re-fire on its
    // own — it gates on a param change). No enabled peer → restore originals.
    AnimationNode* winner = nullptr;
    unsigned winnerSeq = 0;
    unsigned mySeq = mApplySeq.count(targetName) ? mApplySeq[targetName] : 0;
    for (auto& nn : animator->getRegisteredNodeNames()) {
        auto* on = animator->getRegisteredNode(nn);
        if (!on || on == this || !on->isEnabled()) continue;
        const auto& tn = on->getTypeName();
        if (tn != "MaterialNode" && tn != "TextureNode" && tn != "MaterialBridgeNode") continue;

        auto& oInputs = on->getInputs();
        auto oIt = oInputs.find("entity");
        if (oIt == oInputs.end()) continue;
        auto oSources = animator->getSourceNodes(oIt->second);
        bool targetsSame = false;
        for (auto* os : oSources) if (os && os->getName() == targetName) { targetsSame = true; break; }
        if (!targetsSame) continue;

        unsigned otherSeq = 0;
        if (auto* m = dynamic_cast<MaterialNode*>(on))             otherSeq = m->getApplySeq(targetName);
        else if (auto* tx = dynamic_cast<TextureNode*>(on))        otherSeq = tx->getApplySeq(targetName);
        else if (auto* mb = dynamic_cast<MaterialBridgeNode*>(on)) otherSeq = mb->getApplySeq(targetName);
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

void MaterialNode::setEnabled(bool en) {
    AnimationNode::setEnabled(en);
    if (!en) {
        // Disable : detach all targets (Pattern 4 cascade-aware) puis vider la
        // liste des cibles actives — l'attachement logique doit refléter le détach
        // réel. mApplySeq est conservé pour préserver la priorité de cascade au
        // ré-enable (resolveTargets ré-applique sans re-bumper le seq, mReenabling).
        for (auto& t : mCurrentTargets) detachFromEntity(t);
        mCurrentTargets.clear();
    } else {
        // Re-enable : trigger resolveTargets WITHOUT bumping the seq (we want
        // the original seq to be preserved so cascade priority is stable).
        mReenabling = true;
        resolveTargets();
    }
}

void MaterialNode::onLinkChanged() {
    resolveTargets();
}

void MaterialNode::update() {
    auto* matParam = mSpec.getParam("material");
    if (matParam && matParam->stringVal != mMaterialName && !matParam->stringVal.empty()) {
        mMaterialName = matParam->stringVal;
        for (auto& t : mCurrentTargets) applyToEntity(t);
    }
    resolveTargets();
    fireUpdate();
}

void MaterialNode::cleanup() {
    for (auto& t : mCurrentTargets) detachFromEntity(t);
    mCurrentTargets.clear();
    mOriginalMaterials.clear();
}

} // namespace bbfx
