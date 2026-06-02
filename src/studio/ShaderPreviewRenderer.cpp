#include "ShaderPreviewRenderer.h"
#include <Ogre.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgreHighLevelGpuProgramManager.h>
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreTextureUnitState.h>
#include <OgreResourceGroupManager.h>
#include <iostream>
#include <regex>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <GL/gl.h>
#else
#  include <GL/gl.h>
#endif

namespace bbfx {

ShaderPreviewRenderer::ShaderPreviewRenderer() {}

ShaderPreviewRenderer::~ShaderPreviewRenderer() {
    invalidate();
    if (mPlaceholder) {
        GLuint id = static_cast<GLuint>(mPlaceholder);
        glDeleteTextures(1, &id);
        mPlaceholder = 0;
    }
    if (mSceneMgr && mRoot) {
        mRoot->destroySceneManager(mSceneMgr);
        mSceneMgr = nullptr;
    }
}

void ShaderPreviewRenderer::createPlaceholder() {
    const int sz = kPreviewSize;
    std::vector<unsigned char> pixels(sz * sz * 4, 60);
    for (int i = 0; i < sz * sz; ++i) {
        pixels[i * 4 + 3] = 255;
    }
    GLuint glId;
    glGenTextures(1, &glId);
    glBindTexture(GL_TEXTURE_2D, glId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sz, sz, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    mPlaceholder = static_cast<ImTextureID>(glId);
}

void ShaderPreviewRenderer::initialize(Ogre::Root* root) {
    if (mSceneMgr) return; // already initialized
    mRoot = root;

    createPlaceholder();

    try {
        mSceneMgr = root->createSceneManager("DefaultSceneManager", "ShaderPreviewScene");
        mSceneMgr->setAmbientLight(Ogre::ColourValue(0.3f, 0.3f, 0.3f));

        mCamera = mSceneMgr->createCamera("PreviewCamera");
        mCamera->setNearClipDistance(0.1f);
        mCamera->setFarClipDistance(100.0f);
        mCamera->setAutoAspectRatio(false);
        mCamera->setAspectRatio(1.0f); // square previews

        Ogre::SceneNode* camNode = mSceneMgr->getRootSceneNode()->createChildSceneNode("PreviewCameraNode");
        camNode->attachObject(mCamera);
        camNode->setPosition(0, 0, 3);
        camNode->lookAt(Ogre::Vector3(0, 0, 0), Ogre::Node::TS_WORLD);

        mLight = mSceneMgr->createLight("PreviewLight");
        mLight->setType(Ogre::Light::LT_DIRECTIONAL);
        mLight->setDiffuseColour(1.0f, 1.0f, 1.0f);

        Ogre::SceneNode* lightNode = mSceneMgr->getRootSceneNode()->createChildSceneNode("PreviewLightNode");
        lightNode->attachObject(mLight);
        lightNode->setDirection(Ogre::Vector3(1, -1, -1).normalisedCopy());

        std::cout << "[ShaderPreviewRenderer] Initialized with dedicated SceneManager" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[ShaderPreviewRenderer] Init failed: " << e.what() << std::endl;
        mSceneMgr = nullptr;
    }
}

void ShaderPreviewRenderer::createPreviewRTT(const std::string& name, const std::string& materialName, bool isSphere) {
    if (!mSceneMgr || !mRoot) return;

    try {
        std::string texName = "ShaderPreview_" + name;

        // Create RTT
        Ogre::TexturePtr tex = Ogre::TextureManager::getSingleton().createManual(
            texName,
            Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
            Ogre::TEX_TYPE_2D,
            kPreviewSize, kPreviewSize, 0,
            Ogre::PF_R8G8B8A8,
            Ogre::TU_RENDERTARGET
        );

        Ogre::RenderTexture* rt = tex->getBuffer()->getRenderTarget();
        rt->setAutoUpdated(false);

        Ogre::Viewport* vp = rt->addViewport(mCamera);
        vp->setBackgroundColour(Ogre::ColourValue(0.1f, 0.1f, 0.1f));
        vp->setOverlaysEnabled(false);

        // Create a mesh entity in the preview scene
        std::string entityName = "PreviewEntity_" + name;
        std::string meshName = isSphere ? "geosphere4500.mesh" : "bbfx_plane.mesh";

        Ogre::Entity* entity = nullptr;
        try {
            entity = mSceneMgr->createEntity(entityName, meshName);
            if (!materialName.empty()) {
                entity->setMaterialName(materialName);
            }
        } catch (...) {
            // Mesh not found — skip
            std::cerr << "[ShaderPreviewRenderer] Mesh not found for preview: " << meshName << std::endl;
        }

        if (entity) {
            auto* node = mSceneMgr->getRootSceneNode()->createChildSceneNode(entityName + "_node");
            node->attachObject(entity);
            if (isSphere) {
                node->setScale(0.02f, 0.02f, 0.02f); // scale sphere to fit camera
            }
        }

        // Get GL texture ID
        unsigned int glId = 0;
        tex->getCustomAttribute("GLID", &glId);

        PreviewEntry entry;
        entry.renderTarget = rt;
        entry.imguiId = (glId != 0) ? static_cast<ImTextureID>(glId) : mPlaceholder;
        entry.materialName = materialName;
        mPreviews[name] = entry;

    } catch (const std::exception& e) {
        std::cerr << "[ShaderPreviewRenderer] Failed to create RTT for " << name << ": " << e.what() << std::endl;
    }
}

std::string ShaderPreviewRenderer::buildShaderMaterial(const std::string& fragFile,
                                                      const std::string& vertFile) {
    auto cached = mShaderMaterials.find(fragFile);
    if (cached != mShaderMaterials.end()) return cached->second;

    const std::string grp = Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME;
    auto& gpuMgr = Ogre::HighLevelGpuProgramManager::getSingleton();
    auto& matMgr = Ogre::MaterialManager::getSingleton();

    // The "resources/shaders" folder is a resource location, so shader files are
    // referenced by bare name ("plasma.frag"). Fall back to a "shaders/" prefix in
    // case a caller passes an already-qualified path.
    auto openShader = [&grp](const std::string& f) -> std::string {
        auto& rgm = Ogre::ResourceGroupManager::getSingleton();
        try {
            return rgm.openResource(f, grp)->getAsString();
        } catch (const Ogre::Exception&) {
            return rgm.openResource("shaders/" + f, grp)->getAsString();
        }
    };

    try {
        std::string vertPath = vertFile.empty() ? "passthrough.vert" : vertFile;
        std::string fragPath = fragFile;

        std::string vertSource = openShader(vertPath);
        std::string fragSource = openShader(fragPath);

        static int sCounter = 0;
        std::string uid = std::to_string(++sCounter);
        std::string vpName  = "PreviewVP_" + uid;
        std::string fpName  = "PreviewFP_" + uid;
        std::string matName = "PreviewShaderMat_" + uid;

        auto vp = gpuMgr.createProgram(vpName, grp, "glsl", Ogre::GPT_VERTEX_PROGRAM);
        vp->setSource(vertSource);
        auto fp = gpuMgr.createProgram(fpName, grp, "glsl", Ogre::GPT_FRAGMENT_PROGRAM);
        fp->setSource(fragSource);

        Ogre::MaterialPtr mat = matMgr.create(matName, grp);
        auto* pass = mat->getTechnique(0)->getPass(0);
        pass->setLightingEnabled(false);
        pass->setVertexProgram(vpName);
        pass->setFragmentProgram(fpName);

        // Post-process style fragments may sample tex0/rt0/prevFrame — give them a TUS.
        if (fragSource.find("sampler2D tex0") != std::string::npos ||
            fragSource.find("sampler2D rt0") != std::string::npos ||
            fragSource.find("sampler2D prevFrame") != std::string::npos) {
            auto* tus = pass->createTextureUnitState();
            tus->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);
        }

        auto vparams = pass->getVertexProgramParameters();
        try { vparams->setNamedAutoConstant("worldViewProj",
                  Ogre::GpuProgramParameters::ACT_WORLDVIEWPROJ_MATRIX); } catch (...) {}
        try { vparams->setNamedAutoConstant("world",
                  Ogre::GpuProgramParameters::ACT_WORLD_MATRIX); } catch (...) {}

        // Seed scalar uniforms with a non-zero default so the preview isn't flat.
        // ("time" is driven each frame by update().)
        auto fparams = pass->getFragmentProgramParameters();
        std::regex floatRx(R"(uniform\s+float\s+(\w+)\s*;)");
        for (const std::string* src : {&vertSource, &fragSource}) {
            for (std::sregex_iterator it(src->begin(), src->end(), floatRx), e; it != e; ++it) {
                std::string u = (*it)[1].str();
                if (u == "time") continue;
                try { fparams->setNamedConstant(u, 1.0f); } catch (...) {}
                try { vparams->setNamedConstant(u, 1.0f); } catch (...) {}
            }
        }

        std::cout << "[ShaderPreviewRenderer] Built preview material '" << matName
                  << "' from " << vertPath << " + " << fragPath << std::endl;
        mShaderMaterials[fragFile] = matName;
        return matName;
    } catch (const std::exception& e) {
        std::cerr << "[ShaderPreviewRenderer] Failed to build material for " << fragFile
                  << ": " << e.what() << std::endl;
        mShaderMaterials[fragFile] = "";
        return "";
    }
}

ImTextureID ShaderPreviewRenderer::getShaderPreview(const std::string& fragFile,
                                                    const std::string& vertFile) {
    auto it = mPreviews.find("shader_" + fragFile);
    if (it != mPreviews.end()) return it->second.imguiId;

    std::string matName = buildShaderMaterial(fragFile, vertFile);
    createPreviewRTT("shader_" + fragFile, matName, false);
    it = mPreviews.find("shader_" + fragFile);
    return (it != mPreviews.end()) ? it->second.imguiId : mPlaceholder;
}

ImTextureID ShaderPreviewRenderer::getMaterialPreview(const std::string& materialName) {
    auto it = mPreviews.find("mat_" + materialName);
    if (it != mPreviews.end()) return it->second.imguiId;

    createPreviewRTT("mat_" + materialName, materialName, true);
    it = mPreviews.find("mat_" + materialName);
    return (it != mPreviews.end()) ? it->second.imguiId : mPlaceholder;
}

void ShaderPreviewRenderer::update(float deltaTime) {
    if (!mSceneMgr) return;

    mTime += deltaTime;
    mUpdateTimer += deltaTime;
    if (mUpdateTimer < kUpdateInterval) return;
    mUpdateTimer = 0.0f;

    for (auto& [name, entry] : mPreviews) {
        if (!entry.visible || !entry.renderTarget) continue;

        // Update time uniform on the material if it has gpu params
        try {
            Ogre::MaterialPtr matPtr = Ogre::MaterialManager::getSingleton().getByName(entry.materialName,
                Ogre::ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME);
            if (matPtr && matPtr->isLoaded()) {
                Ogre::Technique* tech = matPtr->getBestTechnique();
                if (tech && tech->getNumPasses() > 0) {
                    Ogre::Pass* pass = tech->getPass(0);
                    if (pass->hasVertexProgram()) {
                        Ogre::GpuProgramParametersSharedPtr params = pass->getVertexProgramParameters();
                        try { params->setNamedConstant("time", mTime); } catch (...) {}
                    }
                    if (pass->hasFragmentProgram()) {
                        Ogre::GpuProgramParametersSharedPtr params = pass->getFragmentProgramParameters();
                        try { params->setNamedConstant("time", mTime); } catch (...) {}
                    }
                }
            }
        } catch (...) {}

        entry.renderTarget->update();
    }
}

void ShaderPreviewRenderer::setVisible(const std::string& name, bool visible) {
    auto it = mPreviews.find(name);
    if (it != mPreviews.end()) it->second.visible = visible;
}

void ShaderPreviewRenderer::invalidate() {
    for (auto& [name, entry] : mPreviews) {
        std::string texName = "ShaderPreview_" + name;
        try {
            // Destroy the scene entity
            std::string entityName = "PreviewEntity_" + name;
            if (mSceneMgr && mSceneMgr->hasEntity(entityName)) {
                auto* entity = mSceneMgr->getEntity(entityName);
                if (entity->isAttached()) {
                    entity->getParentSceneNode()->detachObject(entity);
                }
                mSceneMgr->destroyEntity(entity);
                mSceneMgr->destroySceneNode(entityName + "_node");
            }
            // Destroy RTT
            Ogre::TextureManager::getSingleton().remove(texName,
                Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        } catch (...) {}
    }
    mPreviews.clear();
}

} // namespace bbfx
