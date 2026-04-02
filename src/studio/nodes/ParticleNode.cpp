#include "ParticleNode.h"
#include <OgreParticleEmitter.h>
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

    addInput(new AnimationPort("emission_rate", 15.0f));
    addInput(new AnimationPort("position.x", 0.0f));
    addInput(new AnimationPort("position.y", 0.0f));
    addInput(new AnimationPort("position.z", 0.0f));
    addInput(new AnimationPort("enabled", 1.0f));

    mTemplateName = tmpl.stringVal;
    if (mScene) {
        std::string id = std::to_string(++sPsysCounter);
        try {
            mPsys = mScene->createParticleSystem("psys_" + id, mTemplateName);
            mSceneNode = mScene->getRootSceneNode()->createChildSceneNode("psyssn_" + id);
            mSceneNode->attachObject(mPsys);
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
        } catch (...) { mPsys = nullptr; }
    }

    if (!mPsys || !mSceneNode) return;
    auto& in = getInputs();
    mSceneNode->setPosition(in.at("position.x")->getValue(),
                            in.at("position.y")->getValue(),
                            in.at("position.z")->getValue());
    bool en = in.at("enabled")->getValue() >= 0.5f;
    mPsys->setEmitting(en);
    if (mPsys->getNumEmitters() > 0) {
        mPsys->getEmitter(0)->setEmissionRate(in.at("emission_rate")->getValue());
    }
    // OGRE ParticleSystems need _update() to generate and move particles.
    // In Studio mode, the normal OGRE frame loop doesn't run, so we must
    // call _update() manually each DAG frame.
    mPsys->_update(0.016f);
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
}
} // namespace bbfx
