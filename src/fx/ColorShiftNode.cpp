#include "ColorShiftNode.h"
#include "../core/Animator.h"
#include "../studio/nodes/SceneObjectNode.h"
#include <OgreEntity.h>
#include <OgreSubEntity.h>
#include <OgreRTShaderSystem.h>
#include <cmath>
#include <iostream>

namespace bbfx {

ColorShiftNode::ColorShiftNode(const std::string& nodeName)
    : AnimationNode(nodeName)
{
    // Entity input — visual anchor for linking from SceneObjectNode
    addInput(new AnimationPort("entity", 0.0f, true));
    // HSV controls
    addInput(new AnimationPort("hue_shift", 0.0f));
    addInput(new AnimationPort("saturation", 1.0f));
    addInput(new AnimationPort("brightness", 1.0f));
}

ColorShiftNode::~ColorShiftNode() = default;

Ogre::ColourValue ColorShiftNode::hsvToRgb(float h, float s, float v) {
    // Normalize hue to 0-360
    h = std::fmod(h, 360.0f);
    if (h < 0.0f) h += 360.0f;

    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r, g, b;
    if (h < 60.0f)       { r = c; g = x; b = 0; }
    else if (h < 120.0f) { r = x; g = c; b = 0; }
    else if (h < 180.0f) { r = 0; g = c; b = x; }
    else if (h < 240.0f) { r = 0; g = x; b = c; }
    else if (h < 300.0f) { r = x; g = 0; b = c; }
    else                 { r = c; g = 0; b = x; }

    return Ogre::ColourValue(r + m, g + m, b + m, 1.0f);
}

void ColorShiftNode::onLinkChanged() {
    resolveTarget();
}

void ColorShiftNode::setEnabled(bool en) {
    AnimationNode::setEnabled(en);
    if (!en) {
        detachFromEntity();
    }
    // On re-enable, resolveTarget() in next update() will re-attach
}

void ColorShiftNode::resolveTarget() {
    // Read target from the DAG graph (source of "entity" port)
    std::string targetName;
    auto* animator = Animator::instance();
    if (animator) {
        auto& inputs = getInputs();
        auto it = inputs.find("entity");
        if (it != inputs.end()) {
            auto sources = animator->getSourceNodes(it->second);
            if (!sources.empty() && sources[0])
                targetName = sources[0]->getName();
        }
    }

    // No target — detach if we were attached
    if (targetName.empty()) {
        if (mEntity) detachFromEntity();
        mTargetNodeName.clear();
        return;
    }

    // Look up the SceneObjectNode by name
    if (!animator) return;
    auto* targetNode = animator->getRegisteredNode(targetName);
    if (!targetNode) {
        if (mEntity) detachFromEntity();
        mTargetNodeName.clear();
        return;
    }

    auto* sceneObj = dynamic_cast<SceneObjectNode*>(targetNode);
    if (!sceneObj || !sceneObj->getEntity()) {
        if (mEntity) detachFromEntity();
        mTargetNodeName.clear();
        return;
    }

    // If target mesh is disabled, detach
    if (!sceneObj->isEnabled()) {
        if (mEntity) detachFromEntity();
        return;
    }

    // Target changed — detach from old, attach to new
    if (targetName != mTargetNodeName) {
        if (mEntity) detachFromEntity();
        mTargetNodeName = targetName;
        mEntityVersion = -1;
    }

    // Entity recreated (mesh change during project load) — detach so we reattach
    int curVersion = sceneObj->getEntityVersion();
    if (mEntity && curVersion != mEntityVersion) {
        detachFromEntity();
    }

    // Apply if not yet applied
    if (!mEntity) {
        applyToEntity(sceneObj->getEntity());
        mEntityVersion = curVersion;
    }
}

void ColorShiftNode::applyToEntity(Ogre::Entity* entity) {
    if (!entity) return;
    if (mEntity == entity) return;

    mEntity = entity;
    mMaterialNames.clear();
    mOriginalColors.clear();
    mSavedOriginal = false;

    // Capture all sub-entity material names
    for (unsigned i = 0; i < entity->getNumSubEntities(); ++i) {
        mMaterialNames.push_back(entity->getSubEntity(i)->getMaterialName());
    }

    std::cout << "[ColorShift] Attached to '" << mTargetNodeName
              << "' (" << mMaterialNames.size() << " materials)" << std::endl;
}

void ColorShiftNode::detachFromEntity() {
    if (!mEntity) return;

    // Restore original colors
    if (mSavedOriginal) {
        for (size_t i = 0; i < mMaterialNames.size() && i < mOriginalColors.size(); ++i) {
            auto mat = Ogre::MaterialManager::getSingleton().getByName(mMaterialNames[i]);
            if (!mat || mat->getNumTechniques() == 0) continue;
            try {
                auto* pass = mat->getTechnique(0)->getPass(0);
                pass->setEmissive(mOriginalColors[i].emissive);
                pass->setDiffuse(mOriginalColors[i].diffuse);
                pass->setAmbient(mOriginalColors[i].ambient);
                auto* sg = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
                if (sg) sg->invalidateMaterial(
                    Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME, mMaterialNames[i]);
            } catch (...) {}
        }
    }

    mEntity = nullptr;
    mMaterialNames.clear();
    mOriginalColors.clear();
    mSavedOriginal = false;
    std::cout << "[ColorShift] Detached from '" << mTargetNodeName << "'" << std::endl;
}

void ColorShiftNode::update() {
    // Resolve target entity each frame (handles deletion/reconnection)
    resolveTarget();
    if (!mEntity || mMaterialNames.empty()) return;

    auto& inputs = getInputs();
    float hue = inputs.at("hue_shift")->getValue();
    float sat = inputs.at("saturation")->getValue();
    float bright = inputs.at("brightness")->getValue();

    Ogre::ColourValue col = hsvToRgb(hue, sat, bright);
    for (size_t i = 0; i < mMaterialNames.size(); i++) {
        auto mat = Ogre::MaterialManager::getSingleton().getByName(mMaterialNames[i]);
        if (!mat || mat->getNumTechniques() == 0) continue;
        auto* pass = mat->getTechnique(0)->getPass(0);

        // Save original colors on first update
        if (!mSavedOriginal) {
            mOriginalColors.push_back({pass->getEmissive(), pass->getDiffuse(), pass->getAmbient()});
        }

        pass->setEmissive(col);
        pass->setDiffuse(col);
        pass->setAmbient(col);

        // Force RTSS to regenerate shaders with new material properties
        try {
            auto* sg = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
            if (sg) sg->invalidateMaterial(
                Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME, mMaterialNames[i]);
        } catch (...) {}
    }
    if (!mSavedOriginal && !mMaterialNames.empty()) mSavedOriginal = true;

    fireUpdate();
}

void ColorShiftNode::cleanup() {
    detachFromEntity();
}

} // namespace bbfx
