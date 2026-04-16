#include "ShaderFxNode.h"
#include "../core/Animator.h"
#include "../studio/nodes/SceneObjectNode.h"
#include <OgreHighLevelGpuProgramManager.h>
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreEntity.h>
#include <OgreSubEntity.h>
#include <OgreTextureUnitState.h>
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
    addInput(new AnimationPort("entity", 0.0f, true));

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

    // If fragment shader uses sampler2D tex0, add a TextureUnitState
    // so post-process shaders can read the mesh's original texture
    if (fragSource.find("sampler2D tex0") != std::string::npos ||
        fragSource.find("sampler2D rt0") != std::string::npos) {
        auto* tus = pass->createTextureUnitState();
        tus->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);
        // The texture will be inherited from the entity's original material
        // when applyToEntity() is called
        mNeedsTex0 = true;
    }

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

    std::cout << "[ShaderFxNode] Loaded: " << vertPath << " + " << fragPath
              << " (" << mUniforms.size() << " uniforms"
              << (mNeedsTex0 ? ", needs tex0" : "") << ")" << std::endl;
}

void ShaderFxNode::setEnabled(bool en) {
    AnimationNode::setEnabled(en);
    if (!en) {
        detachFromEntity();
    }
    // On re-enable, resolveTarget() in next update() will re-attach
}

void ShaderFxNode::onLinkChanged() {
    resolveTarget();
}

void ShaderFxNode::resolveTarget() {
    // Read first target from the DAG graph (source of truth)
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

    // Diagnostic log (once per target change)
    static std::string sLastDiag;
    std::string diag = getName() + "→" + targetName;
    if (diag != sLastDiag) {
        sLastDiag = diag;
        if (targetName.empty()) {
            std::cout << "[ShaderFx DIAG] " << getName() << ": no entity source found in DAG" << std::endl;
        } else {
            auto* targetNode = animator ? animator->getRegisteredNode(targetName) : nullptr;
            auto* sceneObj = targetNode ? dynamic_cast<SceneObjectNode*>(targetNode) : nullptr;
            std::cout << "[ShaderFx DIAG] " << getName() << ": target='" << targetName
                      << "' node=" << (targetNode ? "found" : "NULL")
                      << " isSceneObj=" << (sceneObj ? "yes" : "no")
                      << " entity=" << (sceneObj && sceneObj->getEntity() ? "present" : "NULL")
                      << " enabled=" << (sceneObj ? (sceneObj->isEnabled() ? "yes" : "no") : "N/A")
                      << " material=" << (mMaterial ? mMaterial->getName() : "NULL")
                      << std::endl;
        }
    }

    // No target configured — detach if we were attached
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
        std::cout << "[ShaderFx] Applying material '" << mMaterial->getName()
                  << "' to entity of '" << targetName << "'" << std::endl;
        applyToEntity(sceneObj->getEntity());
    }
}

void ShaderFxNode::applyToEntity(Ogre::Entity* entity) {
    if (!entity || !mMaterial) return;
    if (mEntity == entity) return; // already applied

    mEntity = entity;
    mOriginalMaterials.clear();

    // If shader needs tex0, copy the original diffuse texture to our material
    if (mNeedsTex0) {
        auto* pass = mMaterial->getTechnique(0)->getPass(0);
        if (pass->getNumTextureUnitStates() > 0 && entity->getNumSubEntities() > 0) {
            auto origMat = entity->getSubEntity(0)->getMaterial();
            if (origMat && origMat->getNumTechniques() > 0) {
                auto* origPass = origMat->getTechnique(0)->getPass(0);
                if (origPass && origPass->getNumTextureUnitStates() > 0) {
                    auto* origTus = origPass->getTextureUnitState(0);
                    pass->getTextureUnitState(0)->setTextureName(origTus->getTextureName());
                }
            }
        }
    }

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
            try { mVertParams->setNamedConstant(u.name, val); }
            catch (...) { /* Uniform may not exist in vertex shader — safe to ignore */ }
        }
        if (mFragParams) {
            try { mFragParams->setNamedConstant(u.name, val); }
            catch (...) { /* Uniform may not exist in fragment shader — safe to ignore */ }
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
