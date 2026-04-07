#include "MaterialNode.h"
#include "TextureNode.h"
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

    for (auto& old : mCurrentTargets) {
        bool stillLinked = false;
        for (auto& n : newTargets) { if (n == old) { stillLinked = true; break; } }
        if (!stillLinked) detachFromEntity(old);
    }

    mCurrentTargets = newTargets;
    if (mEnabled) {
        for (auto& t : mCurrentTargets) applyToEntity(t);
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

    // Check if another ACTIVE MaterialNode/TextureNode is linked
    bool otherActive = false;
    for (auto& nn : animator->getRegisteredNodeNames()) {
        auto* on = animator->getRegisteredNode(nn);
        if (on && on != this && on->isEnabled() &&
            (on->getTypeName() == "MaterialNode" || on->getTypeName() == "TextureNode")) {
            // Check if it targets the same entity
            auto& oInputs = on->getInputs();
            auto oIt = oInputs.find("entity");
            if (oIt != oInputs.end()) {
                auto oSources = animator->getSourceNodes(oIt->second);
                for (auto* os : oSources) {
                    if (os && os->getName() == targetName) { otherActive = true; break; }
                }
            }
        }
        if (otherActive) break;
    }

    auto origIt = mOriginalMaterials.find(targetName);
    if (origIt != mOriginalMaterials.end()) {
        if (!otherActive) {
            auto& origMats = origIt->second;
            for (unsigned s = 0; s < entity->getNumSubEntities() && s < origMats.size(); ++s) {
                entity->getSubEntity(s)->setMaterialName(origMats[s]);
            }
        }
        mOriginalMaterials.erase(origIt);
    }
}

void MaterialNode::setEnabled(bool en) {
    AnimationNode::setEnabled(en);
    if (!en) {
        for (auto& t : mCurrentTargets) detachFromEntity(t);
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
