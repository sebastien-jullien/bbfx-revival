#include "ParticleNode.h"
#include "SceneObjectNode.h"
#include <OgreParticleEmitter.h>
#include <OgreParticleAffector.h>
#include <OgreParticle.h>
#include <OgreMaterialManager.h>
#include <OgreMaterial.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreRTShaderSystem.h>
#include <algorithm>
namespace bbfx {
static int sPsysCounter = 0;

ParticleNode::ParticleNode(const std::string& name, Ogre::SceneManager* scene)
    : AnimationNode(name), mScene(scene) {
    ParamDef tmpl; tmpl.name = "template"; tmpl.label = "Template"; tmpl.type = ParamType::PARTICLE;
    tmpl.stringVal = "Examples/Smoke";
    mSpec.addParam(tmpl);
    ParamDef enDef; enDef.name = "enabled"; enDef.label = "Enabled"; enDef.type = ParamType::BOOL; enDef.boolVal = true;
    mSpec.addParam(enDef);
    setParamSpec(&mSpec);

    // Default -1.0f = "use template value". Positive = explicit override.
    addInput(new AnimationPort("emission_rate", -1.0f));
    addInput(new AnimationPort("position.x", 0.0f));
    addInput(new AnimationPort("position.y", 0.0f));
    addInput(new AnimationPort("position.z", 0.0f));
    // v3.5.2 Sprint S8 Lot AT — `enabled` port now provided by AnimationNode base.
    addInput(new AnimationPort("entity", 0.0f, true));

    // Color ports (I-1580) — default -1.0f = use template color
    addInput(new AnimationPort("color.r", -1.0f));
    addInput(new AnimationPort("color.g", -1.0f));
    addInput(new AnimationPort("color.b", -1.0f));
    addInput(new AnimationPort("color.a", -1.0f));

    // Size/velocity/lifetime ports (I-1581)
    // Default -1.0f = "use template value". Positive = multiplier on template value.
    addInput(new AnimationPort("particle_size", -1.0f));
    addInput(new AnimationPort("velocity", -1.0f));
    addInput(new AnimationPort("lifetime", -1.0f));

    ParamDef tgt; tgt.name = "target_entity"; tgt.label = "Target Entity"; tgt.type = ParamType::STRING; tgt.stringVal = "";
    tgt.readOnly = true; // N1 — mirror read-only de la cible (résolue via le port `entity`)
    tgt.tooltip = "Cible résolue via le port entity-link (read-only).";
    mSpec.addParam(tgt);

    mTemplateName = tmpl.stringVal;
    if (mScene) {
        std::string id = std::to_string(++sPsysCounter);
        try {
            mPsys = mScene->createParticleSystem("psys_" + id, mTemplateName);
            mSceneNode = mScene->getRootSceneNode()->createChildSceneNode("psyssn_" + id);
            mSceneNode->attachObject(mPsys);
            // Store original template values for multiplier mode
            mOrigWidth = mPsys->getDefaultWidth();
            mOrigHeight = mPsys->getDefaultHeight();
            if (mPsys->getNumEmitters() > 0) {
                auto* em = mPsys->getEmitter(0);
                mOrigVelMin = em->getMinParticleVelocity();
                mOrigVelMax = em->getMaxParticleVelocity();
            }
        } catch (...) {
            mPsys = nullptr;
        }
    }
}
ParticleNode::~ParticleNode() { cleanup(); }

void ParticleNode::update() {
    // Check if template changed via ParamSpec
    auto* tp = mSpec.getParam("template");
    if (tp && !tp->stringVal.empty() && tp->stringVal != mTemplateName && mScene) {
        // Destroy old particle system and create new one
        if (mPsys && mSceneNode) {
            mSceneNode->detachObject(mPsys);
            mScene->destroyParticleSystem(mPsys);
            mPsys = nullptr;
        }
        mTemplateName = tp->stringVal;
        std::string id = std::to_string(++sPsysCounter);
        try {
            mPsys = mScene->createParticleSystem("psys_" + id, mTemplateName);
            if (mSceneNode) mSceneNode->attachObject(mPsys);
            // Store original template values for multiplier mode
            mOrigWidth = mPsys->getDefaultWidth();
            mOrigHeight = mPsys->getDefaultHeight();
            if (mPsys->getNumEmitters() > 0) {
                auto* em = mPsys->getEmitter(0);
                mOrigVelMin = em->getMinParticleVelocity();
                mOrigVelMax = em->getMaxParticleVelocity();
            }
        } catch (...) { mPsys = nullptr; }
        // Reset cached values to force re-apply on new system
        mColorOverride = false;
        mAffectorsRemoved = false;
        mOrigMaterialName.clear();
        mPrevSize = -1.0f;
        mPrevVelocity = -1.0f;
        mPrevLifetime = -999.0f;
    }

    resolveTarget();
    if (!mPsys || !mSceneNode) return;
    auto& in = getInputs();
    mSceneNode->setPosition(in.at("position.x")->getValue(),
                            in.at("position.y")->getValue(),
                            in.at("position.z")->getValue());
    bool en = in.at("enabled")->getValue() >= 0.5f;
    mPsys->setEmitting(en);
    if (mPsys->getNumEmitters() > 0) {
        auto* emitter = mPsys->getEmitter(0);

        // Emission rate: default -1 = use template value
        float rate = in.at("emission_rate")->getValue();
        if (rate > 0.0f) {
            emitter->setEmissionRate(rate);
        }

        // Color control (I-1580/I-1690)
        float cr = in.at("color.r")->getValue();
        float cg = in.at("color.g")->getValue();
        float cb = in.at("color.b")->getValue();
        float ca = in.at("color.a")->getValue();
        mColorOverride = (cr >= 0.0f || cg >= 0.0f || cb >= 0.0f || ca >= 0.0f);

        if (mColorOverride) {
            // Unconnected RGB channels (-1) default to 0 so connecting
            // a single channel produces visible colour (not white).
            // Unconnected alpha defaults to 1 (fully opaque).
            cr = std::clamp(cr < 0.0f ? 0.0f : cr, 0.0f, 1.0f);
            cg = std::clamp(cg < 0.0f ? 0.0f : cg, 0.0f, 1.0f);
            cb = std::clamp(cb < 0.0f ? 0.0f : cb, 0.0f, 1.0f);
            ca = std::clamp(ca < 0.0f ? 1.0f : ca, 0.0f, 1.0f);
            mOverrideColour = Ogre::ColourValue(cr, cg, cb, ca);

            // Remove colour-related affectors that fight our override.
            // This is done once per override activation.
            if (!mAffectorsRemoved) {
                for (int i = (int)mPsys->getNumAffectors() - 1; i >= 0; --i) {
                    std::string atype = mPsys->getAffector(i)->getType();
                    if (atype == "ColourFader" || atype == "ColourFader2"
                        || atype == "ColourImage" || atype == "ColourInterpolator") {
                        mPsys->removeAffector((unsigned short)i);
                    }
                }
                mAffectorsRemoved = true;
            }

            // Set emitter colour for new particles
            emitter->setColour(mOverrideColour);
        }

        // Velocity control (I-1581) — multiplier on template velocity range
        // Default -1.0f = use template value unchanged
        float vel = in.at("velocity")->getValue();
        if (vel > 0.0f && vel != mPrevVelocity) {
            emitter->setParticleVelocity(mOrigVelMin * vel, mOrigVelMax * vel);
            mPrevVelocity = vel;
        }

        // Lifetime control (I-1581)
        float lt = in.at("lifetime")->getValue();
        if (lt != mPrevLifetime && lt > 0.0f) {
            emitter->setTimeToLive(lt);
            mPrevLifetime = lt;
        }
    }

    // Particle size control (I-1581) — multiplier on template dimensions
    // Default -1.0f = use template value unchanged
    float sz = in.at("particle_size")->getValue();
    if (sz > 0.0f && sz != mPrevSize) {
        mPsys->setDefaultDimensions(mOrigWidth * sz, mOrigHeight * sz);
        mPrevSize = sz;
    }

    // OGRE ParticleSystems need _update() to generate and move particles.
    // In Studio mode, the normal OGRE frame loop doesn't run, so we must
    // call _update() manually each DAG frame.
    mPsys->_update(0.016f);

    // Force colour on ALL live particles AFTER _update() (I-1690).
    // With colour affectors removed, this ensures consistent colour even if
    // OGRE's scene manager calls _update() again during rendering.
    if (mColorOverride) {
        Ogre::RGBA packed = mOverrideColour.getAsBYTE();
        size_t n = mPsys->getNumParticles();
        for (size_t i = 0; i < n; ++i) {
            mPsys->getParticle(i)->mColour = packed;
        }

        // Clone material with vertex colour tracking (I-1690).
        // RTSS with 'lighting off' ignores vertex colours by default.
        // We clone the material, enable TVC_DIFFUSE tracking, and strip
        // inherited RTSS techniques so RTSS regenerates a shader that
        // reads vertex colours from BillboardSet (particle mColour).
        std::string cloneName = "PtclTint_" + getName();
        if (mOrigMaterialName.empty()) {
            mOrigMaterialName = mPsys->getMaterialName();
            auto& matMgr = Ogre::MaterialManager::getSingleton();
            if (matMgr.resourceExists(cloneName))
                matMgr.remove(cloneName);
            auto srcMat = matMgr.getByName(mOrigMaterialName);
            if (srcMat) {
                auto cloned = srcMat->clone(cloneName);
                // Strip inherited RTSS technique so it regenerates
                while (cloned->getNumTechniques() > 1)
                    cloned->removeTechnique(cloned->getNumTechniques() - 1);
                cloned->getTechnique(0)->getPass(0)->setVertexColourTracking(
                    Ogre::TVC_DIFFUSE);
                auto* shaderGen = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
                if (shaderGen)
                    shaderGen->invalidateMaterial(
                        Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME,
                        cloneName);
                mPsys->setMaterialName(cloneName);
            }
        }
    } else if (!mOrigMaterialName.empty()) {
        // Restore original material when colour override is deactivated
        mPsys->setMaterialName(mOrigMaterialName);
        mOrigMaterialName.clear();
    }

    fireUpdate();
}

void ParticleNode::setEnabled(bool en) {
    AnimationNode::setEnabled(en);
    if (mSceneNode) mSceneNode->setVisible(en && mUserVisible);
}

void ParticleNode::setUserVisible(bool v) {
    AnimationNode::setUserVisible(v);
    if (mSceneNode) mSceneNode->setVisible(v && mEnabled);
}

void ParticleNode::cleanup() {
    if (mScene && mSceneNode) {
        if (mPsys) mSceneNode->detachObject(mPsys);
        mScene->destroySceneNode(mSceneNode);
        if (mPsys) mScene->destroyParticleSystem(mPsys);
        mSceneNode = nullptr; mPsys = nullptr;
    }
    // C8 — libère le matériau cloné pour la teinte (sinon fuite par node coloré).
    std::string cloneName = "PtclTint_" + getName();
    auto& matMgr = Ogre::MaterialManager::getSingleton();
    if (matMgr.resourceExists(cloneName)) matMgr.remove(cloneName);
    mOrigMaterialName.clear();
}
void ParticleNode::resolveTarget() {
    auto* animator = Animator::instance();
    if (!animator || !mSceneNode) return;
    auto& inputs = getInputs();
    auto it = inputs.find("entity");
    if (it != inputs.end()) {
        auto sources = animator->getSourceNodes(it->second);
        if (!sources.empty()) {
            auto* srcNode = sources[0];
            if (srcNode && srcNode->getTypeName() == "SceneObjectNode") {
                auto* soNode = dynamic_cast<SceneObjectNode*>(srcNode);
                if (soNode && soNode->getSceneNode()) {
                    if (mSceneNode->getParentSceneNode() != soNode->getSceneNode()) {
                        auto* oldParent = mSceneNode->getParentSceneNode();
                        if (oldParent) oldParent->removeChild(mSceneNode);
                        soNode->getSceneNode()->addChild(mSceneNode);
                        mSceneNode->setPosition(0, 0, 0);
                    }
                    mTargetNodeName = srcNode->getName();
                    return;
                }
            }
        }
    }
    // No target: reparent to root
    if (!mTargetNodeName.empty() && mSceneNode) {
        auto* oldParent = mSceneNode->getParentSceneNode();
        if (oldParent && oldParent != mScene->getRootSceneNode()) {
            oldParent->removeChild(mSceneNode);
            mScene->getRootSceneNode()->addChild(mSceneNode);
        }
        mTargetNodeName.clear();
    }
}

void ParticleNode::onLinkChanged() { resolveTarget(); }

} // namespace bbfx
