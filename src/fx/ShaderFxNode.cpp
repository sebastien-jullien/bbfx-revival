#include "ShaderFxNode.h"
#include "../core/Animator.h"
#include "../studio/nodes/SceneObjectNode.h"
#include <OgreHighLevelGpuProgramManager.h>
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreEntity.h>
#include <OgreSubEntity.h>
#include <OgreSceneManager.h>
#include <OgreResourceGroupManager.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <iostream>

namespace bbfx {

ShaderFxNode::ShaderFxNode(const std::string& name,
                           const std::string& vertPath,
                           const std::string& fragPath,
                           Ogre::SceneManager* scene)
    : AnimationNode(name), mScene(scene)
{
    // dt input for time accumulation
    addInput(new AnimationPort("dt", 0.016f));
    // entity input — visual anchor for linking from SceneObjectNode
    addInput(new AnimationPort("entity", 0.0f));

    // ParamSpec for target_entity (STRING type, filled by auto-link)
    ParamDef targetDef;
    targetDef.name = "target_entity";
    targetDef.type = ParamType::STRING;
    mSpec.addParam(targetDef);
    setParamSpec(&mSpec);

    loadShader(vertPath, fragPath.empty() ? "shaders/passthrough.frag" : fragPath);
}

void ShaderFxNode::parseUniforms(const std::string& source) {
    static const std::set<std::string> ogreAutoParams = {
        "worldViewProj", "world", "worldView", "view", "projection",
        "lightDiffuse", "ambientLight", "materialDiffuse",
        "lightPosition", "lightDirection", "cameraPosition"
    };

    std::regex uniformRx(R"(uniform\s+float\s+(\w+)\s*;)");
    std::sregex_iterator it(source.begin(), source.end(), uniformRx);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        std::string uname = (*it)[1].str();
        if (ogreAutoParams.count(uname) == 0) {
            mUniforms.push_back({uname, 0.0f});
            addInput(new AnimationPort(uname, 0.0f));
        }
    }
}

void ShaderFxNode::loadShader(const std::string& vertPath, const std::string& fragPath) {
    auto& gpuMgr = Ogre::HighLevelGpuProgramManager::getSingleton();
    auto& matMgr = Ogre::MaterialManager::getSingleton();
    std::string grp = Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME;

    // Read vertex shader source to parse uniforms
    auto vertDataStream = Ogre::ResourceGroupManager::getSingleton().openResource(vertPath, grp);
    std::string vertSource = vertDataStream->getAsString();
    parseUniforms(vertSource);

    // Create vertex program with unique name
    static int sShaderCounter = 0;
    std::string uid = std::to_string(++sShaderCounter);
    std::string vpName = getName() + "_vp_" + uid;
    auto vp = gpuMgr.createProgram(vpName, grp, "glsl", Ogre::GPT_VERTEX_PROGRAM);
    vp->setSource(vertSource);
    vp->setParameter("input_operation_type", "triangle_list");

    // Read fragment shader and parse its uniforms too
    auto fragDataStream = Ogre::ResourceGroupManager::getSingleton().openResource(fragPath, grp);
    std::string fragSource = fragDataStream->getAsString();
    parseUniforms(fragSource);

    std::string fpName = getName() + "_fp_" + uid;
    auto fp = gpuMgr.createProgram(fpName, grp, "glsl", Ogre::GPT_FRAGMENT_PROGRAM);
    fp->setSource(fragSource);

    // Create material (but do NOT apply to any entity yet)
    std::string matName = getName() + "_mat_" + uid;
    mMaterial = matMgr.create(matName, grp);
    auto* pass = mMaterial->getTechnique(0)->getPass(0);

    pass->setVertexProgram(vpName);
    pass->setFragmentProgram(fpName);

    // Set OGRE auto-params
    mVertParams = pass->getVertexProgramParameters();
    mVertParams->setNamedAutoConstant("worldViewProj",
        Ogre::GpuProgramParameters::ACT_WORLDVIEWPROJ_MATRIX);
    mVertParams->setNamedAutoConstant("world",
        Ogre::GpuProgramParameters::ACT_WORLD_MATRIX);

    mFragParams = pass->getFragmentProgramParameters();
    try {
        mFragParams->setNamedAutoConstant("lightDiffuse",
            Ogre::GpuProgramParameters::ACT_LIGHT_DIFFUSE_COLOUR, 0);
        mFragParams->setNamedAutoConstant("ambientLight",
            Ogre::GpuProgramParameters::ACT_AMBIENT_LIGHT_COLOUR);
        mFragParams->setNamedAutoConstant("materialDiffuse",
            Ogre::GpuProgramParameters::ACT_SURFACE_DIFFUSE_COLOUR);
    } catch (...) {
        // Fragment shader may not use all auto-params
    }

    std::cout << "[ShaderFxNode] Loaded: " << vertPath << " (" << mUniforms.size() << " uniforms)" << std::endl;
}

void ShaderFxNode::setEnabled(bool en) {
    AnimationNode::setEnabled(en);
    if (!en) {
        detachFromEntity();
    }
    // On re-enable, resolveTarget() in next update() will re-attach
}

void ShaderFxNode::resolveTarget() {
    // Read target name from ParamSpec
    std::string targetName;
    auto* td = mSpec.getParam("target_entity");
    if (td) targetName = td->stringVal;

    // No target configured — detach if we were attached
    if (targetName.empty()) {
        if (mEntity) detachFromEntity();
        mTargetNodeName.clear();
        return;
    }

    // Look up the SceneObjectNode by name via the Animator
    auto* animator = dynamic_cast<Animator*>(getListener());
    if (!animator) return;

    auto* targetNode = animator->getRegisteredNode(targetName);
    if (!targetNode) {
        // Target was deleted — detach gracefully
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

    // If target mesh is disabled, detach shader
    if (!sceneObj->isEnabled()) {
        if (mEntity) detachFromEntity();
        return;
    }

    // Target changed — detach from old, attach to new
    if (targetName != mTargetNodeName) {
        if (mEntity) detachFromEntity();
        mTargetNodeName = targetName;
    }

    // Apply if not yet applied
    if (!mEntity) {
        applyToEntity(sceneObj->getEntity());
    }
}

void ShaderFxNode::applyToEntity(Ogre::Entity* entity) {
    if (!entity || !mMaterial) return;
    if (mEntity == entity) return; // already applied

    mEntity = entity;
    mOriginalMaterials.clear();
    for (unsigned i = 0; i < mEntity->getNumSubEntities(); ++i) {
        mOriginalMaterials.push_back(mEntity->getSubEntity(i)->getMaterialName());
        mEntity->getSubEntity(i)->setMaterial(mMaterial);
    }
}

void ShaderFxNode::detachFromEntity() {
    if (!mEntity) return;
    try {
        for (unsigned i = 0; i < mEntity->getNumSubEntities() && i < mOriginalMaterials.size(); ++i) {
            auto origMat = Ogre::MaterialManager::getSingleton().getByName(mOriginalMaterials[i]);
            if (origMat) mEntity->getSubEntity(i)->setMaterial(origMat);
        }
    } catch (...) {}
    mEntity = nullptr;
    mOriginalMaterials.clear();
}

void ShaderFxNode::update() {
    if (!mVertParams) return;

    // Resolve target entity each frame (handles deletion/reconnection)
    resolveTarget();

    // Accumulate time
    float dt = mInputs.count("dt") ? mInputs["dt"]->getValue() : 0.016f;
    mTime += dt;

    // Push custom uniforms to GPU
    for (auto& u : mUniforms) {
        float val = 0.0f;
        if (u.name == "time") {
            val = mTime;
        } else if (mInputs.count(u.name)) {
            val = mInputs[u.name]->getValue();
        }
        if (mVertParams) {
            try { mVertParams->setNamedConstant(u.name, val); } catch (...) {}
        }
        if (mFragParams) {
            try { mFragParams->setNamedConstant(u.name, val); } catch (...) {}
        }
    }

    fireUpdate();
}

void ShaderFxNode::cleanup() {
    detachFromEntity();
    mVertParams.reset();
    mFragParams.reset();
    mMaterial.reset();
}

} // namespace bbfx
