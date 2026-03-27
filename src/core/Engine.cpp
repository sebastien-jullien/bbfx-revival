#include "Engine.h"
#include "StatsOverlay.h"
#include "../record/VideoExporter.h"
#include <OgreOverlaySystem.h>
#include <OgreViewport.h>
#include <OgreCamera.h>
#include <OgreConfigFile.h>
#include <OgreResourceGroupManager.h>
#include <OgreRTShaderSystem.h>
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <vector>
#include <filesystem>

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
            // Return the RTSS-generated technique (last one added), not the original
            unsigned short numTechs = mat->getNumTechniques();
            for (short i = (short)numTechs - 1; i >= 0; --i) {
                Ogre::Technique* tech = mat->getTechnique((unsigned short)i);
                if (tech->getSchemeName() == schemeName) {
                    return tech;
                }
            }
        }
        return nullptr;
    }
private:
    Ogre::RTShader::ShaderGenerator* mShaderGen;
};

static std::unique_ptr<ShaderGeneratorResolver> sShaderResolver;

std::filesystem::path findProjectRoot(const std::filesystem::path& start) {
    auto current = std::filesystem::absolute(start);
    while (!current.empty()) {
        if (std::filesystem::exists(current / "resources.cfg") &&
            std::filesystem::exists(current / "lua") &&
            std::filesystem::exists(current / "src")) {
            return current;
        }
        auto parent = current.parent_path();
        if (parent == current) break;
        current = parent;
    }
    return {};
}

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

    // Stats overlay (must be after OverlaySystem)
    new StatsOverlay();

    // Input manager
    mInputManager = new InputManager();

    cout << "OGRE " << OGRE_VERSION_MAJOR << "." << OGRE_VERSION_MINOR << "." << OGRE_VERSION_PATCH
         << " initialized. Renderer: " << rs->getName() << endl;
}

Engine::~Engine() {
    delete StatsOverlay::instance();
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
    auto exeDir = std::filesystem::current_path();
    auto projectRoot = findProjectRoot(exeDir);
    auto configPath = !projectRoot.empty()
        ? (projectRoot / "resources.cfg")
        : (exeDir / "resources.cfg");

    Ogre::ConfigFile cf;
    cf.load(configPath.string());

    for (const auto& sec : cf.getSettingsBySection()) {
        for (const auto& setting : sec.second) {
            auto resourcePath = std::filesystem::path(setting.second);
            if (!resourcePath.is_absolute()) {
                if (!projectRoot.empty()) {
                    resourcePath = projectRoot / resourcePath;
                } else {
                    resourcePath = exeDir / resourcePath;
                }
            }
            Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
                resourcePath.string(), setting.first, sec.first);
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
            if (evt.type == SDL_EVENT_KEY_DOWN && evt.key.key == SDLK_F3) {
                if (auto* stats = StatsOverlay::instance()) stats->toggle();
            }
            if (evt.type == SDL_EVENT_KEY_DOWN && evt.key.key == SDLK_F11) {
                toggleFullscreen();
            }
            if (evt.type == SDL_EVENT_KEY_DOWN && evt.key.key == SDLK_F12) {
                screenshot();
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

        // Update stats overlay (use frame time from time node output port)
        if (auto* stats = StatsOverlay::instance()) {
            float dt = 0.016f;
            if (time) {
                auto& outputs = time->getOutputs();
                auto it = outputs.find("dt");
                if (it != outputs.end()) dt = it->second->getValue();
            }
            stats->update(dt);
        }

        mRoot->renderOneFrame();

        // Capture only after OGRE has rendered the backbuffer for this frame.
        if (mVideoExporter && mVideoExporter->isExporting()) {
            mVideoExporter->captureFrame(mRenderWindow);
        }
    }
}

void Engine::stopRendering() {
    mStopQueued = true;
}

void Engine::setOfflineMode(int fps) {
    mOfflineMode = true;
    mOfflineDt = 1.0f / static_cast<float>(fps);
    std::cout << "[Engine] Offline mode: " << fps << " fps (dt=" << mOfflineDt << ")" << std::endl;
}

void Engine::setOnlineMode() {
    mOfflineMode = false;
    std::cout << "[Engine] Online mode (real-time)" << std::endl;
}

void Engine::setVideoExporter(VideoExporter* exporter) {
    mVideoExporter = exporter;
}

void Engine::clearVideoExporter(VideoExporter* exporter) {
    if (!exporter || mVideoExporter == exporter) {
        mVideoExporter = nullptr;
    }
}

void Engine::screenshot() {
    if (mRenderWindow) {
        Ogre::String filename = mRenderWindow->writeContentsToTimestampedFile("screenshot_", ".png");
        cout << "[screenshot] Saved: " << filename << endl;
    }
}

void Engine::toggleFullscreen() {
    mFullscreen = !mFullscreen;
    if (mWindow) {
        SDL_SetWindowFullscreen(mWindow, mFullscreen ? true : false);
        if (mRenderWindow) {
            mRenderWindow->windowMovedOrResized();
        }
    }
}

} // namespace bbfx
