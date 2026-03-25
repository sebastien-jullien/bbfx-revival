#include "Engine.h"
#include <OgreOverlaySystem.h>
#include <OgreViewport.h>
#include <OgreCamera.h>
#include <OgreConfigFile.h>
#include <OgreResourceGroupManager.h>
#include <OgreRTShaderSystem.h>
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <vector>

namespace {
// RTSS material listener: auto-generates shaders for fixed-function materials
class ShaderGeneratorResolver : public Ogre::MaterialManager::Listener {
public:
    explicit ShaderGeneratorResolver(Ogre::RTShader::ShaderGenerator* sg) : mShaderGen(sg) {}

    Ogre::Technique* handleSchemeNotFound(unsigned short, const Ogre::String& schemeName,
        Ogre::Material* mat, unsigned short, const Ogre::Renderable*) override
    {
        if (schemeName != Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME)
            return nullptr;
        // Generate RTSS shaders for this material
        bool success = mShaderGen->createShaderBasedTechnique(
            *mat, Ogre::MaterialManager::DEFAULT_SCHEME_NAME, schemeName);
        if (success) {
            mShaderGen->validateMaterial(schemeName, mat->getName(), mat->getGroup());
            if (mat->getTechnique(0)->getPass(0)->hasVertexProgram()) {
                return mat->getTechnique(0);
            }
        }
        return nullptr;
    }
private:
    Ogre::RTShader::ShaderGenerator* mShaderGen;
};

static std::unique_ptr<ShaderGeneratorResolver> sShaderResolver;
} // anon namespace

namespace bbfx {

Engine* Engine::sInstance = nullptr;

Engine::Engine(sol::state& lua)
    : mLua(lua) {
    assert(!sInstance);
    sInstance = this;

    // SDL3 init
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)) {
        throw Exception(string("SDL_Init failed: ") + SDL_GetError());
    }

    mWindow = SDL_CreateWindow("BBFx v2", 800, 600, SDL_WINDOW_RESIZABLE);
    if (!mWindow) {
        SDL_Quit();
        throw Exception(string("SDL_CreateWindow failed: ") + SDL_GetError());
    }

    // OGRE init
    mRoot = new Ogre::Root("", "", "bbfx.log");
    loadOgrePlugins();

    Ogre::RenderSystem* rs = mRoot->getRenderSystemByName(BBFX_OGRE_RENDERER);
    if (!rs) {
        rs = mRoot->getRenderSystemByName("OpenGL 3+ Rendering Subsystem");
    }
    if (!rs) {
        throw Exception("No render system available");
    }
    mRoot->setRenderSystem(rs);
    mRoot->initialise(false);

    Ogre::NameValuePairList params;
    fillWindowParams(params);
    mRenderWindow = mRoot->createRenderWindow("BBFx", 800, 600, false, &params);

    mSceneManager = mRoot->createSceneManager("OctreeSceneManager");

    auto* overlaySystem = new Ogre::OverlaySystem();
    mSceneManager->addRenderQueueListener(overlaySystem);

    // Initialize RTSS (auto-generates shaders for fixed-function materials)
    if (Ogre::RTShader::ShaderGenerator::initialize()) {
        auto* shaderGen = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
        shaderGen->addSceneManager(mSceneManager);
        // Install listener: when OGRE encounters a material without shaders,
        // the RTSS generates vertex+fragment shaders automatically
        sShaderResolver = std::make_unique<ShaderGeneratorResolver>(shaderGen);
        Ogre::MaterialManager::getSingleton().addListener(sShaderResolver.get());
    }

    Ogre::Camera* cam = mSceneManager->createCamera("MainCamera");
    cam->setNearClipDistance(0.1f);
    cam->setAutoAspectRatio(true);
    auto* vp = mRenderWindow->addViewport(cam);
    vp->setBackgroundColour(Ogre::ColourValue(0.2f, 0.2f, 0.4f));

    // Load resources (meshes, materials, textures)
    loadResources();

    // Input manager
    mInputManager = new InputManager();

    cout << "OGRE " << OGRE_VERSION_MAJOR << "." << OGRE_VERSION_MINOR << "." << OGRE_VERSION_PATCH
         << " initialized. Renderer: " << rs->getName() << endl;
}

Engine::~Engine() {
    delete mInputManager;
    mInputManager = nullptr;
    delete mRoot;
    mRoot = nullptr;
    if (mWindow) {
        SDL_DestroyWindow(mWindow);
        mWindow = nullptr;
    }
    SDL_Quit();
    sInstance = nullptr;
    cout << "Clean exit." << endl;
}

Engine* Engine::instance() {
    return sInstance;
}

void Engine::loadOgrePlugins() {
    for (const auto& plugin : std::vector<std::string>(BBFX_OGRE_PLUGINS)) {
        mRoot->loadPlugin(plugin);
    }
}

void Engine::loadResources() {
    Ogre::ConfigFile cf;
    cf.load("resources.cfg");

    for (const auto& sec : cf.getSettingsBySection()) {
        for (const auto& setting : sec.second) {
            Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
                setting.second, setting.first, sec.first);
        }
    }

    Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();
}

void Engine::fillWindowParams(Ogre::NameValuePairList& params) {
    params["externalWindowHandle"] = getNativeWindowHandle(mWindow);
}

Ogre::SceneManager* Engine::getSceneManager() const {
    return mSceneManager;
}

Ogre::RenderWindow* Engine::getRenderWindow() const {
    return mRenderWindow;
}

void Engine::startRendering() {
    Animator* animator = Animator::instance();
    RootTimeNode* time = RootTimeNode::instance();

    if (time) {
        time->reset();
    }

    mStopQueued = false;
    SDL_Event evt;

    while (!mStopQueued) {
        // Input capture (once per frame, before events)
        if (mInputManager) {
            mInputManager->capture();
        }

        while (SDL_PollEvent(&evt)) {
            // Route to InputManager
            if (mInputManager) {
                mInputManager->handleSDLEvent(evt);
            }

            if (evt.type == SDL_EVENT_QUIT) {
                mStopQueued = true;
            }
            if (evt.type == SDL_EVENT_KEY_DOWN && evt.key.key == SDLK_ESCAPE) {
                mStopQueued = true;
            }
            if (evt.type == SDL_EVENT_WINDOW_RESIZED) {
                int w = evt.window.data1;
                int h = evt.window.data2;
                if (w > 0 && h > 0) {
                    mRenderWindow->resize(static_cast<unsigned>(w),
                                           static_cast<unsigned>(h));
                    mRenderWindow->windowMovedOrResized();
                }
            }
        }

        if (time) {
            time->update();
        }
        if (animator) {
            animator->renderOneFrame();
        }

        mRoot->renderOneFrame();
    }
}

void Engine::stopRendering() {
    mStopQueued = true;
}

} // namespace bbfx
