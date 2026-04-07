#include "TextureNode.h"
#include "SceneObjectNode.h"
#include <OgreEntity.h>
#include <OgreSubEntity.h>
#include <OgreMaterialManager.h>
#include <OgreMaterial.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreTextureUnitState.h>
#include <iostream>

namespace bbfx {

TextureNode::TextureNode(const std::string& name)
    : AnimationNode(name)
{
    addInput(new AnimationPort("entity", 0.0f, true));

    ParamDef texDef;
    texDef.name = "texture";
    texDef.label = "Texture";
    texDef.type = ParamType::TEXTURE;
    texDef.stringVal = "";
    mSpec.addParam(texDef);

    ParamDef tgtDef;
    tgtDef.name = "target_entity";
    tgtDef.label = "Target Entity";
    tgtDef.type = ParamType::STRING;
    tgtDef.stringVal = "";
    mSpec.addParam(tgtDef);

    setParamSpec(&mSpec);
}

void TextureNode::resolveTargets() {
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
        if (!stillLinked) detachFromEntity(old);
    }

    mCurrentTargets = newTargets;

    // Apply to all current targets (only if enabled)
    if (mEnabled) {
        for (auto& t : mCurrentTargets) {
            applyToEntity(t);
        }
    }
}

void TextureNode::applyToEntity(const std::string& targetName) {
    auto* animator = Animator::instance();
    if (!animator) return;

    auto* targetNode = animator->getRegisteredNode(targetName);
    if (!targetNode || targetNode->getTypeName() != "SceneObjectNode") return;

    auto* soNode = dynamic_cast<SceneObjectNode*>(targetNode);
    if (!soNode || !soNode->getEntity()) return;

    auto* texParam = mSpec.getParam("texture");
    if (!texParam || texParam->stringVal.empty()) return;
    mTextureName = texParam->stringVal;

    auto* entity = soNode->getEntity();

    // Save original materials for ALL sub-entities (only if not already a TextureNode material)
    if (mOriginalMaterials.find(targetName) == mOriginalMaterials.end()) {
        std::vector<std::string> origMats;
        for (unsigned s = 0; s < entity->getNumSubEntities(); ++s) {
            std::string matN = entity->getSubEntity(s)->getMaterialName();
            // Skip materials from other TextureNodes — find the REAL originals
            if (matN.find("TexNode_") == 0) {
                // Another TextureNode already applied — get its saved originals instead
                // Search all registered TextureNodes for the one that has this target
                auto* anim = Animator::instance();
                if (anim) {
                    for (auto& nodeName : anim->getRegisteredNodeNames()) {
                        auto* otherNode = anim->getRegisteredNode(nodeName);
                        if (otherNode && otherNode->getTypeName() == "TextureNode" && otherNode != this) {
                            auto* otherTex = dynamic_cast<TextureNode*>(otherNode);
                            if (otherTex) {
                                auto oit = otherTex->getOriginalMaterials().find(targetName);
                                if (oit != otherTex->getOriginalMaterials().end() && s < oit->second.size()) {
                                    matN = oit->second[s]; // use the REAL original from the first TextureNode
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

    // Create a unique material with the texture
    std::string matName = "TexNode_" + getName() + "_" + mTextureName;
    auto& matMgr = Ogre::MaterialManager::getSingleton();
    auto mat = matMgr.getByName(matName);
    if (!mat) {
        mat = matMgr.create(matName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        auto* pass = mat->getTechnique(0)->getPass(0);
        pass->createTextureUnitState(mTextureName);
        pass->setLightingEnabled(true);
        pass->setDiffuse(1, 1, 1, 1);
        pass->setAmbient(0.3f, 0.3f, 0.3f);
    }

    // Apply to ALL sub-entities
    for (unsigned s = 0; s < entity->getNumSubEntities(); ++s) {
        entity->getSubEntity(s)->setMaterialName(matName);
    }
}

void TextureNode::detachFromEntity(const std::string& targetName) {
    auto* animator = Animator::instance();
    if (!animator) return;

    auto* targetNode = animator->getRegisteredNode(targetName);
    if (!targetNode || targetNode->getTypeName() != "SceneObjectNode") return;

    auto* soNode = dynamic_cast<SceneObjectNode*>(targetNode);
    if (!soNode || !soNode->getEntity()) return;

    auto* entity = soNode->getEntity();

    // Check if another ACTIVE TextureNode is also linked to this target
    // If so, let it handle the materials — don't restore originals
    bool otherActiveTexture = false;
    for (auto& nodeName : animator->getRegisteredNodeNames()) {
        auto* otherNode = animator->getRegisteredNode(nodeName);
        if (otherNode && otherNode != this && otherNode->getTypeName() == "TextureNode"
            && otherNode->isEnabled()) {
            auto* otherTex = dynamic_cast<TextureNode*>(otherNode);
            if (otherTex) {
                for (auto& t : otherTex->getCurrentTargets()) {
                    if (t == targetName) { otherActiveTexture = true; break; }
                }
            }
        }
        if (otherActiveTexture) break;
    }

    // Restore original materials per sub-entity
    auto origIt = mOriginalMaterials.find(targetName);
    if (origIt != mOriginalMaterials.end()) {
        if (!otherActiveTexture) {
            // No other active TextureNode → restore the REAL originals
            auto& origMats = origIt->second;
            for (unsigned s = 0; s < entity->getNumSubEntities() && s < origMats.size(); ++s) {
                entity->getSubEntity(s)->setMaterialName(origMats[s]);
            }
        }
        // else: another TextureNode is active, it will re-apply its texture in the next update()
        mOriginalMaterials.erase(origIt);
    }
}

void TextureNode::setEnabled(bool en) {
    AnimationNode::setEnabled(en);
    if (!en) {
        // Detach from all targets (restore originals)
        for (auto& t : mCurrentTargets) detachFromEntity(t);
    }
    // On re-enable, resolveTargets() in next update() will re-attach
}

void TextureNode::onLinkChanged() {
    resolveTargets();
}

void TextureNode::update() {
    auto* texParam = mSpec.getParam("texture");
    if (texParam && texParam->stringVal != mTextureName && !texParam->stringVal.empty()) {
        mTextureName = texParam->stringVal;
        for (auto& t : mCurrentTargets) applyToEntity(t);
    }
    resolveTargets();
    fireUpdate();
}

void TextureNode::cleanup() {
    for (auto& t : mCurrentTargets) detachFromEntity(t);
    mCurrentTargets.clear();
    mOriginalMaterials.clear();
}

} // namespace bbfx
