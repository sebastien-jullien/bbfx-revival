#include "ShaderFxNode.h"
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
                           Ogre::SceneManager* scene,
                           Ogre::Entity* entity)
    : AnimationNode(name), mScene(scene), mEntity(entity)
{
    // Always add dt input for time accumulation
    addInput(new AnimationPort("dt", 0.016f));

    loadShader(vertPath, fragPath.empty() ? "shaders/passthrough.frag" : fragPath);
}

void ShaderFxNode::parseUniforms(const std::string& source) {
    // Extract "uniform float name;" lines (only float uniforms become ports)
    // Skip OGRE auto-params (worldViewProj, world, lightDiffuse, etc.)
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

    // Create vertex program
    std::string vpName = getName() + "_vp";
    auto vp = gpuMgr.createProgram(vpName, grp, "glsl", Ogre::GPT_VERTEX_PROGRAM);
    vp->setSource(vertSource);
    // Map OGRE vertex attributes
    vp->setParameter("input_operation_type", "triangle_list");

    // Read fragment shader
    auto fragDataStream = Ogre::ResourceGroupManager::getSingleton().openResource(fragPath, grp);
    std::string fragSource = fragDataStream->getAsString();

    std::string fpName = getName() + "_fp";
    auto fp = gpuMgr.createProgram(fpName, grp, "glsl", Ogre::GPT_FRAGMENT_PROGRAM);
    fp->setSource(fragSource);

    // Create material
    std::string matName = getName() + "_mat";
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

    // Apply material to entity
    for (unsigned i = 0; i < mEntity->getNumSubEntities(); ++i) {
        mEntity->getSubEntity(i)->setMaterial(mMaterial);
    }

    std::cout << "[ShaderFxNode] Loaded: " << vertPath << " (" << mUniforms.size() << " uniforms)" << std::endl;
}

void ShaderFxNode::update() {
    if (!mVertParams) return;

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
        try {
            mVertParams->setNamedConstant(u.name, val);
        } catch (...) {}
    }

    fireUpdate();
}

} // namespace bbfx
