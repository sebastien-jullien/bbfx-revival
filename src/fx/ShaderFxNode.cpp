#include "ShaderFxNode.h"
#include "../core/Animator.h"
#include "../core/PrimitiveNodes.h"
#include "../core/DebugLog.h"
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
#include <OgreVector.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <cmath>
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
    targetDef.readOnly = true; // N1 — mirror read-only (cible via le port entity-link)
    targetDef.tooltip = "Cible résolue via le port entity-link (read-only).";
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
    static const std::set<std::string> autoSamplers = {
        "tex0", "rt0", "prevFrame"
    };

    // Match uniform float
    std::regex floatRx(R"(uniform\s+float\s+(\w+)\s*;)");
    std::sregex_iterator it(source.begin(), source.end(), floatRx);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        std::string uname = (*it)[1].str();
        if (ogreAutoParams.count(uname) == 0) {
            mUniforms.push_back({uname, 0.0f});
            addInput(new AnimationPort(uname, 0.0f));
            if (uname == "bass" || uname == "mid" || uname == "high")
                mHasAudioUniforms = true;
        }
    }

    // Match uniform vec2/vec3/vec4 → expand to component ports
    std::regex vecRx(R"(uniform\s+(vec[234])\s+(\w+)\s*;)");
    std::sregex_iterator vit(source.begin(), source.end(), vecRx);
    for (; vit != end; ++vit) {
        std::string type = (*vit)[1].str();
        std::string uname = (*vit)[2].str();
        if (ogreAutoParams.count(uname) != 0 || autoSamplers.count(uname) != 0) continue;

        int components = type[3] - '0'; // vec2→2, vec3→3, vec4→4
        static const char* suffixes[] = {"x", "y", "z", "w"};
        for (int c = 0; c < components; ++c) {
            std::string portName = uname + "." + suffixes[c];
            mUniforms.push_back({portName, 0.0f});
            addInput(new AnimationPort(portName, 0.0f));
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
    mVpName = vpName;
    auto vp = gpuMgr.createProgram(vpName, grp, "glsl", Ogre::GPT_VERTEX_PROGRAM);
    vp->setSource(vertSource);
    vp->setParameter("input_operation_type", "triangle_list");

    // Read fragment shader and parse its uniforms too
    auto fragDataStream = Ogre::ResourceGroupManager::getSingleton().openResource(fragPath, grp);
    std::string fragSource = fragDataStream->getAsString();
    parseUniforms(fragSource);

    std::string fpName = getName() + "_fp_" + uid;
    mFpName = fpName;
    auto fp = gpuMgr.createProgram(fpName, grp, "glsl", Ogre::GPT_FRAGMENT_PROGRAM);
    fp->setSource(fragSource);

    // Create material (but do NOT apply to any entity yet)
    std::string matName = getName() + "_mat_" + uid;
    mMatName = matName;
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

    BBFX_DLOG("[ShaderFxNode] Loaded: " << vertPath << " + " << fragPath
              << " (" << mUniforms.size() << " uniforms"
              << (mNeedsTex0 ? ", needs tex0" : "") << ")");
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
            BBFX_DLOG("[ShaderFx DIAG] " << getName() << ": no entity source found in DAG");
        } else {
            auto* targetNode = animator ? animator->getRegisteredNode(targetName) : nullptr;
            auto* sceneObj = targetNode ? dynamic_cast<SceneObjectNode*>(targetNode) : nullptr;
            BBFX_DLOG("[ShaderFx DIAG] " << getName() << ": target='" << targetName
                      << "' node=" << (targetNode ? "found" : "NULL")
                      << " isSceneObj=" << (sceneObj ? "yes" : "no")
                      << " entity=" << (sceneObj && sceneObj->getEntity() ? "present" : "NULL")
                      << " enabled=" << (sceneObj ? (sceneObj->isEnabled() ? "yes" : "no") : "N/A")
                      << " material=" << (mMaterial ? mMaterial->getName() : "NULL"));
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
        mEntityVersion = -1;
    }

    // Entity recreated (mesh change during project load) — detach so we reattach
    int curVersion = sceneObj->getEntityVersion();
    if (mEntity && curVersion != mEntityVersion) {
        detachFromEntity();
    }

    // Apply if not yet applied
    if (!mEntity) {
        BBFX_DLOG("[ShaderFx] Applying material '" << mMaterial->getName()
                  << "' to entity of '" << targetName << "'");
        applyToEntity(sceneObj->getEntity());
        mEntityVersion = curVersion;
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

float ShaderFxNode::computeBpmFallback(const std::string& uniformName,
                                       float totalTime, float bpm) const {
    if (bpm <= 0.0f) bpm = 120.0f;
    float beatsPerSec = bpm / 60.0f;

    // Multiplier: bass=quarter notes (1x), mid=eighth notes (2x), high=sixteenth notes (4x)
    float mult = 1.0f;
    if (uniformName == "mid")  mult = 2.0f;
    if (uniformName == "high") mult = 4.0f;

    float phase = totalTime * beatsPerSec * mult;
    float frac = phase - std::floor(phase); // 0..1 sawtooth

    // Exponential decay envelope: sharp attack, smooth decay
    // e^(-4*frac) gives a punchy pulse that decays to ~0.02 by end of beat
    return std::exp(-4.0f * frac);
}

void ShaderFxNode::update() {
    if (!mVertParams) return;

    // Resolve target entity each frame (handles deletion/reconnection)
    resolveTarget();

    // Accumulate time
    float dt = mInputs.count("dt") ? mInputs["dt"]->getValue() : 0.016f;
    mTime += dt;

    // BPM fallback: check which audio ports are unconnected (only if shader has audio uniforms)
    bool bpmFallbackBass = false, bpmFallbackMid = false, bpmFallbackHigh = false;
    float bpmVal = 120.0f;
    float totalTime = 0.0f;
    if (mHasAudioUniforms) {
        auto* animator = Animator::instance();
        auto* timeNode = RootTimeNode::instance();
        if (animator && timeNode) {
            bpmVal = timeNode->getBPM();
            totalTime = timeNode->getTotalTime();
            auto& ins = getInputs();
            auto checkPort = [&](const std::string& name) -> bool {
                auto it = ins.find(name);
                if (it == ins.end()) return false;
                return animator->getSourceNodes(it->second).empty();
            };
            bpmFallbackBass = checkPort("bass");
            bpmFallbackMid  = checkPort("mid");
            bpmFallbackHigh = checkPort("high");
        }
    }

    // Push custom uniforms to GPU
    // Collect vec components: "offset.x" → vec uniform "offset"
    std::map<std::string, std::vector<float>> vecAccum;

    for (auto& u : mUniforms) {
        // Check if this is a vec component (has .x/.y/.z/.w suffix)
        auto dotPos = u.name.rfind('.');
        if (dotPos != std::string::npos) {
            std::string base = u.name.substr(0, dotPos);
            float val = mInputs.count(u.name) ? mInputs[u.name]->getValue() : u.defaultValue;
            auto& vec = vecAccum[base];
            if (vec.empty()) vec.resize(4, 0.0f);
            char comp = u.name[dotPos + 1];
            int idx = (comp == 'x') ? 0 : (comp == 'y') ? 1 : (comp == 'z') ? 2 : 3;
            vec[idx] = val;
            continue;
        }

        float val = 0.0f;
        if (u.name == "time") {
            val = mTime;
        } else if (mInputs.count(u.name)) {
            val = mInputs[u.name]->getValue();
        }

        // BPM fallback for unconnected audio uniforms
        if ((u.name == "bass" && bpmFallbackBass) ||
            (u.name == "mid"  && bpmFallbackMid)  ||
            (u.name == "high" && bpmFallbackHigh)) {
            val = computeBpmFallback(u.name, totalTime, bpmVal);
        }

        if (mVertParams) {
            try { mVertParams->setNamedConstant(u.name, val); }
            catch (...) {}
        }
        if (mFragParams) {
            try { mFragParams->setNamedConstant(u.name, val); }
            catch (...) {}
        }
    }

    // Set vec uniforms
    for (auto& [name, comps] : vecAccum) {
        // Determine vec size by counting registered components
        int size = 0;
        for (const auto& u : mUniforms) {
            if (u.name.substr(0, name.size() + 1) == name + ".") size++;
        }
        if (size == 2) {
            Ogre::Vector2 v(comps[0], comps[1]);
            if (mVertParams) { try { mVertParams->setNamedConstant(name, v); } catch (...) {} }
            if (mFragParams) { try { mFragParams->setNamedConstant(name, v); } catch (...) {} }
        } else if (size == 3) {
            Ogre::Vector3 v(comps[0], comps[1], comps[2]);
            if (mVertParams) { try { mVertParams->setNamedConstant(name, v); } catch (...) {} }
            if (mFragParams) { try { mFragParams->setNamedConstant(name, v); } catch (...) {} }
        } else if (size == 4) {
            Ogre::Vector4 v(comps[0], comps[1], comps[2], comps[3]);
            if (mVertParams) { try { mVertParams->setNamedConstant(name, v); } catch (...) {} }
            if (mFragParams) { try { mFragParams->setNamedConstant(name, v); } catch (...) {} }
        }
    }

    fireUpdate();
}

void ShaderFxNode::cleanup() {
    detachFromEntity();
    mVertParams.reset();
    mFragParams.reset();
    mMaterial.reset();
    // Libère le matériau + les programmes GLSL runtime (sinon fuite à chaque create/delete).
    std::string grp = Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME;
    auto& matMgr = Ogre::MaterialManager::getSingleton();
    if (!mMatName.empty() && matMgr.getByName(mMatName, grp)) { matMgr.remove(mMatName, grp); mMatName.clear(); }
    auto& gpuMgr = Ogre::HighLevelGpuProgramManager::getSingleton();
    if (!mVpName.empty() && gpuMgr.getByName(mVpName, grp)) { gpuMgr.remove(mVpName, grp); mVpName.clear(); }
    if (!mFpName.empty() && gpuMgr.getByName(mFpName, grp)) { gpuMgr.remove(mFpName, grp); mFpName.clear(); }
}

} // namespace bbfx
