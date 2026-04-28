#include "PostProcessStack.h"
#include <OgreMaterialManager.h>
#include <OgreMaterial.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreTextureUnitState.h>
#include <OgreTextureManager.h>
#include <OgreRenderTexture.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgreCamera.h>
#include <OgreViewport.h>
#include <OgreRoot.h>
#include <OgreRenderSystem.h>
#include <OgreRenderOperation.h>
#include <algorithm>
#include <iostream>

namespace bbfx {

PostProcessStack::PostProcessStack(Ogre::SceneManager* sm)
    : mSceneMgr(sm)
{
    // Create a fullscreen quad for rendering effects.
    // Rectangle2D with texture coordinates, covering [-1,1] clip space.
    mQuad = std::make_unique<Ogre::Rectangle2D>(true);
    mQuad->setCorners(-1.0f, 1.0f, 1.0f, -1.0f);
    mQuad->setBoundingBox(Ogre::AxisAlignedBox::BOX_INFINITE);
}

PostProcessStack::~PostProcessStack() {
    // Detach quad from scene if attached
    if (mQuadNode && mSceneMgr) {
        mQuadNode->detachAllObjects();
        mSceneMgr->destroySceneNode(mQuadNode);
        mQuadNode = nullptr;
    }
    mQuad.reset();
    destroyRTTs();
}

void PostProcessStack::init(uint32_t width, uint32_t height) {
    destroyRTTs();
    mWidth = width;
    mHeight = height;

    auto& texMgr = Ogre::TextureManager::getSingleton();
    const auto grp = Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME;

    mPingTex = texMgr.createManual(
        "_PostProcess_Ping", grp, Ogre::TEX_TYPE_2D,
        width, height, 0, Ogre::PF_R8G8B8A8, Ogre::TU_RENDERTARGET);
    mPingRT = mPingTex->getBuffer()->getRenderTarget();
    mPingRT->setAutoUpdated(false);

    mPongTex = texMgr.createManual(
        "_PostProcess_Pong", grp, Ogre::TEX_TYPE_2D,
        width, height, 0, Ogre::PF_R8G8B8A8, Ogre::TU_RENDERTARGET);
    mPongRT = mPongTex->getBuffer()->getRenderTarget();
    mPongRT->setAutoUpdated(false);

    // Feedback RTT for temporal effects (prevFrame)
    mPrevFrameTex = texMgr.createManual(
        "_PostProcess_PrevFrame", grp, Ogre::TEX_TYPE_2D,
        width, height, 0, Ogre::PF_R8G8B8A8, Ogre::TU_RENDERTARGET);
    mPrevFrameRT = mPrevFrameTex->getBuffer()->getRenderTarget();
    mPrevFrameRT->setAutoUpdated(false);

    // Add viewports (required for _setViewport during manual rendering).
    // We use the MainCamera but never call update() on these RTs — rendering is manual.
    if (mSceneMgr && mSceneMgr->hasCamera("MainCamera")) {
        auto* cam = mSceneMgr->getCamera("MainCamera");
        auto* vpPing = mPingRT->addViewport(cam);
        vpPing->setOverlaysEnabled(false);
        vpPing->setClearEveryFrame(false);

        auto* vpPong = mPongRT->addViewport(cam);
        vpPong->setOverlaysEnabled(false);
        vpPong->setClearEveryFrame(false);

        auto* vpPrev = mPrevFrameRT->addViewport(cam);
        vpPrev->setOverlaysEnabled(false);
        vpPrev->setClearEveryFrame(false);
    }

    std::cout << "[PostProcessStack] init " << width << "x" << height << std::endl;
}

void PostProcessStack::resize(uint32_t width, uint32_t height) {
    if (width < 64) width = 64;
    if (height < 64) height = 64;
    if (width == mWidth && height == mHeight) return;
    init(width, height);
}

void PostProcessStack::destroyRTTs() {
    auto& texMgr = Ogre::TextureManager::getSingleton();
    const auto grp = Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME;

    mPingRT = nullptr;
    mPongRT = nullptr;
    mPrevFrameRT = nullptr;
    if (mPingTex) {
        mPingTex.reset();
        texMgr.remove("_PostProcess_Ping", grp);
    }
    if (mPongTex) {
        mPongTex.reset();
        texMgr.remove("_PostProcess_Pong", grp);
    }
    if (mPrevFrameTex) {
        mPrevFrameTex.reset();
        texMgr.remove("_PostProcess_PrevFrame", grp);
    }
    mOutputTex.reset();
    mWidth = mHeight = 0;
}

// ── Effect management ────────────────────────────────────────────────────────

bool PostProcessStack::addEffect(const std::string& name, const std::string& materialName) {
    if (!Ogre::MaterialManager::getSingleton().resourceExists(materialName,
            Ogre::ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME)) {
        std::cerr << "[PostProcessStack] addEffect: material '" << materialName
                  << "' not found" << std::endl;
        return false;
    }
    for (auto& e : mEffects) {
        if (e.name == name) {
            std::cerr << "[PostProcessStack] addEffect: '" << name
                      << "' already exists" << std::endl;
            return false;
        }
    }

    PostProcessEffect effect;
    effect.name = name;
    effect.materialName = materialName;
    effect.order = mEffects.empty() ? 0 : mEffects.back().order + 1;

    // Auto-detect if this effect uses prevFrame feedback
    auto mat = Ogre::MaterialManager::getSingleton().getByName(materialName,
        Ogre::ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME);
    if (mat) {
        mat->load();
        auto* tech = mat->getBestTechnique();
        if (tech && tech->getNumPasses() > 0) {
            auto* pass = tech->getPass(0);
            if (pass->hasFragmentProgram()) {
                try {
                    auto fpParams = pass->getFragmentProgramParameters();
                    if (fpParams && fpParams->_findNamedConstantDefinition("prevFrame")) {
                        effect.needsPrevFrame = true;
                    }
                } catch (...) {}
            }
        }
    }

    mEffects.push_back(std::move(effect));
    return true;
}

bool PostProcessStack::removeEffect(const std::string& name) {
    auto it = std::find_if(mEffects.begin(), mEffects.end(),
        [&](const PostProcessEffect& e) { return e.name == name; });
    if (it == mEffects.end()) return false;
    mEffects.erase(it);
    return true;
}

bool PostProcessStack::reorder(const std::string& name, int newPosition) {
    auto it = std::find_if(mEffects.begin(), mEffects.end(),
        [&](const PostProcessEffect& e) { return e.name == name; });
    if (it == mEffects.end()) return false;
    it->order = newPosition;
    std::sort(mEffects.begin(), mEffects.end(),
        [](const PostProcessEffect& a, const PostProcessEffect& b) {
            return a.order < b.order;
        });
    return true;
}

bool PostProcessStack::setEnabled(const std::string& name, bool enabled) {
    auto* e = findEffect(name);
    if (!e) return false;
    e->enabled = enabled;
    return true;
}

bool PostProcessStack::setParam(const std::string& effectName,
                                 const std::string& param, float value) {
    auto* e = findEffect(effectName);
    if (!e) return false;
    e->params[param] = value;
    return true;
}

std::vector<PostProcessEffect> PostProcessStack::getEffects() const {
    auto sorted = mEffects;
    std::sort(sorted.begin(), sorted.end(),
        [](const PostProcessEffect& a, const PostProcessEffect& b) {
            return a.order < b.order;
        });
    return sorted;
}

PostProcessEffect* PostProcessStack::findEffect(const std::string& name) {
    for (auto& e : mEffects) {
        if (e.name == name) return &e;
    }
    return nullptr;
}

void PostProcessStack::clear() {
    mEffects.clear();
}

bool PostProcessStack::hasActiveEffects() const {
    for (auto& e : mEffects) {
        if (e.enabled) return true;
    }
    return false;
}

// ── Rendering pipeline ───────────────────────────────────────────────────────

void PostProcessStack::apply(Ogre::RenderTexture* sourceRT) {
    if (!sourceRT) return;

    // Collect active effects sorted by order
    auto effects = getEffects();
    std::vector<PostProcessEffect*> active;
    for (auto& e : effects) {
        if (e.enabled) active.push_back(&e);
    }

    // No active effects: reset output, ViewportPanel uses raw scene RT
    if (active.empty()) {
        mOutputTex.reset();
        return;
    }

    // Ensure RTTs are initialized
    if (!mPingRT || !mPongRT) {
        init(sourceRT->getWidth(), sourceRT->getHeight());
    }

    // Source texture for first effect
    auto sourceTex = Ogre::TextureManager::getSingleton().getByName(
        "StudioRenderTexture",
        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    if (!sourceTex) return;

    Ogre::TexturePtr currentInput = sourceTex;
    bool usePingAsTarget = true;

    for (size_t i = 0; i < active.size(); ++i) {
        auto* effect = active[i];
        Ogre::RenderTexture* target = usePingAsTarget ? mPingRT : mPongRT;

        // Detect if this effect's shader uses prevFrame
        bool needsPrev = effect->needsPrevFrame;
        renderQuad(target, currentInput, effect->materialName, effect->params, needsPrev);

        currentInput = usePingAsTarget ? mPingTex : mPongTex;
        usePingAsTarget = !usePingAsTarget;
    }

    // Output is the last written texture
    mOutputTex = currentInput;

    // Copy output to prevFrame RTT for next frame's feedback effects
    if (mPrevFrameTex && mOutputTex) {
        try {
            mPrevFrameTex->getBuffer()->blit(mOutputTex->getBuffer());
        } catch (...) {
            // Blit may fail on some drivers — not critical
        }
    }
}

void PostProcessStack::renderQuad(Ogre::RenderTexture* target,
                                   Ogre::TexturePtr source,
                                   const std::string& materialName,
                                   const std::map<std::string, float>& params,
                                   bool needsPrevFrame) {
    if (!target || !source || !mQuad || !mSceneMgr) return;

    auto* rs = Ogre::Root::getSingleton().getRenderSystem();
    if (!rs) return;

    // Get the material
    auto mat = Ogre::MaterialManager::getSingleton().getByName(
        materialName, Ogre::ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME);
    if (!mat) return;
    mat->load();

    auto* tech = mat->getBestTechnique();
    if (!tech || tech->getNumPasses() == 0) return;
    auto* pass = tech->getPass(0);

    // Bind the source texture to texture unit 0 (RT slot)
    if (pass->getNumTextureUnitStates() > 0) {
        auto* tus = pass->getTextureUnitState(0);
        tus->setTexture(source);
    }

    // Bind prevFrame to texture unit 1 if this effect uses feedback
    if (needsPrevFrame && mPrevFrameTex && pass->getNumTextureUnitStates() > 1) {
        auto* tusPrev = pass->getTextureUnitState(1);
        tusPrev->setTexture(mPrevFrameTex);
    }

    // Set fragment program parameters (time + user params)
    if (pass->hasFragmentProgram()) {
        auto fpParams = pass->getFragmentProgramParameters();
        if (fpParams) {
            try { fpParams->setNamedConstant("time", mTime); } catch (...) {}
            for (auto& [name, value] : params) {
                try { fpParams->setNamedConstant(name, value); } catch (...) {}
            }
        }
    }

    // Assign material to quad
    mQuad->setMaterial(mat);

    // Manual render: bind target, set viewport, render the quad
    rs->_setRenderTarget(target);
    rs->_setViewport(target->getViewport(0));

    // Clear the target
    rs->clearFrameBuffer(Ogre::FBT_COLOUR, Ogre::ColourValue::Black);

    // Set up identity matrices for fullscreen quad rendering
    // The quad is already in clip space [-1,1], so we use identity WVP
    mSceneMgr->_setPass(pass, false);

    // Get the render operation from the quad
    Ogre::RenderOperation rop;
    mQuad->getRenderOperation(rop);

    // Render with identity transforms (quad is in clip space)
    rs->_render(rop);
}

unsigned int PostProcessStack::getOutputGLID() const {
    if (!mOutputTex) return 0;
    unsigned int glId = 0;
    mOutputTex->getCustomAttribute("GLID", &glId);
    return glId;
}

} // namespace bbfx
