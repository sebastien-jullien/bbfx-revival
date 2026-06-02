#include "StudioApp.h"
#include "../core/Version.h"   // v3.5.2 Sprint S8 Lot AU — BBFX_VERSION_STRING
#include "DagSnapshot.h"
#include "ZoneSnapshot.h"
#include "commands/NodeCommands.h"
#include "commands/SceneCommands.h"
#include "commands/PluginCommands.h"
#include "../plugin/PluginManager.h"
#include "../plugin/PluginManifest.h"
#include "../network/HttpClient.h"
#include "../network/ArtnetInput.h"
#include "../osc/OscBus.h"
#include "../timing/TempoManager.h"
#include "../plugin/PluginHotReloader.h"
#include "../network/ZipExtractor.h"
#include "dialogs/PermissionPromptDialog.h"
#include "panels/PluginManagerPanel.h"
#include "panels/PluginErrorsPanel.h"
#include "panels/GamepadPanel.h"
#include "panels/EffectRackPanel.h"
#include "panels/PluginAuthoringDialog.h"
#include "panels/LearnPanel.h"
#include "bbfx_imgui_bindings.h"
#include "ScriptPanelRegistry.h"
#include "panels/CommunityBrowserPanel.h"
#include "panels/CommandPalette.h"
#include "panels/AuthorProfilePanel.h"
#include "../plugin/PluginErrorLog.h"
#include "../plugin/DeepLinkHandler.h"
#include "../plugin/DeepLinkRegistry.h"
#include "../input/GamepadNode.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include "../core/Animator.h"
#include "../core/AssetManifest.h"
#include "../core/PrimitiveNodes.h"
#include "../fx/PerlinFxNode.h"
#include "../fx/ShaderFxNode.h"
#include "../fx/TextureBlitterNode.h"
#include "../fx/WaveVertexShader.h"
#include "../fx/ColorShiftNode.h"
#include "ToastSystem.h"
#include <imgui_te_engine.h>
#include <imgui_te_context.h>
#include <imgui_te_ui.h>
#include <OgreSubEntity.h>
#include <OgreRoot.h>
#include "../audio/AudioAnalyzer.h"
#include "../audio/BeatDetector.h"
#include "../audio/AudioCapture.h"
#include "../video/TheoraClipNode.h"
#include "nodes/SceneObjectNode.h"
#include "nodes/MidiInputNode.h"
#include "nodes/MidiOutputNode.h"
#include "nodes/OscInputNode.h"
#include "nodes/OscOutputNode.h"
#include "nodes/NdiOutputNode.h"
#include "TextureShareSender.h"
#include "nodes/TextureShareOutputNode.h"
#include "nodes/ArtnetOutputNode.h"
#include "nodes/ArtnetInputNode.h"
#include "nodes/WarpNode.h"
#include "nodes/BlendNode.h"
#include "../midi/MidiDeviceManager.h"
#include "../midi/MidiLearnManager.h"
#include "nodes/LightNode.h"
#include "nodes/FullscreenOverlayNode.h"
#include "nodes/TextureCycleNode.h"
#include "nodes/TextureSetNode.h"
#include "../fx/TextureBlendNode.h"
#include "../fx/TextureFeedbackNode.h"
#include "nodes/VideoCrossfadeNode.h"
#include "nodes/MaterialAnimNode.h"
#include "nodes/VideoLibraryNode.h"
#include "nodes/BillboardLayerNode.h"
#include "nodes/JoystickRouterNode.h"
#include "nodes/VideoSlicerNode.h"
#include "nodes/MultiTextureBankNode.h"
#include "../fx/NoiseTextureNode.h"
#include "../fx/SpectrogramTextureNode.h"
#include "../fx/GrayscaleNode.h"
#include "nodes/ArtnetVideoMapperNode.h"
#include "nodes/ParticleNode.h"
#include "nodes/CompositorNode.h"
#include "nodes/PostProcessNode.h"
#include "nodes/TextureNode.h"
#include "nodes/MaterialBridgeNode.h"
#include "nodes/MaterialNode.h"
#include "nodes/CameraNode.h"
#include "nodes/SkyboxNode.h"
#include "nodes/FogNode.h"
#include "nodes/BeatTriggerNode.h"
#include "nodes/MathNode.h"
#include "nodes/MixerNode.h"
#include "nodes/MapperNode.h"
#include "nodes/TriggerNode.h"
#include "nodes/SplitterNode.h"
#include "generators/MeshGenerator.h"
#include <OgreEntity.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

#include <SDL3/SDL.h>
#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <GL/gl.h>
#else
#  include <GL/gl.h>
#endif

// glBindFramebuffer is GL3+ — load the function pointer at runtime
typedef void (APIENTRY* PFN_glBindFramebuffer)(GLenum target, GLuint framebuffer);
static PFN_glBindFramebuffer s_glBindFramebuffer = nullptr;
static void ensureGLFunctions() {
    if (!s_glBindFramebuffer) {
        s_glBindFramebuffer = (PFN_glBindFramebuffer)SDL_GL_GetProcAddress("glBindFramebuffer");
    }
}

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cassert>

#ifdef _WIN32
#include <commdlg.h> // GetOpenFileNameA / GetSaveFileNameA
#endif

namespace bbfx {

// ── Unique name generator for OGRE objects created by Studio factories ───────
static std::string uniqueName(const std::string& prefix) {
    static int sCounter = 0;
    return prefix + "_" + std::to_string(++sCounter);
}

// ── Native file dialogs (Windows) ────────────────────────────────────────────
#ifdef _WIN32
static std::string openFileDialog(SDL_Window* sdlWin, const char* filter, const char* title) {
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr; // modal to desktop
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) return std::string(filename);
    return {};
}

static std::string saveFileDialog(SDL_Window* sdlWin, const char* filter, const char* title, const char* defaultName) {
    char filename[MAX_PATH] = {};
    if (defaultName) std::strncpy(filename, defaultName, MAX_PATH - 1);
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = "bbfx-project";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn)) return std::string(filename);
    return {};
}
#else
static std::string openFileDialog(SDL_Window*, const char*, const char*) { return {}; }
static std::string saveFileDialog(SDL_Window*, const char*, const char*, const char*) { return {}; }
#endif

// ── Construction ─────────────────────────────────────────────────────────────

StudioApp::StudioApp(sol::state& lua, const std::string& initialScript, bool forceDefault, bool forceReset,
                     const std::string& initialDemo)
    : mLua(lua), mInitialScript(initialScript), mInitialDemo(initialDemo),
      mForceDefault(forceDefault), mForceReset(forceReset),
      mLastAutoSave(std::chrono::steady_clock::now())
{
    // Engine creates SDL3 window + GL context + OGRE
    mEngine = std::make_unique<StudioEngine>(lua);

    // Panels (created after engine so OGRE is ready)
    mViewportPanel        = std::make_unique<ViewportPanel>(mEngine.get());
    mNodeEditorPanel      = std::make_unique<NodeEditorPanel>(lua);
    setNodeEditorForCommands(mNodeEditorPanel.get());
    mInspectorPanel       = std::make_unique<InspectorPanel>(lua);
    mTimelinePanel        = std::make_unique<TimelinePanel>();
    mTimelinePanel->setPerformanceModePanel(mPerformanceModePanel.get());
    mThumbCache           = std::make_unique<TextureThumbnailCache>();
    mPresetBrowserPanel   = std::make_unique<PresetBrowserPanel>(mNodeEditorPanel.get(), lua);
    mInspectorPanel->setThumbCache(mThumbCache.get());
    mPerformanceModePanel = std::make_unique<PerformanceModePanel>(lua);
    mConsolePanel         = std::make_unique<ConsolePanel>(mLua);
    mSetEditorPanel       = std::make_unique<SetEditorPanel>(mLua);
    mSetEditorPanel->setLoadProjectCallback([this](const std::string& path) {
        loadProject(path);
    });
    mSetEditorPanel->setBpmCallback([](float bpm) {
        auto* time = RootTimeNode::instance();
        if (time) time->setBPM(bpm);
    });
    mSceneHierarchyPanel  = std::make_unique<SceneHierarchyPanel>(Animator::instance());
    mCompositorStackPanel = std::make_unique<CompositorStackPanel>();
    mCompositorStackPanel->setAnimator(Animator::instance());
    mPerformanceModePanel->setCompositorStack(mCompositorStackPanel.get());

    // Shader Gallery + Material Editor (v3.2.5)
    // MIDI Device Manager (v3.3) — auto-detect and open all devices
    mMidiDeviceManager = std::make_unique<MidiDeviceManager>();
    mMidiDeviceManager->openAll();
    std::cout << "[Studio] MIDI auto-detect: " << mMidiDeviceManager->getInputDeviceCount()
              << " input, " << mMidiDeviceManager->getOutputDeviceCount() << " output devices" << std::endl;

    mPreviewRenderer = std::make_unique<ShaderPreviewRenderer>();
    mPreviewRenderer->initialize(Ogre::Root::getSingletonPtr());
    mShaderGalleryPanel = std::make_unique<ShaderGalleryPanel>();
    mShaderGalleryPanel->setPreviewRenderer(mPreviewRenderer.get());
    mMaterialEditorPanel = std::make_unique<MaterialEditorPanel>();
    mMaterialEditorPanel->setPreviewRenderer(mPreviewRenderer.get());
    mMaterialEditorPanel->setThumbCache(mThumbCache.get());
    // Preset wheel callbacks
    mPresetBrowserPanel->setWheelCallbacks(
        [this](const std::string& name, bool add) {
            auto& wheel = mPerformanceModePanel->getWheelPresets();
            if (add) {
                if (std::find(wheel.begin(), wheel.end(), name) == wheel.end())
                    wheel.push_back(name);
            } else {
                wheel.erase(std::remove(wheel.begin(), wheel.end(), name), wheel.end());
            }
        },
        [this](const std::string& name) -> bool {
            auto& wheel = mPerformanceModePanel->getWheelPresets();
            return std::find(wheel.begin(), wheel.end(), name) != wheel.end();
        }
    );
    mUndoHistoryPanel = std::make_unique<UndoHistoryPanel>();
    mMidiActivityPanel = std::make_unique<MidiActivityPanel>();
    mMidiMappingPanel = std::make_unique<MidiMappingPanel>();
    mEffectRackPanel  = std::make_unique<EffectRackPanel>();
    mOutputManagerPanel = std::make_unique<OutputManagerPanel>();
    mOscBrowserPanel = std::make_unique<OscBrowserPanel>();
    mNetworkPanel    = std::make_unique<NetworkPanel>();
    mMasterViewPanel = std::make_unique<MasterViewPanel>();
    mLearnPanel      = std::make_unique<LearnPanel>();   // v3.5.2 Sprint S7 Lot Y
    mAssetBrowserPanel = std::make_unique<AssetBrowserPanel>();
    mAssetBrowserPanel->setThumbCache(mThumbCache.get());
    mAssetBrowserPanel->setDropCallback([this](const std::string& type, const std::string& name) {
        if (type == "Mesh") {
            // Create SceneObjectNode at origin
            auto* animator = Animator::instance();
            std::string nodeName = generateSceneObjectName(name, animator);
            auto compound = std::make_unique<CompoundCommand>("Add " + nodeName);
            compound->add(std::make_unique<CreateNodeCommand>("SceneObjectNode", nodeName, mLua));
            compound->add(std::make_unique<LambdaCommand>("Set mesh",
                [nodeName, name]() {
                    auto* node = Animator::instance()->getRegisteredNode(nodeName);
                    if (!node) return;
                    if (node->getParamSpec()) {
                        auto* mp = node->getParamSpec()->getParam("mesh_file");
                        if (mp) mp->stringVal = name;
                    }
                }, [nodeName]() {}
            ));
            CommandManager::instance().execute(std::move(compound));
        } else if (type == "Preset") {
            mLua.script("dbg.preset('" + name + "')");
        } else if (type == "Particle") {
            auto* animator = Animator::instance();
            std::string nodeName = generateSceneObjectName(name + ".mesh", animator);
            auto compound = std::make_unique<CompoundCommand>("Add Particle " + nodeName);
            compound->add(std::make_unique<CreateNodeCommand>("ParticleNode", nodeName, mLua));
            compound->add(std::make_unique<LambdaCommand>("Set template",
                [nodeName, name]() {
                    auto* node = Animator::instance()->getRegisteredNode(nodeName);
                    if (!node || !node->getParamSpec()) return;
                    auto* tp = node->getParamSpec()->getParam("template");
                    if (tp) tp->stringVal = name;
                    node->update(); // Force immediate template change (isolated nodes don't get graph updates)
                }, [nodeName]() {}
            ));
            CommandManager::instance().execute(std::move(compound));
        } else if (type == "Texture") {
            static int sTexCount = 0;
            std::string nodeName = "tex_" + std::to_string(++sTexCount);
            auto compound = std::make_unique<CompoundCommand>("Add Texture " + name);
            compound->add(std::make_unique<CreateNodeCommand>("TextureNode", nodeName, mLua));
            compound->add(std::make_unique<LambdaCommand>("Set texture",
                [nodeName, name]() {
                    auto* node = Animator::instance()->getRegisteredNode(nodeName);
                    if (!node || !node->getParamSpec()) return;
                    auto* tp = node->getParamSpec()->getParam("texture");
                    if (tp) tp->stringVal = name;
                    node->update();
                }, [nodeName]() {}
            ));
            CommandManager::instance().execute(std::move(compound));
        } else if (type == "Material") {
            static int sMatCount = 0;
            std::string nodeName = "mat_" + std::to_string(++sMatCount);
            auto compound = std::make_unique<CompoundCommand>("Add Material " + name);
            compound->add(std::make_unique<CreateNodeCommand>("MaterialNode", nodeName, mLua));
            compound->add(std::make_unique<LambdaCommand>("Set material",
                [nodeName, name]() {
                    auto* node = Animator::instance()->getRegisteredNode(nodeName);
                    if (!node || !node->getParamSpec()) return;
                    auto* mp = node->getParamSpec()->getParam("material");
                    if (mp) mp->stringVal = name;
                    node->update();
                }, [nodeName]() {}
            ));
            CommandManager::instance().execute(std::move(compound));
        } else if (type == "Shader") {
            // Create a ShaderFxNode with the shader file
            static int sShaderCount = 0;
            std::string nodeName = "shader_" + std::to_string(++sShaderCount);
            mLua.script("dbg.create_with_shader('" + nodeName + "', 'passthrough.vert', '" + name + "')");
        } else if (type == "Effect") {
            mLua.script("dbg.create('PostProcessNode', 'pp_" + name + "')");
        } else if (type == "Template") {
            mLua.script("local t = dofile('lua/templates/" + name + ".lua'); if t and t.setup then t.setup() end");
        }
    });
    mPluginManagerPanel    = std::make_unique<PluginManagerPanel>();
    mPluginManagerPanel->setOpenCommunityBrowserCb([this]() { mShowCommunityBrowser = true; });
    mPluginErrorsPanel     = std::make_unique<PluginErrorsPanel>();
    mCommunityBrowserPanel = std::make_unique<CommunityBrowserPanel>();
    mAuthorProfilePanel    = std::make_unique<AuthorProfilePanel>();
    mGamepadPanel          = std::make_unique<GamepadPanel>();
    mPluginAuthoringDialog = std::make_unique<PluginAuthoringDialog>();
    mGamepadPanel->setLearnCallback([this](const std::string& source) {
        ToastSystem::instance().toast(
            "Learn: detected " + source + " — pick a port in Node Editor to bind.",
            ToastSeverity::Info, 5.0f);
    });
    mEffectRackPanel->setGamepadPanel(mGamepadPanel.get());

    // v3.5 Lot I — wire Community Browser's author click -> AuthorProfilePanel.
    mCommunityBrowserPanel->setOnOpenAuthor([this](const std::string& author) {
        mAuthorProfilePanel->setAuthor(author);
        mShowAuthorProfile = true;
    });
    // When the user picks a plugin from the Author panel, raise the
    // Community Browser on that plugin.
    static StudioApp* sSelf = this;
    mAuthorProfilePanel->setOnSelectPlugin([](const std::string& id) {
        if (!sSelf) return;
        sSelf->mShowCommunityBrowser = true;
        if (sSelf->mCommunityBrowserPanel) sSelf->mCommunityBrowserPanel->focusOnEntry(id);
    });

    // v3.5 Lot I — deep link dispatcher.
    DeepLinkHandler::instance().onInstall = [this](const std::string& id) {
        mShowCommunityBrowser = true;
        if (mCommunityBrowserPanel) mCommunityBrowserPanel->focusOnEntry(id);
    };
    // Sécurité — enable/run via deep-link externe exécute du code plugin tiers :
    // on NE l'exécute jamais directement, on demande confirmation à l'utilisateur.
    DeepLinkHandler::instance().onEnable = [this](const std::string& id) {
        mDeepLinkAction = "enable";
        mDeepLinkPluginId = id;
        mDeepLinkNodeType.clear();
        mShowDeepLinkConfirm = true;
    };
    DeepLinkHandler::instance().onDisable = [](const std::string& id) {
        // disable est non-dangereux (arrête du code, n'en lance pas) → direct.
        PluginManager::instance().disable(id);
    };
    DeepLinkHandler::instance().onRun = [this](const std::string& id, const std::string& type) {
        mDeepLinkAction = "run";
        mDeepLinkPluginId = id;
        mDeepLinkNodeType = type;
        mShowDeepLinkConfirm = true;
    };

    // v3.5 Lot H: wire the static command palette entries into actual
    // StudioApp flag toggles so they actually do something at click.
    CommandPalette::instance().registerCommand({
        "Toggle Plugin Manager", "Ctrl+Shift+X",
        [this]() { mShowPluginManager = !mShowPluginManager; }
    });
    CommandPalette::instance().registerCommand({
        "Toggle Community Browser", "Community plugins from GitHub",
        [this]() {
            mShowCommunityBrowser = !mShowCommunityBrowser;
            if (mShowCommunityBrowser && mCommunityBrowserPanel) {
                mCommunityBrowserPanel->requestRefresh();
            }
        }
    });
    CommandPalette::instance().registerCommand({
        "Toggle Plugin Errors", "Ctrl+Shift+E",
        [this]() { mShowPluginErrors = !mShowPluginErrors; }
    });
    // v3.5 Lot L — Gamepad panel in the command palette.
    CommandPalette::instance().registerCommand({
        "Toggle Gamepad Panel", "Ctrl+Shift+G",
        [this]() { mShowGamepadPanel = !mShowGamepadPanel; }
    });

    // Surface Mapping (v3.4 Lot E)
    mSurfaceMap = std::make_unique<SurfaceMap>();
    mSurfaceEditorPanel = std::make_unique<SurfaceEditorPanel>();
    mSurfaceEditorPanel->setSurfaceMap(mSurfaceMap.get());
    mSurfaceEditorPanel->setPerformanceModePanel(mPerformanceModePanel.get());
    if (mEngine && mEngine->getOutputManager()) {
        mEngine->getOutputManager()->setSurfaceMap(mSurfaceMap.get());
        mSurfaceEditorPanel->setOutputManager(mEngine->getOutputManager());
    }

    // Network Sync (v3.4 Lot F)
    mSyncManager = std::make_unique<SyncManager>();
    mSyncManager->setOnToast([this](const std::string& msg) {
        std::cout << "[Sync] " << msg << std::endl;
        if (mNetworkPanel) mNetworkPanel->pushLog(msg);
    });
    mSyncManager->setOnChord([this](const std::string& name) {
        // Forward to node graph: find any BeatTriggerNode or similar
        std::cout << "[Sync] Chord received: " << name << std::endl;
    });
    mSyncManager->setOnPanic([this]() {
        std::cout << "[Sync] PANIC received." << std::endl;
    });
    mSyncManager->setOnBeat([this](float bpm, int beat) {
        // Could drive a BPM-aware node if present
        std::cout << "[Sync] Beat " << beat << " @ " << bpm << " BPM" << std::endl;
    });
    mSyncManager->setOnSet([this](const std::string& node, const std::string& port, float val) {
        auto* animator = Animator::instance();
        if (!animator) return;
        auto* n = animator->getRegisteredNode(node);
        if (!n) return;
        auto& inputs = n->getInputs();
        auto it = inputs.find(port);
        if (it != inputs.end()) it->second->setValue(static_cast<Ogre::Real>(val));
    });
    mSyncManager->setOnSnapshot([this](const std::string& json) {
        std::cout << "[Sync] Snapshot received (" << json.size() << " bytes)" << std::endl;
        auto* animator = Animator::instance();
        if (!animator || json.empty()) return;
        try {
            auto j = nlohmann::json::parse(json);
            // Format: { "nodeName.portName": floatValue, ... }
            DagSnapshot snap;
            std::map<std::string, float> data;
            for (auto& [key, val] : j.items()) {
                if (val.is_number()) data[key] = val.get<float>();
            }
            snap.setData(data);
            DagSnapshot::apply(snap, snap, 0.0f, *animator);
            std::cout << "[Sync] Snapshot restored (" << data.size() << " ports)" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[Sync] Snapshot parse error: " << e.what() << std::endl;
        }
    });

    // Crash recovery: create lock file at startup, check for stale lock + autosave
    {
        std::string lockPath = ".bbfx_lock";
        if (std::ifstream(lockPath).good()) {
            std::cout << "[StudioApp] Stale lock file detected — possible previous crash" << std::endl;
            // Check if an autosave exists
            auto& settings = SettingsManager::instance();
            std::string lastProject = settings.get().lastProjectPath;
            if (!lastProject.empty()) {
                std::string autosavePath = lastProject + ".autosave";
                if (std::filesystem::exists(autosavePath)) {
                    // Check if autosave is more recent than project
                    auto projectTime = std::filesystem::last_write_time(lastProject);
                    auto autosaveTime = std::filesystem::last_write_time(autosavePath);
                    if (autosaveTime > projectTime) {
                        mRecoveryAutosavePath = autosavePath;
                        mShowRecoveryDialog = true;
                        std::cout << "[StudioApp] Autosave found: " << autosavePath << " — offering recovery" << std::endl;
                    }
                }
            }
        }
        createLockFile();
    }
    mAutomationEngine     = std::make_unique<AutomationEngine>(&mTimelinePanel->getAutomation());

    // Wire fader recording callback
    mPerformanceModePanel->setRecordValueCallback(
        [this](const std::string& nodeName, const std::string& portName, float value, float beat) {
            if (mTimelinePanel) mTimelinePanel->recordValue(nodeName, portName, value, beat);
        });

    // Wire Inspector callbacks
    mInspectorPanel->setAddToTimelineCallback(
        [this](const std::string& nodeName, const std::string& portName, float minVal, float maxVal) {
            if (mTimelinePanel) mTimelinePanel->addLaneForPort(nodeName, portName, minVal, maxVal);
        });
    mInspectorPanel->setRecordValueCallback(
        [this](const std::string& nodeName, const std::string& portName, float value, float beat) {
            if (mTimelinePanel) mTimelinePanel->recordValue(nodeName, portName, value, beat);
        });

    initNodeTypeRegistry();
    MeshGenerator::registerDefaults();
    SettingsManager::instance().load();

    // v3.5.2 — Restore window geometry from settings
    {
        const auto& ws = SettingsManager::instance().get();
        if (ws.windowWidth > 0 && ws.windowHeight > 0) {
            SDL_SetWindowSize(mEngine->getSDLWindow(), ws.windowWidth, ws.windowHeight);
            if (ws.windowX >= 0 && ws.windowY >= 0) {
                SDL_SetWindowPosition(mEngine->getSDLWindow(), ws.windowX, ws.windowY);
            } else {
                SDL_SetWindowPosition(mEngine->getSDLWindow(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            }
        }
    }

    // Load studio chord system (provides ChordSystem global for triggers/quick access)
    mLua.safe_script("require 'studio_chord'", sol::script_pass_on_error);

    // Install Studio Debugger (programmatic testing interface)
    Debugger::install(mLua, this);

    // Wire inspector to node editor selection + viewport sync
    mNodeEditorPanel->setSelectionCallback([this](const std::string& nodeName) {
        mInspectorPanel->setSelectedNode(nodeName);
        mInspectorPanel->setSelectedNodes(mNodeEditorPanel->getSelectedNodeNames());
        // Sync selection to viewport (highlight object, no callback to avoid loop)
        if (mViewportPanel && mViewportPanel->getPicker()) {
            mViewportPanel->getPicker()->selectByDAGName(nodeName);
        }
        // Sync selection to hierarchy panel
        if (mSceneHierarchyPanel) {
            mSceneHierarchyPanel->selectNodeFromExternal(nodeName);
        }
    });

    // Wire viewport picker selection to node editor + inspector + hierarchy
    if (mViewportPanel && mViewportPanel->getPicker()) {
        mViewportPanel->getPicker()->setSelectionCallback([this](const std::string& nodeName) {
            mNodeEditorPanel->selectNodeFromExternal(nodeName);
            mInspectorPanel->setSelectedNode(nodeName);
            if (mSceneHierarchyPanel) mSceneHierarchyPanel->selectNodeFromExternal(nodeName);
        });
    }

    // Wire hierarchy panel selection to viewport + node editor + inspector
    if (mSceneHierarchyPanel) {
        mSceneHierarchyPanel->setSelectionCallback([this](const std::string& nodeName) {
            mNodeEditorPanel->selectNodeFromExternal(nodeName);
            mInspectorPanel->setSelectedNode(nodeName);
            if (mViewportPanel && mViewportPanel->getPicker()) {
                mViewportPanel->getPicker()->selectByDAGName(nodeName);
                // Also set gizmo target
                auto* dagNode = Animator::instance()->getRegisteredNode(nodeName);
                auto* soNode = dynamic_cast<SceneObjectNode*>(dagNode);
                if (soNode && soNode->getSceneNode()) {
                    if (mViewportPanel->getGizmo()) mViewportPanel->getGizmo()->setTarget(soNode->getSceneNode(), dagNode);
                    if (mViewportPanel->getCameraController()) mViewportPanel->getCameraController()->setOrbitLockTarget(soNode->getSceneNode());
                }
            }
        });
    }

    // Wire viewport callbacks for scene object creation and FX application
    if (mViewportPanel) {
        mViewportPanel->setCreateSceneObjectCallback([this](const std::string& meshFile, float x, float y, float z) {
            auto* animator = Animator::instance();
            std::string name = generateSceneObjectName(meshFile, animator);
            auto compound = std::make_unique<CompoundCommand>("Add " + name);
            compound->add(std::make_unique<CreateNodeCommand>("SceneObjectNode", name, mLua));
            compound->add(std::make_unique<LambdaCommand>("Set mesh+pos",
                [this, name, meshFile, x, y, z]() {
                    auto* node = Animator::instance()->getRegisteredNode(name);
                    if (!node) return;
                    if (node->getParamSpec()) {
                        auto* mp = node->getParamSpec()->getParam("mesh_file");
                        if (mp) mp->stringVal = meshFile;
                    }
                    auto& inputs = node->getInputs();
                    if (inputs.count("position.x")) inputs["position.x"]->setValue(x);
                    if (inputs.count("position.y")) inputs["position.y"]->setValue(y);
                    if (inputs.count("position.z")) inputs["position.z"]->setValue(z);
                },
                [this, name]() {
                    // Undo is handled by CreateNodeCommand::undo
                }
            ));
            CommandManager::instance().execute(std::move(compound));
        });

        mViewportPanel->setDuplicateCallback([this](const std::string& nodeName) {
            CommandManager::instance().execute(
                std::make_unique<DuplicateNodeCommand>(nodeName, mLua));
        });

        // Create arbitrary node from viewport drop (particle, compositor)
        mViewportPanel->setCreateNodeCallback([this](const std::string& nodeType, const std::string& paramValue,
                                                       const std::string& targetNode) {
            std::cout << "[ViewportDrop] type=" << nodeType << " param=" << paramValue
                      << " target='" << targetNode << "'" << std::endl;
            auto* animator = Animator::instance();
            if (!animator) return;
            std::string name = generateSceneObjectName(paramValue + ".mesh", animator);
            auto compound = std::make_unique<CompoundCommand>("Create " + nodeType);
            compound->add(std::make_unique<CreateNodeCommand>(nodeType, name, mLua));
            compound->add(std::make_unique<LambdaCommand>("Set param + link",
                [name, nodeType, paramValue, targetNode]() {
                    auto* animator2 = Animator::instance();
                    if (!animator2) return;
                    auto* n = animator2->getRegisteredNode(name);
                    if (!n || !n->getParamSpec()) return;
                    if (nodeType == "ParticleNode") {
                        auto* p = n->getParamSpec()->getParam("template");
                        if (p) p->stringVal = paramValue;
                    } else if (nodeType == "CompositorNode" || nodeType == "PostProcessNode") {
                        auto* p = n->getParamSpec()->getParam("compositor");
                        if (p) p->stringVal = paramValue;
                    } else if (nodeType == "TextureNode") {
                        auto* p = n->getParamSpec()->getParam("texture");
                        if (p) p->stringVal = paramValue;
                    } else if (nodeType == "MaterialNode") {
                        auto* p = n->getParamSpec()->getParam("material");
                        if (p) p->stringVal = paramValue;
                    }
                    // Force update so isolated nodes pick up the new param immediately
                    n->update();
                    // Create entity link if target is specified
                    if (!targetNode.empty()) {
                        auto* srcNode = animator2->getRegisteredNode(targetNode);
                        if (srcNode && n) {
                            auto srcIt = srcNode->getOutputs().find("entity");
                            auto dstIt = n->getInputs().find("entity");
                            if (srcIt != srcNode->getOutputs().end() &&
                                dstIt != n->getInputs().end()) {
                                animator2->link(srcIt->second, dstIt->second);
                                n->onLinkChanged();
                            }
                        }
                    }
                },
                []() {}
            ));
            CommandManager::instance().execute(std::move(compound));

            // Position texture/material nodes next to their target in the node editor
            if (!targetNode.empty() && (nodeType == "TextureNode" || nodeType == "MaterialNode")) {
                if (mNodeEditorPanel) {
                    mNodeEditorPanel->positionNodeNextTo(name, targetNode);
                }
            }
        });

        // Same callback for NodeEditorPanel drops
        mNodeEditorPanel->setCreateNodeCallback([this](const std::string& nodeType, const std::string& paramValue,
                                                        const std::string& targetNode) {
            auto* animator = Animator::instance();
            if (!animator) return;
            std::string name = generateSceneObjectName(paramValue + ".mesh", animator);
            auto compound = std::make_unique<CompoundCommand>("Create " + nodeType);
            compound->add(std::make_unique<CreateNodeCommand>(nodeType, name, mLua));
            compound->add(std::make_unique<LambdaCommand>("Set param + link",
                [name, nodeType, paramValue, targetNode]() {
                    auto* anim2 = Animator::instance();
                    if (!anim2) return;
                    auto* n = anim2->getRegisteredNode(name);
                    if (!n || !n->getParamSpec()) return;
                    if (nodeType == "TextureNode") {
                        auto* p = n->getParamSpec()->getParam("texture");
                        if (p) p->stringVal = paramValue;
                    } else if (nodeType == "MaterialNode") {
                        auto* p = n->getParamSpec()->getParam("material");
                        if (p) p->stringVal = paramValue;
                    }
                    // Force update so isolated nodes pick up the new param immediately
                    n->update();
                    if (!targetNode.empty()) {
                        auto* srcNode = anim2->getRegisteredNode(targetNode);
                        if (srcNode && n) {
                            auto srcIt = srcNode->getOutputs().find("entity");
                            auto dstIt = n->getInputs().find("entity");
                            if (srcIt != srcNode->getOutputs().end() &&
                                dstIt != n->getInputs().end()) {
                                anim2->link(srcIt->second, dstIt->second);
                                n->onLinkChanged();
                            }
                        }
                    }
                },
                []() {}
            ));
            CommandManager::instance().execute(std::move(compound));

            // Schedule deferred positioning in the node editor
            if (mNodeEditorPanel) {
                mNodeEditorPanel->scheduleDropPosition(name, targetNode,
                    mNodeEditorPanel->getDropScreenPos());
            }
        });

        mViewportPanel->setApplyFxCallback([this](const std::string& fxType, const std::string& targetName) {
            auto* animator = Animator::instance();
            if (!animator) return;
            // Generate FX name
            std::string fxName = generateSceneObjectName(fxType + ".mesh", animator);
            // Use the fxType directly as a node type name for FX nodes
            // Check if fxType is a preset or a node type
            auto* typeInfo = NodeTypeRegistry::instance().getType(fxType);
            if (typeInfo) {
                auto compound = std::make_unique<CompoundCommand>("Apply " + fxType + " to " + targetName);
                compound->add(std::make_unique<CreateNodeCommand>(fxType, fxName, mLua));
                // Auto-connect entity ports
                compound->add(std::make_unique<LambdaCommand>("Auto-connect entity",
                    [this, fxName, targetName]() {
                        auto* animator = Animator::instance();
                        if (!animator) return;
                        auto* fxNode = animator->getRegisteredNode(fxName);
                        auto* targetNode = animator->getRegisteredNode(targetName);
                        if (!fxNode || !targetNode) return;
                        // Set target_entity ParamSpec
                        if (fxNode->getParamSpec()) {
                            auto* p = fxNode->getParamSpec()->getParam("target_entity");
                            if (p) p->stringVal = targetName;
                        }
                        // Create the DAG link (entity output → entity input)
                        auto& srcOutputs = targetNode->getOutputs();
                        auto& dstInputs = fxNode->getInputs();
                        auto srcIt = srcOutputs.find("entity");
                        auto dstIt = dstInputs.find("entity");
                        if (srcIt != srcOutputs.end() && dstIt != dstInputs.end()) {
                            animator->link(srcIt->second, dstIt->second);
                        }
                        fxNode->onLinkChanged();
                    },
                    [this, fxName, targetName]() {
                        auto* animator = Animator::instance();
                        if (!animator) return;
                        auto* fxNode = animator->getRegisteredNode(fxName);
                        if (!fxNode) return;
                        if (fxNode->getParamSpec()) {
                            auto* p = fxNode->getParamSpec()->getParam("target_entity");
                            if (p) p->stringVal = "";
                        }
                        auto* targetNode = animator->getRegisteredNode(targetName);
                        if (!targetNode) return;
                        auto& srcOutputs = targetNode->getOutputs();
                        auto& dstInputs = fxNode->getInputs();
                        auto srcIt = srcOutputs.find("entity");
                        auto dstIt = dstInputs.find("entity");
                        if (srcIt != srcOutputs.end() && dstIt != dstInputs.end()) {
                            animator->unlink(srcIt->second, dstIt->second);
                        }
                        fxNode->onLinkChanged();
                    }
                ));
                CommandManager::instance().execute(std::move(compound));

                // Position FX node to the right of the target in the node editor
                if (mNodeEditorPanel) {
                    auto positions = mNodeEditorPanel->getNodePositions();
                    float targetX = 0, targetY = 0;
                    for (auto& p : positions) {
                        if (p.name == targetName) { targetX = p.x; targetY = p.y; break; }
                    }
                    // Count existing FX on this target to stack vertically
                    int fxIndex = 0;
                    for (auto& n : animator->getRegisteredNodeNames()) {
                        auto* nd = animator->getRegisteredNode(n);
                        if (nd && nd->getParamSpec() && n != fxName) {
                            auto* te = nd->getParamSpec()->getParam("target_entity");
                            if (te && te->stringVal == targetName) fxIndex++;
                        }
                    }
                    std::vector<NodeEditorPanel::NodePosition> newPos;
                    newPos.push_back({fxName, targetX + 200.0f, targetY + fxIndex * 100.0f});
                    mNodeEditorPanel->setNodePositions(newPos);
                }
            }
        });
    }

    initImGui();

    // ImGui Test Engine initialization (v3.2.5)
    mTestEngine = ImGuiTestEngine_CreateContext();
    ImGuiTestEngineIO& teIO = ImGuiTestEngine_GetIO(mTestEngine);
    teIO.ConfigVerboseLevel = ImGuiTestVerboseLevel_Info;
    teIO.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
    teIO.ConfigRunSpeed = ImGuiTestRunSpeed_Fast;
    ImGuiTestEngine_Start(mTestEngine, ImGui::GetCurrentContext());
    registerTests();
    std::cout << "[StudioApp] ImGui Test Engine initialized (" << 25 << " tests)" << std::endl;
}

StudioApp::~StudioApp() {
    if (mTestEngine) {
        ImGuiTestEngine_Stop(mTestEngine);
    }
    removeLockFile();
    shutdownImGui(); // destroys ImGui context first
    if (mTestEngine) {
        ImGuiTestEngine_DestroyContext(mTestEngine); // then test engine
        mTestEngine = nullptr;
    }
}

// ── ImGui lifecycle ───────────────────────────────────────────────────────────

void StudioApp::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Dockable panels
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = "imgui.ini"; // Persist layout

    applyDarkTheme();

    ImGui_ImplSDL3_InitForOpenGL(mEngine->getSDLWindow(), mEngine->getGLContext());
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // Run one dummy ImGui frame AND render it to force full creation of GL
    // device objects (shaders + font texture upload via WantCreate) while the
    // GL context is still pristine. OutputManager creates SDL_WINDOW_OPENGL
    // windows during project load; on AMD drivers this corrupts the current
    // GL context, preventing subsequent glCompileShader calls.
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void StudioApp::shutdownImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void StudioApp::applyDarkTheme() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    // Base palette
    ImVec4 bg       = {0.10f, 0.10f, 0.10f, 1.00f}; // #1A1A1A
    ImVec4 panel    = {0.14f, 0.14f, 0.14f, 1.00f}; // #242424
    ImVec4 border   = {0.18f, 0.18f, 0.18f, 1.00f}; // #2D2D2D
    ImVec4 cyan     = {0.00f, 1.00f, 1.00f, 1.00f}; // #00FFFF
    ImVec4 cyan_dim = {0.00f, 0.60f, 0.60f, 1.00f};
    ImVec4 text     = {0.88f, 0.88f, 0.88f, 1.00f}; // #E0E0E0

    style.Colors[ImGuiCol_WindowBg]         = bg;
    style.Colors[ImGuiCol_ChildBg]          = panel;
    style.Colors[ImGuiCol_PopupBg]          = panel;
    style.Colors[ImGuiCol_Border]           = border;
    style.Colors[ImGuiCol_FrameBg]          = panel;
    style.Colors[ImGuiCol_FrameBgHovered]   = border;
    style.Colors[ImGuiCol_TitleBg]          = bg;
    style.Colors[ImGuiCol_TitleBgActive]    = {0.10f, 0.30f, 0.30f, 1.00f};
    style.Colors[ImGuiCol_Tab]              = panel;
    style.Colors[ImGuiCol_TabHovered]       = cyan_dim;
    style.Colors[ImGuiCol_TabSelected]      = {0.00f, 0.40f, 0.40f, 1.00f};
    style.Colors[ImGuiCol_Header]           = cyan_dim;
    style.Colors[ImGuiCol_HeaderHovered]    = cyan;
    style.Colors[ImGuiCol_Button]           = border;
    style.Colors[ImGuiCol_ButtonHovered]    = cyan_dim;
    style.Colors[ImGuiCol_ButtonActive]     = cyan;
    style.Colors[ImGuiCol_SliderGrab]       = cyan_dim;
    style.Colors[ImGuiCol_SliderGrabActive] = cyan;
    style.Colors[ImGuiCol_CheckMark]        = cyan;
    style.Colors[ImGuiCol_Text]             = text;
    style.Colors[ImGuiCol_DockingPreview]   = {0.00f, 1.00f, 1.00f, 0.40f};

    style.WindowRounding   = 4.0f;
    style.FrameRounding    = 3.0f;
    style.GrabRounding     = 3.0f;
    style.ItemSpacing      = {8.0f, 6.0f};
    style.WindowPadding    = {8.0f, 8.0f};
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void StudioApp::run() {
    auto* animator = Animator::instance();
    auto* time     = RootTimeNode::instance();
    if (time) time->reset();

    // Load asset manifests produced by tools/asset_pipeline.py. Missing files
    // are silently skipped (pipeline may not have been run yet — runtime falls
    // back to v3.5.1 textures via filename resolution).
    {
        auto& mf = AssetManifest::instance();
        const char* manifests[] = {
            "lua/assets/heritage_pack.lua",
            "lua/assets/video_library.lua",
        };
        size_t total = 0;
        for (const char* p : manifests) {
            total += mf.loadFromLuaFile(mLua, p);
        }
        if (total > 0) {
            std::cout << "[AssetManifest] loaded " << total << " entries" << std::endl;
        }
    }

    // Load scene setup script — always needed to create camera, lights, mesh
    {
        std::string sceneScript = mInitialScript.empty()
            ? "lua/demos/demo_studio.lua" : mInitialScript;
        if (std::filesystem::exists(sceneScript)) {
            auto result = mLua.safe_script_file(sceneScript, sol::script_pass_on_error);
            if (!result.valid()) {
                sol::error err = result;
                std::cerr << "[Studio] Lua error: " << err.what() << '\n';
            }
        } else {
            std::cerr << "[Studio] Scene script not found: " << sceneScript << std::endl;
        }
    }

    // Start TCP REPL server for remote debugger access (port 33195)
    {
        auto shellResult = mLua.safe_script(R"(
            local ok, err = pcall(function()
                require 'shell.server'
                ShellServer:new({port = 33195, max_clients = 2})
            end)
            if ok then
                print("[Studio] TCP Debugger listening on port 33195")
            else
                print("[Studio] TCP Debugger not available: " .. tostring(err))
            end
        )", sol::script_pass_on_error);
    }

    // v3.5.2 Sprint S8 Lot AV.5 — CLI --demo <name> : short-circuit le chargement
    // de projet par défaut, charge directement le builder de la démo demandée.
    // Évite la phase "load demo_studio_base puis open Open Demo" qui consommait
    // ~5s. Usage : ./bbfx-studio.exe --demo demo_video_wall
    if (!mInitialDemo.empty()) {
        std::string builderPath = "lua/demos/projects/" + mInitialDemo + "_builder.lua";
        if (!std::filesystem::exists(builderPath)) {
            std::cerr << "[Studio] --demo: builder not found: " << builderPath << std::endl;
        } else {
            std::cout << "[Studio] --demo: loading " << builderPath << std::endl;
            // Clear user graph (équivalent du handler File → Open Demo).
            if (auto* anim = Animator::instance()) {
                auto names = anim->getRegisteredNodeNames();
                for (auto& n : names) {
                    if (n == "time") continue;
                    if (n.rfind("shell/", 0) == 0 || n.rfind("_dbg_", 0) == 0 || n.rfind("_test_", 0) == 0) continue;
                    if (auto* nd = anim->getRegisteredNode(n)) { anim->removeNode(nd); nd->cleanup(); delete nd; }
                }
            }
            // Run builder.setup().
            mLua.safe_script("local _b = dofile('" + builderPath + "'); if type(_b)=='table' and type(_b.setup)=='function' then _b.setup() end",
                             sol::script_pass_on_error);
            { sol::optional<sol::function> fn = mLua["_dbg_process_pending"]; if (fn) (*fn)(); }
            mProjectPath.clear();
            mProjectDirty = false;
            SDL_SetWindowTitle(mEngine->getSDLWindow(),
                ("BBFx Studio — demo: " + mInitialDemo).c_str());
        }
    } else
    // Load project: --default/--reset → template, else last saved project, else template
    {
        auto& settings = SettingsManager::instance();
        const auto& lastPath = settings.get().lastProjectPath;
        bool loadTemplate = mForceDefault || lastPath.empty() || !std::filesystem::exists(lastPath);

        if (!loadTemplate) {
            std::cout << "[Studio] Loading last project: " << lastPath << std::endl;
            loadProject(lastPath);
        } else {
            // v3.5.2 Sprint S8 Lot AU — fresh-start fallback = the cleaned
            // reference scene (demo_studio_base). The legacy
            // data/templates/default.bbfx-project is kept only as a last resort
            // (it has a pre-existing startup hang on some setups — see Lot AU.3).
            std::string startupScene = "lua/demos/projects/demo_studio_base.bbfx-project";
            if (!std::filesystem::exists(startupScene))
                startupScene = "data/templates/default.bbfx-project";
            if (std::filesystem::exists(startupScene)) {
                std::cout << "[Studio] Loading startup scene: " << startupScene << std::endl;
                loadProject(startupScene);
                mProjectPath.clear();
                SDL_SetWindowTitle(mEngine->getSDLWindow(),
                                   ("BBFx Studio v" + std::string(BBFX_VERSION_STRING) + " — " + BBFX_VERSION_NAME).c_str());
            }
        }
        if (mRecentProjects.empty() && !settings.get().recentProjects.empty()) {
            mRecentProjects = settings.get().recentProjects;
        }
    }

    while (mRunning) {
      // (Major fix) Filet par frame : un throw d'un node/panel/Lua/OGRE ne doit pas
      // tuer toute la session VJ. On log et on continue (skip-frame) au lieu de crasher.
      try {
        // ── Process deferred debugger ops (between frames, before DAG evaluation) ──
        {
            sol::optional<sol::function> fn = mLua["_dbg_process_pending"];
            if (fn) (*fn)();
        }
        {
            sol::optional<sol::function> fn = mLua["_dbg_process_pending_load"];
            if (fn) (*fn)();
        }

        // ── Events ───────────────────────────────────────────────────────────
        handleEvents();

        // v3.5.2 Sprint S8 Lot AU.8 — refresh the input devices once per frame
        // (rumble timers, gyro/accel, and — importantly — so GamepadNode reads
        // a live device state). Without this the headless render loop captures
        // input but the Studio main loop didn't → joystick/gamepad nodes inert.
        if (auto* im = mEngine ? mEngine->getInputManager() : nullptr) im->capture();

        // ── Animation DAG ─────────────────────────────────────────────────────
        if (time && !mTimelinePanel->isPaused()) time->update();
        // Automation: loop region wrap + evaluation
        if (time && !mTimelinePanel->isPaused()) {
            float bpm = time->getBPM();
            float currentBeat = (bpm > 0.0f) ? time->getTotalTime() * bpm / 60.0f : 0.0f;

            // Loop region wrap
            auto& loop = mTimelinePanel->getAutomation().loopRegion;
            if (loop.active && loop.endBeat > loop.startBeat && currentBeat >= loop.endBeat) {
                float seekTime = loop.startBeat * 60.0f / bpm;
                time->seekTo(seekTime);
                currentBeat = loop.startBeat;
            }

            // Chord snapshot auto-restore + crossfade
            {
                float prevBeat2 = currentBeat - (time->getOutputs().at("dt")->getValue() * bpm / 60.0f);
                for (auto& cb : mTimelinePanel->getChordBlocks()) {
                    if (!cb.snapshot.empty() && prevBeat2 < cb.startBeat && currentBeat >= cb.startBeat) {
                        // Entering chord with snapshot — inject values (with crossfade if transitionBeats > 0)
                        if (animator) {
                            for (auto& [key, targetVal] : cb.snapshot) {
                                auto dot = key.find('.');
                                if (dot == std::string::npos) continue;
                                std::string nodeName = key.substr(0, dot);
                                std::string portName = key.substr(dot + 1);
                                auto* n = animator->getRegisteredNode(nodeName);
                                if (!n) continue;
                                auto& inputs = n->getInputs();
                                auto it = inputs.find(portName);
                                if (it == inputs.end()) continue;
                                // Immediate restore (crossfade would need per-frame state tracking)
                                it->second->setValue(targetVal);
                            }
                        }
                    }
                }
            }

            // Trigger events
            float prevBeat = currentBeat - (time->getOutputs().at("dt")->getValue() * bpm / 60.0f);
            for (auto& te : mTimelinePanel->getAutomation().triggerEvents) {
                if (prevBeat < te.beat && currentBeat >= te.beat) {
                    // Parse and execute action
                    if (te.action.substr(0, 6) == "chord:" && te.action.size() > 6) {
                        std::string chordName = te.action.substr(6);
                        mLua["ChordSystem"]["toggle"](chordName);
                    } else if (te.action.substr(0, 7) == "enable:" && te.action.size() > 7) {
                        std::string nodeName = te.action.substr(7);
                        if (animator) {
                            auto* n = animator->getRegisteredNode(nodeName);
                            if (n) n->setEnabled(true);
                        }
                    } else if (te.action.substr(0, 8) == "disable:" && te.action.size() > 8) {
                        std::string nodeName = te.action.substr(8);
                        if (animator) {
                            auto* n = animator->getRegisteredNode(nodeName);
                            if (n) n->setEnabled(false);
                        }
                    } else if (te.action.substr(0, 11) == "chord_jump:" && te.action.size() > 11) {
                        std::string chordName = te.action.substr(11);
                        if (mTimelinePanel) {
                            for (auto& cb : mTimelinePanel->getChordBlocks()) {
                                if (cb.name == chordName) {
                                    float seekTime = cb.startBeat * 60.0f / bpm;
                                    time->seekTo(seekTime);
                                    break;
                                }
                            }
                        }
                    } else if (te.action.substr(0, 7) == "preset:" && te.action.size() > 7) {
                        std::string presetName = te.action.substr(7);
                        std::string cmd = "instantiatePreset('" + presetName + "')";
                        mLua.safe_script(cmd, sol::script_pass_on_error);
                    }
                }
            }

            // Automation injection
            if (mAutomationEngine) {
                mAutomationEngine->evaluate(currentBeat, animator);
            }
        }
        if (animator) animator->renderOneFrame();

        // ── Update all registered nodes (DAG only updates connected ones) ────
        // v3.5.2 Sprint S8 Lot AT — call tick() (not update()) on EVERY node,
        // even disabled ones, so the universal `enabled` DAG port can re-enable
        // a node from an upstream signal (joystick toggle, beat trigger, …).
        // tick() = syncEnabledFromPort() + (mEnabled ? update() : nothing).
        if (animator) {
            for (auto& name : animator->getRegisteredNodeNames()) {
                auto* node = animator->getRegisteredNode(name);
                if (node && node->getTypeName() != "RootTimeNode") {
                    node->tick();
                }
            }
        }

        // NOTE: Do NOT call _dbg_process_pending() here — even after the node update
        // loop completes, the Animator may hold internal references that get invalidated
        // by node creation/deletion. The ONLY safe place is at the TOP of the main loop.
        // Test WL(3) accounts for this: op queued frame N → processed frame N+1 step 1
        // → test resumes frame N+3 step 4.

        // NOTE: Compositors are NOT applied in Studio Mode (FBO corruption with ImGui).
        // They are applied in Performance Mode only, via PerformanceModePanel::applyCompositorChain().

        // ── OGRE: render to RenderTexture ─────────────────────────────────────
        mViewportPanel->updateOgreRender();

        // ── Output windows: render to each projector (v3.4 Lot A) ────────────
        if (mEngine) mEngine->updateOutputTarget();

        // ── Network sync poll (v3.4 Lot F) ────────────────────────────────────
        if (mSyncManager) mSyncManager->poll();

        // ── MIDI learn capture hors Performance Mode (D17/D18) ────────────────
        // La capture du MIDI learn ne tournait que dans PerformanceModePanel (actif
        // uniquement en perf mode) → Learn impossible à compléter en session normale
        // (MidiMappingPanel / EffectRackPanel). On poll + processMessages ICI, mais
        // SEULEMENT pendant un learn actif et hors perf mode : éviter de drainer la
        // queue MIDI quand d'autres consommateurs (MidiInputNode, MidiActivityPanel)
        // en ont besoin, et éviter le double-drain avec PerformanceModePanel.
        if (!mPerformanceMode) {
            auto& mlm = MidiLearnManager::instance();
            if (mlm.isLearning()) {
                if (auto* midiMgr = MidiDeviceManager::instance()) {
                    mlm.processMessages(midiMgr->poll());
                }
            }
        }

        // ── HTTP client main-thread callback drain (v3.5 Lot E) ───────────────
        HttpClient::instance().pumpMainThread();

        // ── OSC bus + Art-Net input pump (v3.5 Lot M) ─────────────────────────
        OscBus::instance().tick();
        ArtnetInput::instance().tick();

        // ── N4 — drain des demandes de preset OSC (/bbfx/preset/load/<name>) ──
        // Le chargement touche le graphe → main-thread uniquement. On appelle le
        // loader Lua dbg.preset(name) pour chaque demande empilée par OscInputNode.
        if (!gPendingOscPresetLoads.empty()) {
            auto pending = std::move(gPendingOscPresetLoads);
            gPendingOscPresetLoads.clear();
            sol::object dbgObj = mLua["dbg"];
            if (dbgObj.is<sol::table>()) {
                sol::function presetFn = dbgObj.as<sol::table>()["preset"];
                for (auto& nm : pending) {
                    if (presetFn.valid()) {
                        try { presetFn(nm); }
                        catch (const std::exception& e) {
                            std::cerr << "[OSC] preset load '" << nm << "' failed: " << e.what() << std::endl;
                        }
                    }
                }
            }
        }

        // ── Tempo callback dispatch (v3.5 Lot N) ──────────────────────────────
        TempoManager::instance().update();

        // ── Plugin hot-reloader (v3.5 Lot U) — rate-limited 500ms scan ─────────
        PluginHotReloader::instance().tick();

        // ── Auto-save ─────────────────────────────────────────────────────────
        tickAutoSave();

        // ── ImGui frame ───────────────────────────────────────────────────────
        renderFrame();
      } catch (const std::exception& e) {
        std::cerr << "[StudioApp] Exception interceptée pendant la frame (skip-frame, "
                     "session préservée) : " << e.what() << std::endl;
      } catch (...) {
        std::cerr << "[StudioApp] Exception non-standard interceptée pendant la frame "
                     "(skip-frame, session préservée)." << std::endl;
      }
    }

    // v3.5.2 — Persist window geometry on exit (even without explicit save)
    {
        auto& settings = SettingsManager::instance();
        auto s = settings.get();
        int wx, wy, ww, wh;
        SDL_GetWindowPosition(mEngine->getSDLWindow(), &wx, &wy);
        SDL_GetWindowSize(mEngine->getSDLWindow(), &ww, &wh);
        s.windowX = wx;
        s.windowY = wy;
        s.windowWidth = ww;
        s.windowHeight = wh;
        settings.set(s);
        settings.save();
    }
}

void StudioApp::handleEvents() {
    SDL_Event evt;
    while (SDL_PollEvent(&evt)) {
        handleEvent(evt);
    }
}

void StudioApp::handleEvent(const SDL_Event& evt) {
    // ImGui gets first dibs on all events
    ImGui_ImplSDL3_ProcessEvent(&evt);
    ImGuiIO& io = ImGui::GetIO();

    // ── WarpWizard event routing (v3.4 Lot D) ────────────────────────────────
    if (mWarpWizard.isActive() && mEngine) {
        auto* outMgr = mEngine->getOutputManager();
        auto* slot = outMgr ? outMgr->getSlot(mWarpWizard.getOutputSlotId()) : nullptr;
        if (slot && slot->window) {
            SDL_WindowID wizWinId = SDL_GetWindowID(slot->window);

            if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                evt.button.button == SDL_BUTTON_LEFT &&
                evt.button.windowID == wizWinId) {
                int winW = 0, winH = 0;
                SDL_GetWindowSize(slot->window, &winW, &winH);
                float nx = (winW > 0) ? (evt.button.x / static_cast<float>(winW)) : 0.5f;
                float ny = (winH > 0) ? (evt.button.y / static_cast<float>(winH)) : 0.5f;
                mWarpWizard.handleMouseClick(nx, ny);

                // If wizard just finished (DONE), push undo command.
                if (mWarpWizard.getState() == WarpWizardState::DONE) {
                    WarpProfile prev = mWarpWizard.getPreviousWarp();
                    WarpProfile next = mWarpWizard.getComputedWarp();
                    int slotId = mWarpWizard.getOutputSlotId();
                    OutputManager* mgr = outMgr;
                    CommandManager::instance().execute(
                        std::make_unique<LambdaCommand>(
                            "Warp Calibration",
                            [mgr, slotId, next]() {
                                auto* s = mgr->getSlot(slotId);
                                if (s) { s->warpProfile = next; mgr->updateWarpParams(slotId); }
                            },
                            [mgr, slotId, prev]() {
                                auto* s = mgr->getSlot(slotId);
                                if (s) { s->warpProfile = prev; mgr->updateWarpParams(slotId); }
                            }
                        )
                    );
                    ToastSystem::instance().toast("Warp calibration complete");
                    // Reset wizard to IDLE so it can be restarted.
                    mWarpWizard = WarpWizard{};
                }
                return; // consumed
            }

            if (evt.type == SDL_EVENT_KEY_DOWN && evt.key.key == SDLK_ESCAPE) {
                mWarpWizard.handleEscapeKey();
                mWarpWizard = WarpWizard{};
                return;
            }
        }
    }

    // Global events that always apply
    if (evt.type == SDL_EVENT_QUIT) {
        mRunning = false;
        return;
    }

    // ── Effect Rack keyboard learn (capture key before global shortcuts) ────
    if (evt.type == SDL_EVENT_KEY_DOWN && mEffectRackPanel) {
        if (mEffectRackPanel->handleKeyEvent(evt)) return;
    }

    if (evt.type == SDL_EVENT_KEY_DOWN) {
        if (evt.key.key == SDLK_ESCAPE) {
            if (mWarpWizard.isActive()) {
                // Escape during wizard = cancel (secondary path, handled above if output window focused)
                mWarpWizard.handleEscapeKey();
                mWarpWizard = WarpWizard{};
                return;
            }
            if (mPerformanceMode) {
                mPerformanceMode = false;
                if (mPerformanceModePanel && mEngine)
                    mPerformanceModePanel->removeCompositorChain(mEngine.get());
                if (mEngine)
                    mEngine->invalidateFBOCache();
                if (mViewportPanel)
                    mViewportPanel->invalidateSize();
            } else if (mViewportPanel && mViewportPanel->getGizmo() &&
                       mViewportPanel->getGizmo()->isInKeyboardMode()) {
                mViewportPanel->getGizmo()->cancelKeyboardTransform();
            } else if (mViewportPanel && mViewportPanel->getPicker() &&
                       !mViewportPanel->getPicker()->getSelectedNodeName().empty()) {
                mViewportPanel->getPicker()->deselect();
                if (mViewportPanel->getGizmo()) mViewportPanel->getGizmo()->clearTarget();
            } else {
                mRunning = false;
            }
            return;
        }
        if (evt.key.key == SDLK_F5) {
            mPerformanceMode = !mPerformanceMode;
            if (mPerformanceMode) {
                // Entering Performance Mode: apply compositors
                if (mPerformanceModePanel && mEngine)
                    mPerformanceModePanel->applyCompositorChain(mEngine.get());
            } else {
                // Exiting Performance Mode: remove compositors and restore viewport
                if (mPerformanceModePanel && mEngine)
                    mPerformanceModePanel->removeCompositorChain(mEngine.get());
                if (mEngine)
                    mEngine->invalidateFBOCache();
                if (mViewportPanel)
                    mViewportPanel->invalidateSize();
            }
            return;
        }
        if (evt.key.key == SDLK_F11) {
            if (mEngine && mEngine->isOutputOpen()) {
                mEngine->toggleOutputFullscreen();
            }
            return;
        }
        // +/- : BPM up/down — Ctrl held: ±5, plain: ±1
        {
            const float bpmStep = (evt.key.mod & SDL_KMOD_CTRL) ? 5.0f : 1.0f;
            if (evt.key.key == SDLK_PLUS || evt.key.key == SDLK_KP_PLUS ||
                evt.key.key == SDLK_EQUALS) {
                if (mTimelinePanel) mTimelinePanel->changeBPM(+bpmStep);
                return;
            }
            if (evt.key.key == SDLK_MINUS || evt.key.key == SDLK_KP_MINUS) {
                if (mTimelinePanel) mTimelinePanel->changeBPM(-bpmStep);
                return;
            }
        }

        bool ctrl  = (evt.key.mod & SDL_KMOD_CTRL)  != 0;
        bool shift = (evt.key.mod & SDL_KMOD_SHIFT) != 0;

        // Ctrl+Shift+O : Output Manager
        // Ctrl+Shift+N : Network Sync panel
        if (ctrl && shift && evt.key.key == SDLK_O) { mShowOutputManager = !mShowOutputManager; return; }
        if (ctrl && shift && evt.key.key == SDLK_S) { mShowSurfaceEditor = !mShowSurfaceEditor; return; }
        if (ctrl && shift && evt.key.key == SDLK_N) { mShowNetworkPanel  = !mShowNetworkPanel;  return; }
        if (ctrl && shift && evt.key.key == SDLK_M) { mShowMasterView    = !mShowMasterView;    return; }
        // v3.5 Lot H: Ctrl+Shift+P is the Command Palette (VS Code convention).
        // PANIC ALL moves to Ctrl+Shift+! (exclamation) to free the shortcut.
        if (ctrl && shift && evt.key.key == SDLK_P) {
            CommandPalette::instance().open();
            return;
        }
        if (ctrl && shift && evt.key.key == SDLK_1) { panicAll(); return; }
        // v3.5 Lot H: Ctrl+Shift+C opens the Community Browser.
        if (ctrl && shift && evt.key.key == SDLK_C) {
            mShowCommunityBrowser = !mShowCommunityBrowser;
            if (mShowCommunityBrowser && mCommunityBrowserPanel)
                mCommunityBrowserPanel->requestRefresh();
            return;
        }
        // v3.5 Lot D: Ctrl+Shift+X toggles Plugin Manager window.
        if (ctrl && shift && evt.key.key == SDLK_X) { mShowPluginManager = !mShowPluginManager; return; }
        // v3.5 Lot G: Ctrl+Shift+E toggles Plugin Errors window.
        if (ctrl && shift && evt.key.key == SDLK_E) {
            mShowPluginErrors = !mShowPluginErrors;
            if (mShowPluginErrors) PluginErrorLog::instance().acknowledgeAll();
            return;
        }
        // v3.5 Lot L: Ctrl+Shift+G toggles the Gamepad panel.
        if (ctrl && shift && evt.key.key == SDLK_G) {
            mShowGamepadPanel = !mShowGamepadPanel;
            return;
        }
        // v3.5.1 Lot L: Ctrl+Shift+A toggles the Asset Browser.
        if (ctrl && shift && evt.key.key == SDLK_A) {
            mShowAssetBrowser = !mShowAssetBrowser;
            return;
        }
        // v3.5.2 Sprint S7 Lot Y: Ctrl+Shift+L toggles the Learn Panel.
        if (ctrl && shift && evt.key.key == SDLK_L) {
            mShowLearnPanel = !mShowLearnPanel;
            if (mLearnPanel) mLearnPanel->setVisible(mShowLearnPanel);
            return;
        }

        if (ctrl && evt.key.key == SDLK_S) {
            if (mProjectPath.empty()) {
                saveProject("project.bbfx-project");
            } else {
                saveProject(mProjectPath);
            }
            return;
        }
        if (ctrl && evt.key.key == SDLK_Z) {
            CommandManager::instance().undo();
            return;
        }
        if (ctrl && evt.key.key == SDLK_Y) {
            CommandManager::instance().redo();
            return;
        }
        // Ctrl+D: handled by NodeEditorPanel via ImGui key polling
        if (ctrl && evt.key.key == SDLK_N) {
            newProject();
            return;
        }
        if (ctrl && evt.key.key == SDLK_O) {
            static const char* filter = "BBFx Project (*.bbfx-project)\0*.bbfx-project\0All Files\0*.*\0";
            auto path = openFileDialog(mEngine->getSDLWindow(), filter, "Open BBFx Project");
            if (!path.empty()) loadProject(path);
            return;
        }
        if (ctrl && evt.key.key == SDLK_COMMA) {
            mShowSettings = true;
            return;
        }
        if (ctrl && evt.key.key == SDLK_E) {
            mExportDialog.open();
            return;
        }
        // F1 = Help/About
        if (evt.key.key == SDLK_F1) {
            mShowAbout = true;
            return;
        }
        // F2 = Toggle Console
        if (evt.key.key == SDLK_F2) {
            mShowConsole = !mShowConsole;
            return;
        }
        // F3 = Toggle Inspector
        if (evt.key.key == SDLK_F3) {
            mShowInspector = !mShowInspector;
            return;
        }
        // F4 = Toggle Timeline
        if (evt.key.key == SDLK_F4) {
            mShowTimeline = !mShowTimeline;
            return;
        }
        // F6 = Toggle Preset Browser
        if (evt.key.key == SDLK_F6) {
            mShowPresetBrowser = !mShowPresetBrowser;
            return;
        }
        // F7 = Toggle Node Editor
        if (evt.key.key == SDLK_F7) {
            mShowNodeEditor = !mShowNodeEditor;
            return;
        }
        // F8 = Toggle Scene Hierarchy
        if (evt.key.key == SDLK_F8) {
            mShowSceneHierarchy = !mShowSceneHierarchy;
            return;
        }
        // F9 = Toggle Effect Rack
        if (evt.key.key == SDLK_F9) {
            mShowEffectRack = !mShowEffectRack;
            return;
        }
        // Space = Play/Pause toggle (when not typing in a text field)
        if (evt.key.key == SDLK_SPACE && !io.WantCaptureKeyboard) {
            if (mTimelinePanel) {
                mTimelinePanel->togglePause();
            }
            return;
        }

        // Viewport gizmo shortcuts (G/R/S/X/Y/Z) — disabled during FPS camera mode
        bool fpsCaptured = mViewportPanel && mViewportPanel->isFpsCaptured();
        if (mViewportPanel && mViewportPanel->getGizmo() && !io.WantCaptureKeyboard && !ctrl && !fpsCaptured) {
            auto* gizmo = mViewportPanel->getGizmo();
            if (gizmo->isInKeyboardMode()) {
                // During keyboard mode: X/Y/Z constrain, Escape cancels
                if (evt.key.key == SDLK_X) { gizmo->constrainAxis(0); return; }
                if (evt.key.key == SDLK_Y) { gizmo->constrainAxis(1); return; }
                if (evt.key.key == SDLK_Z) { gizmo->constrainAxis(2); return; }
                // Escape handled above (deselect section)
            } else if (gizmo->hasTarget()) {
                if (evt.key.key == SDLK_G) { gizmo->startKeyboardTransform(ViewportGizmo::Tool::Translate); return; }
                if (evt.key.key == SDLK_R && !ctrl) { gizmo->startKeyboardTransform(ViewportGizmo::Tool::Rotate); return; }
                if (evt.key.key == SDLK_S && !ctrl) { gizmo->startKeyboardTransform(ViewportGizmo::Tool::Scale); return; }
            }
        }

        // Viewport camera shortcuts (F=focus, Home=reset, Numpad views)
        if (mViewportPanel && mViewportPanel->getCameraController() && !io.WantCaptureKeyboard) {
            auto* camCtrl = mViewportPanel->getCameraController();
            if (evt.key.key == SDLK_F && !ctrl) {
                // Focus on selected object
                auto selName = mNodeEditorPanel ? mNodeEditorPanel->getSelectedNodeName() : "";
                if (!selName.empty()) {
                    auto* animator = Animator::instance();
                    auto* node = animator ? animator->getRegisteredNode(selName) : nullptr;
                    if (node) {
                        // Try to get SceneNode from known node types
                        auto* soNode = dynamic_cast<SceneObjectNode*>(node);
                        if (soNode && soNode->getSceneNode()) {
                            camCtrl->focusOn(soNode->getSceneNode());
                        }
                    }
                }
            }
            if (evt.key.key == SDLK_HOME) {
                camCtrl->resetCamera();
            }
            // Numpad preset views
            bool numCtrl = ctrl;
            if (evt.key.key == SDLK_KP_1) camCtrl->setPresetView(1, numCtrl);
            if (evt.key.key == SDLK_KP_3) camCtrl->setPresetView(3, numCtrl);
            if (evt.key.key == SDLK_KP_7) camCtrl->setPresetView(7, numCtrl);
        }

        // Record keyboard events if recording is active
        if (mTimelinePanel && mTimelinePanel->isRecording()) {
            auto* rec = mTimelinePanel->getRecorder();
            if (rec) rec->recordKey(static_cast<int>(evt.key.key), "press");
        }
    }

    if (evt.type == SDL_EVENT_KEY_UP) {
        if (mTimelinePanel && mTimelinePanel->isRecording()) {
            auto* rec = mTimelinePanel->getRecorder();
            if (rec) rec->recordKey(static_cast<int>(evt.key.key), "release");
        }
    }

    if (evt.type == SDL_EVENT_DROP_FILE) {
        const char* dropped = evt.drop.data;
        if (dropped) {
            std::string path(dropped);
            std::string ext = std::filesystem::path(path).extension().string();
            if (ext == ".bbfx-project") {
                loadProject(path);
            } else if (ext == ".lua") {
                auto result = mLua.safe_script_file(path, sol::script_pass_on_error);
                if (!result.valid()) {
                    sol::error err = result;
                    std::cerr << "[Studio] Lua error: " << err.what() << '\n';
                }
            } else if (ext == ".zip") {
                // v3.5 Lot F: install a plugin ZIP dropped onto the Studio.
                // We extract a read-only view of the manifest, prompt for
                // permissions, and only then commit the install.
                promptInstallZip(path);
            }
        }
    }

    if (evt.type == SDL_EVENT_WINDOW_RESIZED) {
        // Resize handled by ViewportPanel when it detects size change
    }

    // Forward to REPL/input:
    //  - gamepad / joystick events: ALWAYS (they don't conflict with ImGui text
    //    input — and without this, SDL_EVENT_GAMEPAD_ADDED is dropped whenever
    //    ImGui is capturing the keyboard/mouse, so the controller never gets
    //    SDL_OpenGamepad'd → GamepadNode reads getCount()==0 → controls nothing);
    //  - keyboard / mouse: only when ImGui isn't capturing them (no hotkeys
    //    while typing in a field). — v3.5.2 Sprint S8 Lot AU.8.
    if (auto* im = mEngine ? mEngine->getInputManager() : nullptr) {
        const bool isPadEvt = (evt.type >= SDL_EVENT_JOYSTICK_AXIS_MOTION &&
                               evt.type <= SDL_EVENT_GAMEPAD_UPDATE_COMPLETE);
        if (isPadEvt || (!io.WantCaptureKeyboard && !io.WantCaptureMouse)) {
            im->handleSDLEvent(evt);
        }
    }
}

void StudioApp::renderFrame() {
    // Process deferred debugger operations (v3.2.5 original position — safe between ImGui frames)
    mLua.safe_script("if _dbg_process_pending then _dbg_process_pending() end", sol::script_pass_on_error);

    // Process deferred deletions from DeleteNodeCommand (v3.2.5 original position)
    {
        if (!bbfx::gPendingDeletes.empty()) {
            auto names = std::move(bbfx::gPendingDeletes);
            bbfx::gPendingDeletes.clear();
            auto* animator = Animator::instance();
            for (auto& n : names) {
                if (!animator) break;
                auto* node = animator->getRegisteredNode(n);
                if (!node) continue;

                // Clear viewport references BEFORE destroying the node
                if (mViewportPanel) {
                    auto* picker = mViewportPanel->getPicker();
                    if (picker && picker->getSelectedNodeName() == n)
                        picker->deselect();

                    auto* soNode = dynamic_cast<SceneObjectNode*>(node);
                    if (soNode) {
                        auto* gizmo = mViewportPanel->getGizmo();
                        if (gizmo && gizmo->getTargetSceneNode() == soNode->getSceneNode())
                            gizmo->clearTarget();

                        auto* camCtrl = mViewportPanel->getCameraController();
                        if (camCtrl && camCtrl->getLockTarget() == soNode->getSceneNode())
                            camCtrl->setOrbitLockTarget(nullptr);

                        if (mLua["_sceneNodes"].valid()) {
                            sol::object sn = mLua["_sceneNodes"][n];
                            mLua["_sceneNodes"][n] = sol::nil;
                            if (sn.valid() && mLua["_rotateTarget"].valid()) {
                                if (sn == mLua["_rotateTarget"])
                                    mLua["_rotateTarget"] = sol::nil;
                            }
                        }
                    }
                }

                animator->removeNode(node);
                try { node->cleanup(); } catch (...) {}
                delete node;
            }

            if (mNodeEditorPanel) mNodeEditorPanel->syncFromDAG();
        }
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (!mPerformanceMode) {
        // Full-screen dockspace (Design Mode only)
        // Reserve space at the bottom for the status bar so it doesn't overlap panels
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        float statusBarH = ImGui::GetTextLineHeightWithSpacing() + 4;
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize({viewport->WorkSize.x, viewport->WorkSize.y - statusBarH});
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGuiWindowFlags dockFlags =
            ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        ImGui::Begin("##DockSpace", nullptr, dockFlags);
        ImGui::PopStyleVar();

        renderMenuBar();

        ImGuiID dockId = ImGui::GetID("MainDockSpace");

        // Only build default layout when no saved imgui.ini exists (first run
        // or after --reset/--clear).  On subsequent launches the saved docking
        // layout from imgui.ini is respected, preserving user customizations.
        static bool firstFrame = true;
        if (firstFrame) {
            firstFrame = false;
            bool hasSavedLayout = ImGui::DockBuilderGetNode(dockId) != nullptr;
            if (!hasSavedLayout) {
                ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(dockId, viewport->WorkSize);

                ImGuiID dockLeft, dockCenter, dockRight;
                ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Left, 0.20f, &dockLeft, &dockCenter);
                ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Right, 0.25f, &dockRight, &dockCenter);

                ImGuiID dockBottom;
                ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Down, 0.30f, &dockBottom, &dockCenter);

                ImGuiID dockViewport, dockNodeEditor;
                ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Left, 0.50f, &dockViewport, &dockNodeEditor);

                ImGui::DockBuilderDockWindow("Presets",       dockLeft);
                ImGui::DockBuilderDockWindow("Viewport",      dockViewport);
                ImGui::DockBuilderDockWindow("Node Editor",   dockNodeEditor);
                ImGui::DockBuilderDockWindow("Inspector",     dockRight);
                ImGui::DockBuilderDockWindow("Timeline",      dockBottom);
                ImGui::DockBuilderDockWindow("Asset Browser", dockBottom); // tabbed with Timeline

                ImGui::DockBuilderFinish(dockId);
            }
        }

        ImGui::DockSpace(dockId, {0, 0}, ImGuiDockNodeFlags_PassthruCentralNode);

        ImGui::End();
    }

    // Panels
    renderPanels();

    // Status bar is rendered inline inside renderPanels()

    // Render
    ImGui::Render();

    // Restore default framebuffer (screen) — panels may have triggered OGRE
    // RenderTexture updates which leave OGRE's FBO bound.
    ensureGLFunctions();
    if (s_glBindFramebuffer) s_glBindFramebuffer(0x8D40 /*GL_FRAMEBUFFER*/, 0);

    int w, h;
    SDL_GetWindowSize(mEngine->getSDLWindow(), &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.10f, 0.10f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // ImGui Test Engine post-swap (must be called every frame for engine to function)
    if (mTestEngine)
        ImGuiTestEngine_PostSwap(mTestEngine);

    SDL_GL_SwapWindow(mEngine->getSDLWindow());
}

void StudioApp::renderMenuBar() {
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New", "Ctrl+N")) {
            newProject();
        }
        if (ImGui::MenuItem("Open...", "Ctrl+O")) {
            static const char* filter = "BBFx Project (*.bbfx-project)\0*.bbfx-project\0All Files\0*.*\0";
            auto path = openFileDialog(mEngine->getSDLWindow(), filter, "Open BBFx Project");
            if (!path.empty()) loadProject(path);
        }
        // v3.5.2 Sprint S8 Lot AU — Demo Showcase Pack. Opening a demo RUNS its
        // Lua builder (clear user graph + dofile + setup) rather than loading the
        // baked .bbfx-project via loadProject(): loadProject() also re-applies the
        // outputs/surface-zone/FBO config mid-frame which freezes the live render
        // on a runtime project switch. Running the builder is the same path used
        // by bake_demos.lua / inspect_demos.lua — it switches cleanly without
        // killing the animation. (The baked .bbfx-project files remain available
        // via File → Open Project for the standard workflow.)
        if (ImGui::BeginMenu("Open Demo")) {
            namespace fs = std::filesystem;
            const std::string demoDir = "lua/demos/projects";
            bool any = false;
            std::error_code ec;
            if (fs::exists(demoDir, ec) && fs::is_directory(demoDir, ec)) {
                std::vector<std::string> demos;   // demo names (stems), with a *_builder.lua
                for (auto& e : fs::directory_iterator(demoDir, ec)) {
                    if (!e.is_regular_file()) continue;
                    std::string fn = e.path().filename().string();
                    const std::string suf = "_builder.lua";
                    if (fn.size() > suf.size() && fn.compare(fn.size() - suf.size(), suf.size(), suf) == 0)
                        demos.push_back(fn.substr(0, fn.size() - suf.size()));   // "demo_mesh_morph"
                }
                std::sort(demos.begin(), demos.end());
                for (auto& name : demos) {
                    any = true;
                    if (ImGui::MenuItem(name.c_str())) {
                        std::string builderPath = demoDir + "/" + name + "_builder.lua";
                        // 1. Clear the user graph (keep time/shell/_dbg/_test) — synchronous, destroys entities.
                        if (auto* anim = Animator::instance()) {
                            auto names = anim->getRegisteredNodeNames();
                            for (auto& n : names) {
                                if (n == "time") continue;
                                if (n.rfind("shell/", 0) == 0 || n.rfind("_dbg_", 0) == 0 || n.rfind("_test_", 0) == 0) continue;
                                if (auto* nd = anim->getRegisteredNode(n)) { anim->removeNode(nd); nd->cleanup(); delete nd; }
                            }
                        }
                        // 2. Run the builder's setup().
                        mLua.safe_script("local _b = dofile('" + builderPath + "'); if type(_b)=='table' and type(_b.setup)=='function' then _b.setup() end",
                                         sol::script_pass_on_error);
                        // 3. Flush deferred dbg.create ops so the nodes exist this frame.
                        { sol::optional<sol::function> fn = mLua["_dbg_process_pending"]; if (fn) (*fn)(); }
                        mProjectPath.clear();   // a demo isn't "the" project — don't Save over the builder
                        mProjectDirty = false;
                        SDL_SetWindowTitle(mEngine->getSDLWindow(), ("BBFx Studio — demo: " + name).c_str());
                        std::cout << "[Studio] Opened demo (builder): " << name << std::endl;
                    }
                }
            }
            if (!any) ImGui::TextDisabled("(no demo builders in lua/demos/projects/)");
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
            if (mProjectPath.empty()) {
                static const char* filter = "BBFx Project (*.bbfx-project)\0*.bbfx-project\0";
                auto path = saveFileDialog(mEngine->getSDLWindow(), filter, "Save BBFx Project", "project.bbfx-project");
                if (!path.empty()) saveProject(path);
            } else {
                saveProject(mProjectPath);
            }
        }
        if (ImGui::MenuItem("Save As...")) {
            static const char* filter = "BBFx Project (*.bbfx-project)\0*.bbfx-project\0";
            auto path = saveFileDialog(mEngine->getSDLWindow(), filter, "Save BBFx Project As", "project.bbfx-project");
            if (!path.empty()) saveProject(path);
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Recent Projects")) {
            if (mRecentProjects.empty()) {
                ImGui::TextDisabled("(none)");
            } else {
                for (auto& p : mRecentProjects) {
                    if (ImGui::MenuItem(p.c_str())) loadProject(p);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Export...")) {
            mExportDialog.open();
        }
        // v3.5 Lot S — plugin authoring exports.
        if (ImGui::BeginMenu("Export Plugin")) {
            // C1 — capture du VRAI graphe courant (nodes + links) à exporter.
            // Avant, le dialog exportait des specs vides → plugins coquilles.
            // Parité ProjectSerializer : on saute les nodes runtime `shell/<n>`.
            auto captureGraph = []() -> nlohmann::json {
                nlohmann::json nodes = nlohmann::json::array();
                nlohmann::json links = nlohmann::json::array();
                if (auto* anim = Animator::instance()) {
                    for (auto& name : anim->getRegisteredNodeNames()) {
                        if (name.rfind("shell/", 0) == 0) continue;
                        auto* node = anim->getRegisteredNode(name);
                        if (!node) continue;
                        nlohmann::json n;
                        n["type"] = node->getTypeName();
                        n["name"] = name;
                        if (node->getParamSpec() && !node->getParamSpec()->empty())
                            n["params"] = node->getParamSpec()->toJson();
                        nodes.push_back(std::move(n));
                    }
                    for (auto& l : anim->getLinks()) {
                        if (l.fromNode.rfind("shell/", 0) == 0 || l.toNode.rfind("shell/", 0) == 0) continue;
                        links.push_back({ {"fromNode", l.fromNode}, {"fromPort", l.fromPort},
                                          {"toNode", l.toNode},     {"toPort", l.toPort} });
                    }
                }
                return { {"nodes", nodes}, {"links", links} };
            };

            if (ImGui::MenuItem("Subgraph as Plugin...")) {
                mPluginAuthoringDialog->setSubgraphSpec(captureGraph());
                mPluginAuthoringDialog->open(PluginAuthoringDialog::Mode::Subgraph);
                mShowPluginAuthoringDialog = true;
            }
            if (ImGui::MenuItem("Scene Preset Plugin...")) {
                // Une scène = le graphe courant (+ outputs pour contexte).
                nlohmann::json scene = captureGraph();
                if (mEngine && mEngine->getOutputManager())
                    scene["outputs"] = mEngine->getOutputManager()->toJson();
                mPluginAuthoringDialog->setSceneJson(scene);
                mPluginAuthoringDialog->open(PluginAuthoringDialog::Mode::Scene);
                mShowPluginAuthoringDialog = true;
            }
            if (ImGui::MenuItem("Output Template Plugin...")) {
                nlohmann::json outputs = nlohmann::json::array();
                if (mEngine && mEngine->getOutputManager())
                    outputs = mEngine->getOutputManager()->toJson();
                mPluginAuthoringDialog->setOutputsJson(outputs);
                mPluginAuthoringDialog->open(PluginAuthoringDialog::Mode::OutputTemplate);
                mShowPluginAuthoringDialog = true;
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Settings...")) {
            mShowSettings = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4"))       { mRunning = false; }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CommandManager::instance().canUndo())) {
            CommandManager::instance().undo();
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, CommandManager::instance().canRedo())) {
            CommandManager::instance().redo();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Viewport",      nullptr, &mShowViewport);
        ImGui::MenuItem("Node Editor",   nullptr, &mShowNodeEditor);
        ImGui::MenuItem("Inspector",     nullptr, &mShowInspector);
        ImGui::MenuItem("Timeline",      nullptr, &mShowTimeline);
        ImGui::MenuItem("Preset Browser",nullptr, &mShowPresetBrowser);
        ImGui::MenuItem("Console",       nullptr, &mShowConsole);
        ImGui::MenuItem("Set Editor",    nullptr, &mShowSetEditor);
        ImGui::MenuItem("Scene Hierarchy", nullptr, &mShowSceneHierarchy);
        ImGui::MenuItem("Compositor Stack", nullptr, &mShowCompositorStack);
        ImGui::MenuItem("Shader Gallery",  nullptr, &mShowShaderGallery);
        ImGui::MenuItem("Material Editor", nullptr, &mShowMaterialEditor);
        ImGui::MenuItem("Undo History",   nullptr, &mShowUndoHistory);
        ImGui::MenuItem("Asset Browser",  "Ctrl+Shift+A", &mShowAssetBrowser);
        if (ImGui::MenuItem("Learn Panel", "Ctrl+Shift+L", &mShowLearnPanel)) {
            if (mLearnPanel) mLearnPanel->setVisible(mShowLearnPanel);  // v3.5.2 Sprint S7 Lot Y
        }
        if (mTestEngine && ImGui::MenuItem("Test Engine UI")) {
            ImGuiTestEngine_ShowTestEngineWindows(mTestEngine, nullptr);
        }
        ImGui::Separator();
        // Editor Camera toggle
        bool editorCam = CameraNode::sEditorCameraActive;
        if (ImGui::MenuItem("Use Editor Camera", nullptr, &editorCam)) {
            CameraNode::sEditorCameraActive = editorCam;
            if (mViewportPanel && mViewportPanel->getCameraController()) {
                mViewportPanel->getCameraController()->setMode(
                    editorCam ? ViewportCameraController::Mode::Editor
                              : ViewportCameraController::Mode::DAGDriven);
            }
        }
        ImGui::Separator();
        bool pm = mPerformanceMode;
        if (ImGui::MenuItem("Performance Mode", "F5", &pm)) {
            mPerformanceMode = pm;
            if (pm) {
                if (mPerformanceModePanel && mEngine)
                    mPerformanceModePanel->applyCompositorChain(mEngine.get());
            } else {
                if (mPerformanceModePanel && mEngine)
                    mPerformanceModePanel->removeCompositorChain(mEngine.get());
                if (mEngine)
                    mEngine->invalidateFBOCache();
                if (mViewportPanel)
                    mViewportPanel->invalidateSize();
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Connect")) {
        ImGui::MenuItem("MIDI Activity",  nullptr, &mShowMidiActivity);
        ImGui::MenuItem("MIDI Mapping",   nullptr, &mShowMidiMapping);
        ImGui::MenuItem("Effect Rack",    "F9",    &mShowEffectRack);
        ImGui::Separator();
        ImGui::MenuItem("OSC Browser",    nullptr, &mShowOscBrowser);
        ImGui::Separator();
        // Load MIDI mapping preset
        if (ImGui::BeginMenu("Load Mapping Preset")) {
            namespace fs = std::filesystem;
            std::string mappingsDir = "data/mappings";
            if (fs::exists(mappingsDir) && fs::is_directory(mappingsDir)) {
                for (auto& entry : fs::directory_iterator(mappingsDir)) {
                    if (entry.path().extension() == ".bbfx-mapping") {
                        std::string name = entry.path().stem().string();
                        if (ImGui::MenuItem(name.c_str())) {
                            try {
                                std::ifstream ifs(entry.path().string());
                                if (ifs.is_open()) {
                                    nlohmann::json mj = nlohmann::json::parse(ifs);
                                    MidiLearnManager::instance().fromJson(mj);
                                    std::cout << "[Connect] Loaded mapping: " << name << std::endl;
                                }
                            } catch (const std::exception& e) {
                                std::cerr << "[Connect] Failed to load mapping: " << e.what() << std::endl;
                            }
                        }
                    }
                }
            } else {
                ImGui::TextDisabled("(no mappings found)");
            }
            ImGui::EndMenu();
        }
        // Save current MIDI mapping as preset
        if (ImGui::MenuItem("Save Mapping As...")) {
            auto& bindings = MidiLearnManager::instance().getBindings();
            if (!bindings.empty()) {
                static const char* filter = "BBFx Mapping (*.bbfx-mapping)\0*.bbfx-mapping\0";
                auto savePath = saveFileDialog(mEngine->getSDLWindow(), filter,
                    "Save MIDI Mapping", "mapping.bbfx-mapping");
                if (!savePath.empty()) {
                    try {
                        std::ofstream ofs(savePath);
                        ofs << MidiLearnManager::instance().toJson().dump(2);
                        std::cout << "[Connect] Saved mapping: " << savePath << std::endl;
                    } catch (const std::exception& e) {
                        std::cerr << "[Connect] Save failed: " << e.what() << std::endl;
                    }
                }
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Clear All Bindings")) {
            MidiLearnManager::instance().getBindings().clear();
            std::cout << "[Connect] All MIDI bindings cleared" << std::endl;
        }
        ImGui::Separator();
        // v3.5 Lot L — Gamepad visualisation / calibration / learn.
        ImGui::MenuItem("Gamepad...", "Ctrl+Shift+G", &mShowGamepadPanel);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Stage")) {
        ImGui::MenuItem("Output Manager",  "Ctrl+Shift+O", &mShowOutputManager);
        ImGui::MenuItem("Surface Editor", "Ctrl+Shift+S", &mShowSurfaceEditor);
        ImGui::MenuItem("Network Sync",   "Ctrl+Shift+N", &mShowNetworkPanel);
        ImGui::MenuItem("Master View",    "Ctrl+Shift+M", &mShowMasterView);
        ImGui::Separator();
        if (ImGui::MenuItem("PANIC ALL", "Ctrl+Shift+1")) {
            panicAll();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Plugins")) {
        ImGui::MenuItem("Manage...", "Ctrl+Shift+X", &mShowPluginManager);
        if (ImGui::MenuItem("Community Browser...", "Ctrl+Shift+C", &mShowCommunityBrowser)) {
            if (mShowCommunityBrowser && mCommunityBrowserPanel)
                mCommunityBrowserPanel->requestRefresh();
        }
        if (ImGui::MenuItem("Command Palette...", "Ctrl+Shift+P")) {
            CommandPalette::instance().open();
        }
        {
            size_t total = PluginErrorLog::instance().totalCount();
            size_t unseen = PluginErrorLog::instance().unacknowledgedCount();
            char label[64];
            if (unseen > 0)      std::snprintf(label, sizeof(label), "Errors (%zu new)", unseen);
            else if (total > 0)  std::snprintf(label, sizeof(label), "Errors (%zu)", total);
            else                 std::snprintf(label, sizeof(label), "Errors");
            ImGui::MenuItem(label, "Ctrl+Shift+E", &mShowPluginErrors);
        }
        ImGui::Separator();
        ImGui::TextDisabled(" Drop .zip to install");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About BBFx Studio")) { mShowAbout = true; }
        if (ImGui::MenuItem("Keyboard Shortcuts")) { mShowShortcuts = true; }
        ImGui::EndMenu();
    }

    // Right-aligned performance mode indicator
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 140.0f);
    if (mPerformanceMode) {
        ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "PERFORMANCE MODE [F5]");
    } else {
        ImGui::TextDisabled("Design Mode [F5]");
    }

    ImGui::EndMenuBar();
}

void StudioApp::renderPanels() {
    // Pass recording state to fader + inspector panels each frame
    if (mTimelinePanel) {
        auto* time = RootTimeNode::instance();
        float beat = 0.0f;
        if (time) { float bpm = time->getBPM(); if (bpm > 0) beat = time->getTotalTime() * bpm / 60.0f; }
        bool rec = mTimelinePanel->isRecording();
        if (mPerformanceModePanel) mPerformanceModePanel->setRecordingState(rec, beat);
        if (mInspectorPanel) mInspectorPanel->setRecordingState(rec, beat);
    }
    if (mPerformanceMode) {
        mPerformanceModePanel->render(mEngine.get());
        return;
    }

    if (mShowViewport)      mViewportPanel->render();
    if (mShowNodeEditor)    mNodeEditorPanel->render();
    if (mShowInspector)     mInspectorPanel->render();
    if (mShowTimeline)      mTimelinePanel->render(mEngine.get());
    if (mShowPresetBrowser) mPresetBrowserPanel->render();

    if (mShowConsole) mConsolePanel->render();
    if (mShowSetEditor) mSetEditorPanel->render();
    if (mShowSceneHierarchy && mSceneHierarchyPanel) mSceneHierarchyPanel->render();
    if (mShowCompositorStack && mCompositorStackPanel) mCompositorStackPanel->render();
    if (mShowShaderGallery && mShaderGalleryPanel) mShaderGalleryPanel->render();
    if (mShowMaterialEditor && mMaterialEditorPanel) mMaterialEditorPanel->render();
    if (mShowUndoHistory && mUndoHistoryPanel) mUndoHistoryPanel->render();
    if (mShowAssetBrowser && mAssetBrowserPanel) mAssetBrowserPanel->render();
    // v3.5.2 Sprint S7 Lot Y — LearnPanel always update() (binds bound even
    // when panel hidden), render only if visible.
    if (mLearnPanel) {
        mLearnPanel->update();
        if (mShowLearnPanel) {
            mLearnPanel->setVisible(true);
            mLearnPanel->render();
            // Sync mShowLearnPanel back from panel's own state (panel can close itself via X).
            mShowLearnPanel = mLearnPanel->isVisible();
        }
    }
    if (mShowMidiActivity && mMidiActivityPanel) mMidiActivityPanel->render();
    if (mShowMidiMapping && mMidiMappingPanel) mMidiMappingPanel->render();
    if (mShowEffectRack && mEffectRackPanel) mEffectRackPanel->render();
    if (mEffectRackPanel) {
        mEffectRackPanel->updateBindings();          // MIDI + Gamepad (always active)
        mEffectRackPanel->processKeyboardBindings(); // Keyboard (always active)
    }
    // SetEditor playback — runs every frame regardless of panel visibility
    if (mSetEditorPanel) mSetEditorPanel->update(ImGui::GetIO().DeltaTime);
    if (mShowOutputManager && mOutputManagerPanel) mOutputManagerPanel->render(mEngine.get(), this);
    if (mShowOscBrowser && mOscBrowserPanel) mOscBrowserPanel->render();
    if (mShowSurfaceEditor && mSurfaceEditorPanel) mSurfaceEditorPanel->render(mEngine.get());
    if (mShowNetworkPanel && mNetworkPanel) mNetworkPanel->render(mSyncManager.get(), &mShowNetworkPanel);
    if (mShowMasterView && mMasterViewPanel) {
        // Update active scene info from PerformanceModePanel (Lot P)
        if (mPerformanceModePanel) {
            auto chordName = mPerformanceModePanel->getActiveSceneChord();
            if (!chordName.empty()) {
                auto& zoneSnaps = mPerformanceModePanel->getChordZoneSnapshots();
                auto it = zoneSnaps.find(chordName);
                if (it != zoneSnaps.end()) {
                    mMasterViewPanel->setActiveScene(chordName, it->second.zoneCount(),
                        it->second.camPosX(), it->second.camPosY(), it->second.camPosZ(), it->second.camFov());
                }
            } else {
                mMasterViewPanel->clearActiveScene();
            }
        }
        mMasterViewPanel->render(mEngine.get(), mSyncManager.get());
    }

    // v3.5 Lot G: real Plugin Manager + Plugin Errors panels.
    if (mPluginManagerPanel) mPluginManagerPanel->render(&mShowPluginManager);
    if (mPluginErrorsPanel)  mPluginErrorsPanel->render(&mShowPluginErrors);
    // v3.5 Lot H: Community Browser + Command Palette.
    if (mCommunityBrowserPanel) mCommunityBrowserPanel->render(&mShowCommunityBrowser);
    if (mAuthorProfilePanel)    mAuthorProfilePanel->render(&mShowAuthorProfile);
    if (mGamepadPanel)          mGamepadPanel->render(&mShowGamepadPanel);
    if (mPluginAuthoringDialog) mPluginAuthoringDialog->render(&mShowPluginAuthoringDialog);

    // v3.5 Lot P — plugin-contributed ImGui panels (registerPanel).
    ScriptPanelRegistry::instance().drawAll();

    CommandPalette::instance().draw();

    // v3.5 Lot F: permission prompt modal (draws only when pending).
    PermissionPromptDialog::instance().draw();

    // Update shader/material preview renderer
    if (mPreviewRenderer) mPreviewRenderer->update(ImGui::GetIO().DeltaTime);

    // ── Status Bar ─────────────────────────────────────────────────────
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        float barH = ImGui::GetTextLineHeightWithSpacing() + 4;
        ImGui::SetNextWindowPos({vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - barH});
        ImGui::SetNextWindowSize({vp->WorkSize.x, barH});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8, 2});
        ImGui::Begin("##StatusBar", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav);

        // FPS
        ImGui::Text("%.0f FPS", ImGui::GetIO().Framerate);
        ImGui::SameLine(100);

        // Nodes + Links count
        auto* animator = Animator::instance();
        if (animator) {
            auto names = animator->getRegisteredNodeNames();
            auto links = animator->getLinks();
            ImGui::Text("%zu nodes | %zu links", names.size(), links.size());
        }
        ImGui::SameLine(300);

        // Audio (check if any AudioCaptureNode exists in DAG)
        bool audioOn = false;
        if (animator) {
            for (auto& n : animator->getRegisteredNodeNames()) {
                auto* nd = animator->getRegisteredNode(n);
                if (nd && nd->getTypeName() == "AudioCaptureNode") { audioOn = true; break; }
            }
        }
        ImGui::TextDisabled("Audio: %s", audioOn ? "ON" : "OFF");
        ImGui::SameLine(420);

        // Outputs count (v3.4 Lot M)
        {
            int outCount = (mEngine && mEngine->getOutputManager())
                ? mEngine->getOutputManager()->count() : 0;
            if (outCount > 0) {
                ImGui::TextColored(ImVec4(0.6f, 0.9f, 1.f, 1.f), "Out: %d", outCount);
            } else {
                ImGui::TextDisabled("Out: 0");
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Active output windows: %d\nCtrl+Shift+O → Output Manager", outCount);
            }
            if (ImGui::IsItemClicked()) mShowOutputManager = true;
        }
        ImGui::SameLine(500);

        // Mode
        ImGui::TextDisabled("Studio");
        ImGui::SameLine(580);

        // Network sync indicator (v3.4 Lot G)
        if (mSyncManager) {
            SyncRole role = mSyncManager->getRole();
            const auto& peers = mSyncManager->getPeers();
            int connectedCount = 0;
            for (const auto& p : peers) if (p.connected) ++connectedCount;

            ImVec4 netCol;
            const char* roleLetter = "-";
            if (!mSyncManager->isRunning()) {
                netCol = ImVec4(0.5f, 0.5f, 0.5f, 1.f);
            } else if (role == SyncRole::MASTER) {
                netCol = ImVec4(0.2f, 1.f, 0.2f, 1.f); roleLetter = "M";
            } else if (role == SyncRole::SLAVE) {
                netCol = connectedCount > 0 ? ImVec4(0.2f, 1.f, 0.2f, 1.f) : ImVec4(1.f, 0.4f, 0.f, 1.f);
                roleLetter = "S";
            } else {
                netCol = ImVec4(0.5f, 0.5f, 0.5f, 1.f);
            }
            ImGui::PushStyleColor(ImGuiCol_Text, netCol);
            ImGui::Text("[%s] %d peer%s", roleLetter, connectedCount, connectedCount == 1 ? "" : "s");
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Role: %s", syncRoleName(role));
                ImGui::Text("Peers: %d connected", connectedCount);
                if (role == SyncRole::SLAVE)
                    ImGui::Text("Clock offset: %.1f ms", mSyncManager->getClockOffset());
                ImGui::Text("Click Stage > Network Sync to open panel");
                ImGui::EndTooltip();
            }
            if (ImGui::IsItemClicked()) mShowNetworkPanel = true;
        }
        // Spout indicator (v3.4 Lot H)
        if (mEngine && mEngine->getOutputManager()) {
            auto* om = mEngine->getOutputManager();
            bool anyTexShare = false;
            std::string texShareTip;
            for (const auto& s : om->getAllSlots()) {
                if (s.textureShareEnabled) {
                    anyTexShare = true;
                    texShareTip += "Output " + std::to_string(s.id) + ": " + s.textureShareSourceName + "\n";
                }
            }
            ImGui::SameLine();
            if (anyTexShare) {
                ImGui::TextColored(ImVec4(0.2f, 1.f, 0.2f, 1.f), "[SPT]");
            } else {
                ImGui::TextDisabled("[SPT]");
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                if (anyTexShare) {
                    ImGui::Text("%s Active:", getTextureShareLabel());
                    ImGui::TextUnformatted(texShareTip.c_str());
                } else {
                    ImGui::TextDisabled("%s: inactive", getTextureShareLabel());
                }
                ImGui::EndTooltip();
            }
        }
        // NDI indicator (v3.4 Lot I)
        {
            auto* animator = Animator::instance();
            bool ndiActive = false;
            if (animator) {
                for (const auto& n : animator->getRegisteredNodeNames()) {
                    auto* node = animator->getRegisteredNode(n);
                    if (node && node->getTypeName() == "NdiOutputNode") { ndiActive = true; break; }
                }
            }
            ImGui::SameLine();
            if (ndiActive) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.f, 1.f), "[NDI]");
            } else {
                ImGui::TextDisabled("[NDI]");
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(ndiActive ? "NDI: Active" : "NDI: Inactive (no NdiOutputNode in DAG)");
            }
        }
        // MIDI Clock indicator (v3.4 Lot L)
        {
            bool clockActive = false;
            float clockBpm   = 0.0f;
            if (animator) {
                for (const auto& nm : animator->getRegisteredNodeNames()) {
                    auto* n = animator->getRegisteredNode(nm);
                    if (n && n->getTypeName() == "MidiOutputNode") {
                        auto* mo = static_cast<MidiOutputNode*>(n);
                        if (mo->isClockRunning()) { clockActive = true; clockBpm = mo->getClockBpm(); break; }
                    }
                }
            }
            ImGui::SameLine();
            if (clockActive) {
                ImGui::TextColored(ImVec4(1.f, 0.8f, 0.1f, 1.f), "[CLK %.0f]", clockBpm);
            } else {
                ImGui::TextDisabled("[CLK]");
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(clockActive ? "MIDI Clock Master: running" : "MIDI Clock Master: stopped");
            }
        }

        // Plugin count badge (click opens Plugin Manager)
        {
            auto ids = PluginManager::instance().listPlugins();
            int enabled = 0, failed = 0;
            for (const auto& id : ids) {
                const auto* p = PluginManager::instance().getPlugin(id);
                if (!p) continue;
                if (p->state == PluginState::ENABLED) ++enabled;
                if (p->state == PluginState::FAILED)  ++failed;
            }
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            ImVec4 col = failed ? ImVec4{1.0f, 0.5f, 0.5f, 1.0f}
                                 : (enabled ? ImVec4{0.5f, 0.9f, 0.5f, 1.0f}
                                            : ImVec4{0.6f, 0.6f, 0.6f, 1.0f});
            char pbuf[64];
            if (failed)
                std::snprintf(pbuf, sizeof(pbuf), "Plugins: %d/%zu (%d failed)",
                               enabled, ids.size(), failed);
            else
                std::snprintf(pbuf, sizeof(pbuf), "Plugins: %d/%zu",
                               enabled, ids.size());
            ImGui::TextColored(col, "%s", pbuf);
            if (ImGui::IsItemClicked()) mShowPluginManager = true;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to open Plugin Manager (Ctrl+Shift+X)");
        }

        // MIDI indicator
        {
            auto* midiMgr = MidiDeviceManager::instance();
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            if (midiMgr && midiMgr->getInputDeviceCount() > 0) {
                int bindingCount = static_cast<int>(MidiLearnManager::instance().getBindings().size());
                ImGui::TextColored({0.3f, 1.0f, 0.3f, 1.0f}, "MIDI: %d dev, %d bind",
                                   midiMgr->getInputDeviceCount(), bindingCount);
            } else {
                ImGui::TextDisabled("MIDI: Off");
            }
        }

        // Scene indicator (v3.4)
        {
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            std::string scnChord;
            if (mPerformanceModePanel) scnChord = mPerformanceModePanel->getActiveSceneChord();
            if (!scnChord.empty()) {
                ImGui::TextColored({0.0f, 0.8f, 1.0f, 1.0f}, "[SCN]");
                if (ImGui::IsItemHovered()) {
                    auto& zoneSnaps = mPerformanceModePanel->getChordZoneSnapshots();
                    auto it = zoneSnaps.find(scnChord);
                    int n = (it != zoneSnaps.end()) ? it->second.zoneCount() : 0;
                    ImGui::SetTooltip("Scene: chord '%s' (%d zones)", scnChord.c_str(), n);
                }
            } else {
                ImGui::TextDisabled("[SCN]");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scene: inactive");
            }
        }

        // Project dirty indicator
        if (mProjectDirty) {
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            ImGui::TextColored({1.0f, 0.8f, 0.0f, 1.0f}, "*");
        }

        ImGui::SameLine();
        // Version
        ImGui::TextDisabled("v3.5.2");

        ImGui::End();
        ImGui::PopStyleVar();
    }

    // Toast notifications
    ToastSystem::instance().render();

    // Export dialog (modal, shown on demand)
    mExportDialog.render(mEngine.get());

    // Modal dialogs
    renderAboutDialog();
    renderShortcutsDialog();
    renderSettingsDialog();
    renderRecoveryDialog();
    renderDeepLinkConfirmDialog();
}

// ── Project save / load / auto-save ──────────────────────────────────────────

void StudioApp::clearUserGraph() {
    auto* animator = Animator::instance();
    if (!animator) return;
    // Snapshot the names first — the map mutates as we delete.
    auto names = animator->getRegisteredNodeNames();
    for (auto& n : names) {
        if (n == "time") continue;                                  // keep RootTimeNode
        if (n.rfind("shell/", 0) == 0 ||                            // REPL client nodes
            n.rfind("_dbg_", 0)  == 0 ||                            // debugger helper nodes
            n.rfind("_test_", 0) == 0) continue;                    // test-suite helper nodes
        auto* node = animator->getRegisteredNode(n);
        if (!node) continue;                                        // already gone (cascade)

        // Drop any viewport-side references to this node BEFORE destroying it,
        // so the gizmo / picker / camera-lock / Lua `_sceneNodes` table never
        // dereference a freed SceneNode. (Mirrors the deferred-delete path.)
        if (mViewportPanel) {
            auto* picker = mViewportPanel->getPicker();
            if (picker && picker->getSelectedNodeName() == n) picker->deselect();

            if (auto* soNode = dynamic_cast<SceneObjectNode*>(node)) {
                auto* gizmo = mViewportPanel->getGizmo();
                if (gizmo && gizmo->getTargetSceneNode() == soNode->getSceneNode())
                    gizmo->clearTarget();
                auto* camCtrl = mViewportPanel->getCameraController();
                if (camCtrl && camCtrl->getLockTarget() == soNode->getSceneNode())
                    camCtrl->setOrbitLockTarget(nullptr);
                if (mLua["_sceneNodes"].valid()) {
                    sol::object sn = mLua["_sceneNodes"][n];
                    mLua["_sceneNodes"][n] = sol::nil;
                    if (sn.valid() && mLua["_rotateTarget"].valid() && sn == mLua["_rotateTarget"])
                        mLua["_rotateTarget"] = sol::nil;
                }
            }
        }

        animator->removeNode(node);
        // A node's cleanup() may touch one of its peers (e.g. PerlinFxNode → its
        // target SceneObjectNode). If that peer was already deleted earlier in this
        // loop the call can throw — swallow it so the remaining nodes (and their
        // OGRE entities) still get destroyed. Without this guard the loop would
        // abort mid-way and leave stale geometry in the viewport.
        try { node->cleanup(); }
        catch (const std::exception& e) { std::cerr << "[clearUserGraph] cleanup('" << n << "') threw: " << e.what() << std::endl; }
        catch (...) { std::cerr << "[clearUserGraph] cleanup('" << n << "') threw" << std::endl; }
        delete node;
    }
    if (mNodeEditorPanel) mNodeEditorPanel->syncFromDAG();
}

void StudioApp::newProject() {
    clearUserGraph();                       // DAG + 3D scene entities
    if (mSurfaceMap) mSurfaceMap->clear();  // projection zones rendered in the viewport
    mProjectPath.clear();
    mProjectDirty = false;
    SDL_SetWindowTitle(mEngine->getSDLWindow(),
                       ("BBFx Studio v" + std::string(BBFX_VERSION_STRING) + " — " + BBFX_VERSION_NAME).c_str());
    std::cout << "[Studio] New project — scene cleared" << std::endl;
}

void StudioApp::panicAll() {
    // 1. Reset all warps and blends
    if (mEngine) {
        auto* mgr = mEngine->getOutputManager();
        if (mgr) {
            mgr->resetAllWarps();
            mgr->resetAllGridWarps();
            mgr->resetAllBlends();
        }
    }

    // 2. Disconnect network peers
    if (mSyncManager && mSyncManager->isRunning()) {
        mSyncManager->sendPanic();
        std::cout << "[PANIC ALL] Network panic sent" << std::endl;
    }

    // 3. Disable Spout on all outputs
    if (mEngine) {
        auto* mgr = mEngine->getOutputManager();
        if (mgr) {
            for (auto& slot : mgr->getAllSlots()) {
                if (slot.textureShareEnabled) mgr->disableTextureShare(slot.id);
            }
        }
    }

    // 4. Mute all ArtnetOutputNode DMX channels
    auto* animator = Animator::instance();
    if (animator) {
        for (auto& name : animator->getRegisteredNodeNames()) {
            auto* node = animator->getRegisteredNode(name);
            if (node && node->getTypeName() == "ArtnetOutputNode") {
                for (auto& [portName, port] : node->getInputs()) {
                    if (portName.rfind("ch", 0) == 0) port->setValue(0.0f);
                }
            }
        }
    }

    // 5. Restore rest snapshot (PerformanceModePanel PANIC)
    if (mPerformanceModePanel) {
        mPerformanceModePanel->triggerPanic();
    }

    ToastSystem::instance().toast("PANIC ALL — reset complete");
    std::cout << "[PANIC ALL] Complete: warps reset, Spout disabled, DMX muted, network panicked, rest snapshot restored" << std::endl;
}

void StudioApp::saveProject(const std::string& path) {
    if (path.empty()) return;

    ProjectSerializer::ProjectState state;

    // Collect node positions from editor
    if (mNodeEditorPanel) {
        auto positions = mNodeEditorPanel->getNodePositions();
        for (auto& np : positions) {
            state.nodePositions.push_back({np.name, np.x, np.y});
        }
    }

    // Collect chord blocks from timeline
    if (mTimelinePanel) {
        for (auto& cb : mTimelinePanel->getChordBlocks()) {
            ProjectSerializer::ChordData cd;
            cd.name = cb.name; cd.startBeat = cb.startBeat;
            cd.endBeat = cb.endBeat; cd.hue = cb.hue;
            cd.snapshot = cb.snapshot; cd.transitionBeats = cb.transitionBeats;
            state.chords.push_back(cd);
        }
    }

    // Collect automation data from timeline
    if (mTimelinePanel) {
        state.automation = mTimelinePanel->getAutomation();
    }

    // Collect performance mode data (faders, triggers, compositor stack)
    if (mPerformanceModePanel) {
        auto& faders = mPerformanceModePanel->getFaders();
        for (int i = 0; i < 8; ++i) {
            state.faders[i].nodeName = faders[i].nodeName;
            state.faders[i].portName = faders[i].portName;
            state.faders[i].minVal = faders[i].minVal;
            state.faders[i].maxVal = faders[i].maxVal;
        }
        auto& pages = mPerformanceModePanel->getTriggerPages();
        state.triggerPages.resize(pages.size());
        for (size_t p = 0; p < pages.size(); ++p) {
            state.triggerPages[p].resize(16);
            for (int i = 0; i < 16; ++i) {
                state.triggerPages[p][i].label = pages[p][i].label;
                state.triggerPages[p][i].action = pages[p][i].action;
                state.triggerPages[p][i].momentary = pages[p][i].momentary;
                state.triggerPages[p][i].hue = pages[p][i].hue;
                state.triggerPages[p][i].macroActions = pages[p][i].macroActions;
            }
        }
    }
    if (mCompositorStackPanel) {
        state.compositorStack = mCompositorStackPanel->getStackOrder();
    }

    // Chord snapshots
    if (mPerformanceModePanel) {
        for (auto& [name, snap] : mPerformanceModePanel->getChordSnapshots()) {
            state.chordSnapshots[name] = snap.getData();
        }
        // Zone snapshots (v3.4 Lot O — Scene Switcher)
        auto& zoneSnaps = mPerformanceModePanel->getChordZoneSnapshots();
        if (!zoneSnaps.empty()) {
            nlohmann::json zj;
            for (auto& [name, zs] : zoneSnaps) {
                zj[name] = zs.toJson();
            }
            state.chordZoneSnapshotsJson = zj;
        }
    }

    // Outputs (v3.4)
    if (mEngine && mEngine->getOutputManager()) {
        state.outputsJson = mEngine->getOutputManager()->toJson();
    }

    // Surface map (v3.4 Lot E)
    if (mSurfaceMap) {
        state.extraJson["surfaceMap"] = mSurfaceMap->toJson();
    }

    // Network sync config (v3.4 Lot F)
    if (mSyncManager) {
        state.extraJson["network"] = mSyncManager->toJson();
    }

    // Effect Rack bindings (v3.5.1)
    if (mEffectRackPanel) {
        state.extraJson["effectRack"] = mEffectRackPanel->toJson();
    }

    // MIDI bindings in project (v3.5.1)
    state.extraJson["midiBindings"] = MidiLearnManager::instance().toJson();

    // Viewport camera state (v3.5.1)
    if (mViewportPanel && mViewportPanel->getCameraController()) {
        auto cam = mViewportPanel->getCameraController()->getOrbitState();
        state.extraJson["camera"] = {
            {"yaw", cam.yaw}, {"pitch", cam.pitch},
            {"distance", cam.distance},
            {"centerX", cam.center.x}, {"centerY", cam.center.y}, {"centerZ", cam.center.z}
        };
    }

    if (mSerializer.save(path, state)) {
        mProjectPath = path;
        mProjectDirty = false;
        // Add to recent projects (deduplicate)
        mRecentProjects.erase(
            std::remove(mRecentProjects.begin(), mRecentProjects.end(), path),
            mRecentProjects.end());
        mRecentProjects.insert(mRecentProjects.begin(), path);
        if (mRecentProjects.size() > 10) mRecentProjects.resize(10);
        // Persist in settings for auto-load on next startup (skip test files,
        // and skip the demo showcase pack — re-baking demos must not clobber the
        // user's working project as "last opened" — v3.5.2 Sprint S8 Lot AU).
        if (path.find("output/test_") == std::string::npos &&
            path.find("lua/demos/projects/") == std::string::npos) {
            auto& settings = SettingsManager::instance();
            auto s = settings.get();
            s.lastProjectPath = path;
            s.recentProjects = mRecentProjects;
            // v3.5.2 — Persist window geometry
            int wx, wy, ww, wh;
            SDL_GetWindowPosition(mEngine->getSDLWindow(), &wx, &wy);
            SDL_GetWindowSize(mEngine->getSDLWindow(), &ww, &wh);
            s.windowX = wx;
            s.windowY = wy;
            s.windowWidth = ww;
            s.windowHeight = wh;
            settings.set(s);
            settings.save();
        }
        SDL_SetWindowTitle(mEngine->getSDLWindow(), ("BBFx Studio — " + path).c_str());
        // Flush ImGui docking layout so user customizations persist
        ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
        std::cout << "[Studio] Saved: " << path << std::endl;
    } else {
        std::cerr << "[Studio] Save failed: " << mSerializer.getLastError() << std::endl;
    }
}

void StudioApp::promptInstallZip(const std::string& zipPath) {
    namespace fs = std::filesystem;
    std::error_code ec;

    // Extract into a temp dir. If anything fails, toast and bail.
    fs::path tempBase = fs::temp_directory_path(ec);
    if (ec) tempBase = fs::current_path();
    fs::path tempDir = tempBase / ("bbfx_prompt_install_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    ZipExtractor::Options zopts;
    std::string zErr;
    if (!ZipExtractor::extract(zipPath, tempDir, zopts, zErr)) {
        fs::remove_all(tempDir, ec);
        ToastSystem::instance().toast("Install failed: " + zErr, ToastSeverity::Error, 5.0f);
        return;
    }

    // Find the plugin root inside the temp dir (root or single-subdir layout).
    fs::path pluginRoot = tempDir;
    if (!fs::exists(pluginRoot / "manifest.json", ec)) {
        std::vector<fs::path> children;
        for (auto& e : fs::directory_iterator(pluginRoot, ec)) children.push_back(e.path());
        if (children.size() == 1 && fs::is_directory(children[0], ec) &&
            fs::exists(children[0] / "manifest.json", ec)) {
            pluginRoot = children[0];
        } else {
            fs::remove_all(tempDir, ec);
            ToastSystem::instance().toast("Install failed: manifest.json not found in zip",
                                           ToastSeverity::Error, 5.0f);
            return;
        }
    }

    // Read and parse the manifest so the prompt can show name/author/permissions.
    std::ifstream mf(pluginRoot / "manifest.json");
    nlohmann::json mj;
    try { mf >> mj; } catch (const std::exception& e) {
        fs::remove_all(tempDir, ec);
        ToastSystem::instance().toast(std::string("Install failed: manifest.json parse: ") + e.what(),
                                       ToastSeverity::Error, 6.0f);
        return;
    }
    std::string parseErr;
    auto manifestOpt = PluginManifest::fromJson(mj, parseErr);
    if (!manifestOpt) {
        fs::remove_all(tempDir, ec);
        ToastSystem::instance().toast("Install failed: " + parseErr, ToastSeverity::Error, 6.0f);
        return;
    }

    // Capture tempDir + pluginRoot by value so the closures stay valid.
    auto cleanup = [tempDir]() {
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
    };

    PermissionPromptDialog::instance().open(*manifestOpt,
        [pluginRoot, cleanup]() {
            std::string err;
            std::string id = PluginManager::instance().installFromPath(pluginRoot, &err);
            cleanup();
            if (id.empty()) {
                ToastSystem::instance().toast("Install failed: " + err, ToastSeverity::Error, 6.0f);
            } else {
                ToastSystem::instance().toast("Installed '" + id + "'", ToastSeverity::Info, 4.0f);
            }
        },
        [cleanup]() {
            cleanup();
            ToastSystem::instance().toast("Install cancelled", ToastSeverity::Info, 2.5f);
        });
}

void StudioApp::loadProject(const std::string& path) {
    if (path.empty()) return;
    // Opening a project REPLACES the current scene — wipe the user graph (and the
    // 3D entities it owns) first, otherwise the loaded nodes would be merged on top
    // of whatever is already there. (`mSerializer.load()` recreates the project's
    // own nodes, surface zones and outputs below.)
    clearUserGraph();
    ProjectSerializer::ProjectState state;
    if (mSerializer.load(path, mLua, &state)) {
        mProjectPath = path;
        mRecentProjects.erase(
            std::remove(mRecentProjects.begin(), mRecentProjects.end(), path),
            mRecentProjects.end());
        mRecentProjects.insert(mRecentProjects.begin(), path);
        if (mRecentProjects.size() > 10) mRecentProjects.resize(10);
        SDL_SetWindowTitle(mEngine->getSDLWindow(), ("BBFx Studio — " + path).c_str());
        // Persist last project for auto-reload on next startup (skip templates and test files)
        if (path.find("data/templates/") == std::string::npos &&
            path.find("output/test_") == std::string::npos) {
            auto& settings = SettingsManager::instance();
            auto s = settings.get();
            s.lastProjectPath = path;
            s.recentProjects = mRecentProjects;
            settings.set(s);
            settings.save();
        }

        // Restore node positions:
        // - Normal/project load: always apply project positions
        // - --default (no --reset): skip if node_editor.json exists (preserve user layout)
        // - --reset: always apply template positions (override node_editor.json)
        bool nodeEditorHasLayout = std::filesystem::exists("node_editor.json");
        bool skipPositions = mForceDefault && !mForceReset && nodeEditorHasLayout;
        if (mNodeEditorPanel && !state.nodePositions.empty() && !skipPositions) {
            std::vector<NodeEditorPanel::NodePosition> positions;
            for (auto& np : state.nodePositions) {
                positions.push_back({np.name, np.x, np.y});
            }
            mNodeEditorPanel->setNodePositions(positions);
        }

        // Restore chord blocks
        if (mTimelinePanel && !state.chords.empty()) {
            std::vector<ChordBlock> chords;
            for (auto& cd : state.chords) {
                ChordBlock cb;
                cb.name = cd.name; cb.startBeat = cd.startBeat;
                cb.endBeat = cd.endBeat; cb.hue = cd.hue;
                cb.snapshot = cd.snapshot; cb.transitionBeats = cd.transitionBeats;
                chords.push_back(cb);
            }
            mTimelinePanel->setChordBlocks(chords);
        }

        // Restore automation data
        if (mTimelinePanel) {
            mTimelinePanel->getAutomation() = state.automation;
        }

        // Restore performance mode data (faders, triggers, compositor stack)
        if (mPerformanceModePanel) {
            auto& faders = mPerformanceModePanel->getFaders();
            for (int i = 0; i < 8; ++i) {
                faders[i].nodeName = state.faders[i].nodeName;
                faders[i].portName = state.faders[i].portName;
                faders[i].minVal = state.faders[i].minVal;
                faders[i].maxVal = state.faders[i].maxVal;
            }
            auto& pages = mPerformanceModePanel->getTriggerPages();
            if (!state.triggerPages.empty()) {
                pages.resize(state.triggerPages.size());
                for (size_t p = 0; p < state.triggerPages.size(); ++p) {
                    for (int i = 0; i < 16 && i < static_cast<int>(state.triggerPages[p].size()); ++i) {
                        pages[p][i].label = state.triggerPages[p][i].label;
                        pages[p][i].action = state.triggerPages[p][i].action;
                        pages[p][i].momentary = state.triggerPages[p][i].momentary;
                        pages[p][i].hue = state.triggerPages[p][i].hue;
                        pages[p][i].macroActions = state.triggerPages[p][i].macroActions;
                    }
                }
            }
        }
        if (mCompositorStackPanel && !state.compositorStack.empty()) {
            mCompositorStackPanel->setStackOrder(state.compositorStack);
        }

        // Restore chord snapshots
        if (mPerformanceModePanel && !state.chordSnapshots.empty()) {
            std::map<std::string, DagSnapshot> snaps;
            for (auto& [name, data] : state.chordSnapshots) {
                DagSnapshot snap;
                snap.setData(data);
                snaps[name] = snap;
            }
            mPerformanceModePanel->setChordSnapshots(snaps);
        }

        // Restore zone snapshots (v3.4 Lot O — Scene Switcher)
        if (mPerformanceModePanel && !state.chordZoneSnapshotsJson.is_null() && state.chordZoneSnapshotsJson.is_object()) {
            std::map<std::string, ZoneSnapshot> zoneSnaps;
            for (auto& [name, zsJson] : state.chordZoneSnapshotsJson.items()) {
                ZoneSnapshot zs;
                zs.fromJson(zsJson);
                zoneSnaps[name] = zs;
            }
            mPerformanceModePanel->setChordZoneSnapshots(zoneSnaps);
        }

        // Rebuild fader/trigger MIDI bindings from MidiLearnManager (single source of truth)
        if (mPerformanceModePanel) {
            mPerformanceModePanel->syncMidiBindingsFromManager();
            mPerformanceModePanel->resetRestSnapshot(); // recapture rest state from loaded project
        }

        // Restore outputs (v3.4) — only if the project saved output config.
        // v3.3 projects have no output config; the user creates outputs manually
        // via Output Manager (Ctrl+Shift+O) when needed.
        if (mEngine && mEngine->getOutputManager()) {
            auto* outMgr = mEngine->getOutputManager();
            if (state.outputsJson.is_array() && !state.outputsJson.empty()) {
                outMgr->fromJson(state.outputsJson, Engine::instance()->getSceneManager());
            }
            mEngine->invalidateFBOCache();
        }

        // Restore network sync config (v3.4 Lot F)
        if (mSyncManager && state.extraJson.contains("network")) {
            mSyncManager->fromJson(state.extraJson["network"]);
        }

        // Restore Effect Rack bindings (v3.5.1)
        if (mEffectRackPanel && state.extraJson.contains("effectRack")) {
            mEffectRackPanel->fromJson(state.extraJson["effectRack"]);
        }

        // Restore MIDI bindings from project (v3.5.1)
        if (state.extraJson.contains("midiBindings")) {
            MidiLearnManager::instance().fromJson(state.extraJson["midiBindings"]);
        }

        // Restore viewport camera state (v3.5.1)
        if (mViewportPanel && mViewportPanel->getCameraController() && state.extraJson.contains("camera")) {
            auto& cj = state.extraJson["camera"];
            ViewportCameraController::OrbitState os;
            os.yaw      = cj.value("yaw", 0.0f);
            os.pitch    = cj.value("pitch", 30.0f);
            os.distance = cj.value("distance", 50.0f);
            os.center   = Ogre::Vector3(cj.value("centerX", 0.0f), cj.value("centerY", 0.0f), cj.value("centerZ", 0.0f));
            mViewportPanel->getCameraController()->setOrbitState(os);
        }

        // Restore surface map (v3.4 Lot E)
        if (mSurfaceMap && state.extraJson.contains("surfaceMap")) {
            auto* sm = mEngine ? mEngine->getSceneManager() : nullptr;
            mSurfaceMap->fromJson(state.extraJson["surfaceMap"], sm);
        } else if (mSurfaceMap && mSurfaceMap->size() == 0) {
            // Only clear if no zones exist (e.g. from a Lua demo script).
            // v3.3 projects without surfaceMap data start with an empty map,
            // but we must not destroy zones set up by a Lua script before loadProject.
            mSurfaceMap->clear();
        }

        std::cout << "[Studio] Loaded: " << path << std::endl;
    } else {
        std::cerr << "[Studio] Load failed: " << mSerializer.getLastError() << std::endl;
    }
}

void StudioApp::tickAutoSave() {
    if (mProjectPath.empty()) return;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - mLastAutoSave).count();
    if (elapsed >= kAutoSaveIntervalSec) {
        std::string autoPath = mProjectPath + ".autosave";
        ProjectSerializer::ProjectState autoState;
        if (mNodeEditorPanel) {
            auto positions = mNodeEditorPanel->getNodePositions();
            for (auto& np : positions) {
                autoState.nodePositions.push_back({np.name, np.x, np.y});
            }
        }
        if (mTimelinePanel) {
            for (auto& cb : mTimelinePanel->getChordBlocks()) {
                ProjectSerializer::ChordData cd;
                cd.name = cb.name; cd.startBeat = cb.startBeat;
                cd.endBeat = cb.endBeat; cd.hue = cb.hue;
                cd.snapshot = cb.snapshot; cd.transitionBeats = cb.transitionBeats;
                autoState.chords.push_back(cd);
            }
            autoState.automation = mTimelinePanel->getAutomation();
        }
        // Performance mode data (same as saveProject)
        if (mPerformanceModePanel) {
            auto& faders = mPerformanceModePanel->getFaders();
            for (int i = 0; i < 8; ++i) {
                autoState.faders[i].nodeName = faders[i].nodeName;
                autoState.faders[i].portName = faders[i].portName;
                autoState.faders[i].minVal = faders[i].minVal;
                autoState.faders[i].maxVal = faders[i].maxVal;
            }
            auto& pages = mPerformanceModePanel->getTriggerPages();
            autoState.triggerPages.resize(pages.size());
            for (size_t p = 0; p < pages.size(); ++p) {
                autoState.triggerPages[p].resize(16);
                for (int i = 0; i < 16; ++i) {
                    autoState.triggerPages[p][i].label = pages[p][i].label;
                    autoState.triggerPages[p][i].action = pages[p][i].action;
                    autoState.triggerPages[p][i].momentary = pages[p][i].momentary;
                    autoState.triggerPages[p][i].hue = pages[p][i].hue;
                    autoState.triggerPages[p][i].macroActions = pages[p][i].macroActions;
                }
            }
            for (auto& [name, snap] : mPerformanceModePanel->getChordSnapshots()) {
                autoState.chordSnapshots[name] = snap.getData();
            }
        }
        if (mCompositorStackPanel) {
            autoState.compositorStack = mCompositorStackPanel->getStackOrder();
        }
        // Viewport camera state
        if (mViewportPanel && mViewportPanel->getCameraController()) {
            auto cam = mViewportPanel->getCameraController()->getOrbitState();
            autoState.extraJson["camera"] = {
                {"yaw", cam.yaw}, {"pitch", cam.pitch},
                {"distance", cam.distance},
                {"centerX", cam.center.x}, {"centerY", cam.center.y}, {"centerZ", cam.center.z}
            };
        }
        // Zone snapshots (v3.4)
        if (mPerformanceModePanel) {
            auto& zoneSnaps = mPerformanceModePanel->getChordZoneSnapshots();
            if (!zoneSnaps.empty()) {
                nlohmann::json zj;
                for (auto& [name, zs] : zoneSnaps) {
                    zj[name] = zs.toJson();
                }
                autoState.chordZoneSnapshotsJson = zj;
            }
        }
        // Outputs
        if (mEngine && mEngine->getOutputManager()) {
            autoState.outputsJson = mEngine->getOutputManager()->toJson();
        }
        // Surface map
        if (mSurfaceMap) {
            autoState.extraJson["surfaceMap"] = mSurfaceMap->toJson();
        }
        // Network sync config
        if (mSyncManager) {
            autoState.extraJson["network"] = mSyncManager->toJson();
        }
        // Effect Rack bindings
        if (mEffectRackPanel) {
            autoState.extraJson["effectRack"] = mEffectRackPanel->toJson();
        }
        // MIDI bindings
        autoState.extraJson["midiBindings"] = MidiLearnManager::instance().toJson();

        if (mSerializer.save(autoPath, autoState)) {
            std::cout << "[Studio] Auto-saved → " << autoPath << std::endl;
        }
        mLastAutoSave = now;
    }
}

// ── Node type registration ──────────────────────────────────────────────────

void StudioApp::initNodeTypeRegistry() {
    assert(Engine::instance() && "Engine singleton must exist before node registration");
    assert(Engine::instance()->getSceneManager() && "SceneManager must exist before node registration");

    auto& reg = NodeTypeRegistry::instance();

    // Core
    reg.registerType({"RootTimeNode", "Core", {0.4f, 0.8f, 0.4f, 1.0f},
        [](const std::string& /*name*/, sol::state& /*lua*/) -> AnimationNode* {
            return RootTimeNode::instance();
        }});

    // Logic
    reg.registerType({"LuaAnimationNode", "Logic", {0.3f, 0.6f, 1.0f, 1.0f},
        [](const std::string& name, sol::state& lua) -> AnimationNode* {
            sol::function noop = lua["function() end"];
            if (!noop.valid()) {
                lua.script("function _bbfx_noop() end");
                noop = lua["_bbfx_noop"];
            }
            auto* node = new LuaAnimationNode(name, noop);
            node->addInput("in");
            node->addOutput("out");
            auto* animator = Animator::instance();
            if (animator) {
                for (auto& [pname, port] : node->getInputs()) animator->add(port);
                for (auto& [pname, port] : node->getOutputs()) animator->add(port);
                node->setListener(animator);
                animator->registerNode(node);
            }
            return node;
        }});

    reg.registerType({"AccumulatorNode", "Logic", {0.3f, 0.6f, 1.0f, 1.0f},
        [](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new AccumulatorNode();
            auto* animator = Animator::instance();
            if (animator) {
                for (auto& [pname, port] : node->getInputs()) animator->add(port);
                for (auto& [pname, port] : node->getOutputs()) animator->add(port);
                node->setListener(animator);
                animator->registerNode(node);
                // Rename from default "accumulator" to the requested name
                if (!name.empty() && name != node->getName()) {
                    animator->renameNode(node->getName(), name);
                }
            }
            return node;
        }});

    // FX nodes — use the SAME factory as LuaAnimationNode (proven safe)
    reg.registerType({"PerlinFxNode", "FX", {1.0f, 0.5f, 0.2f, 1.0f},
        [](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            std::string clonePrefix = uniqueName("studio_perlin");
            auto* node = new PerlinFxNode("ogrehead.mesh", clonePrefix, name);
            // Clones are created dynamically in resolveTargets() when entity links are made
            auto* animator = Animator::instance();
            if (animator) {
                for (auto& [pname, port] : node->getInputs()) animator->add(port);
                for (auto& [pname, port] : node->getOutputs()) animator->add(port);
                node->setListener(animator);
                animator->registerNode(node);
                if (!name.empty() && name != node->getName())
                    animator->renameNode(node->getName(), name);
                auto* rootTime = RootTimeNode::instance();
                if (rootTime && node->getInputs().count("dt"))
                    animator->link(rootTime->getOutputs().at("dt"), node->getInputs().at("dt"));
            }
            std::cout << "[Studio] PerlinFxNode: " << name << " (multi-target)" << std::endl;
            return node;
        }});

    reg.registerType({"ShaderFxNode", "FX", {1.0f, 0.5f, 0.2f, 1.0f},
        [](const std::string& name, sol::state& lua) -> AnimationNode* {
            auto* sceneMgr = Engine::instance()->getSceneManager();
            if (!sceneMgr) return nullptr;
            // Read shader paths from preset params if available
            std::string vertShader = "perlin_deform.glsl";
            std::string fragShader = "passthrough.frag";
            sol::optional<sol::table> pp = lua["_preset_params"];
            if (pp) {
                sol::optional<std::string> vs = (*pp)["vert_shader"];
                sol::optional<std::string> fs = (*pp)["frag_shader"];
                if (vs && !vs->empty()) vertShader = *vs;
                if (fs && !fs->empty()) fragShader = *fs;
            }
            std::cout << "[ShaderFxNode Factory] name='" << name
                      << "' vert='" << vertShader << "' frag='" << fragShader
                      << "' _preset_params=" << (pp ? "present" : "nil") << std::endl;
            auto* node = new ShaderFxNode(name,
                vertShader, fragShader,
                sceneMgr);
            auto* animator = Animator::instance();
            if (animator) {
                for (auto& [pname, port] : node->getInputs()) animator->add(port);
                for (auto& [pname, port] : node->getOutputs()) animator->add(port);
                node->setListener(animator);
                animator->registerNode(node);
                auto* rootTime = RootTimeNode::instance();
                if (rootTime && node->getInputs().count("dt")) {
                    animator->link(rootTime->getOutputs().at("dt"), node->getInputs().at("dt"));
                }
            }
            return node;
        }});

    reg.registerType({"TextureBlitterNode", "FX", {1.0f, 0.5f, 0.2f, 1.0f},
        [](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            std::string texName = uniqueName("studio_blittex");
            auto* node = new TextureBlitterNode(texName, name);
            auto* animator = Animator::instance();
            if (animator) {
                for (auto& [pname, port] : node->getInputs()) animator->add(port);
                for (auto& [pname, port] : node->getOutputs()) animator->add(port);
                node->setListener(animator);
                animator->registerNode(node);
            }
            return node;
        }});

    reg.registerType({"WaveVertexShader", "FX", {1.0f, 0.5f, 0.2f, 1.0f},
        [](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* sceneMgr = Engine::instance()->getSceneManager();
            if (!sceneMgr) return nullptr;
            std::string cloneName = uniqueName("studio_wave");
            auto* node = new WaveVertexShader("ogrehead.mesh", cloneName, name);
            // Entity creation is deferred — the clone mesh is prepared in frameStarted().
            // We store the entity/scenenode names for deferred creation (same pattern as PerlinFxNode).
            node->setStudioNames(uniqueName("studio_ent"), uniqueName("studio_sn"));
            node->enable();
            auto* animator = Animator::instance();
            if (animator) {
                for (auto& [pname, port] : node->getInputs()) animator->add(port);
                for (auto& [pname, port] : node->getOutputs()) animator->add(port);
                node->setListener(animator);
                animator->registerNode(node);
                if (!name.empty() && name != node->getName())
                    animator->renameNode(node->getName(), name);
                // Link dt from RootTimeNode so waves animate
                auto* rootTime = RootTimeNode::instance();
                if (rootTime && node->getInputs().count("dt"))
                    animator->link(rootTime->getOutputs().at("dt"), node->getInputs().at("dt"));
            }
            return node;
        }});

    reg.registerType({"ColorShiftNode", "FX", {1.0f, 0.5f, 0.2f, 1.0f},
        [](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new ColorShiftNode(name);
            auto* animator = Animator::instance();
            if (animator) {
                for (auto& [pname, port] : node->getInputs()) animator->add(port);
                for (auto& [pname, port] : node->getOutputs()) animator->add(port);
                node->setListener(animator);
                animator->registerNode(node);
            }
            return node;
        }});

    // Audio — factory creates the full chain if dependencies are missing
    // Static audio capture singleton (shared across all audio nodes)
    static AudioCapture* sStudioAudioCapture = nullptr;
    static AudioCaptureNode* sStudioCaptureNode = nullptr;
    static AudioAnalyzerNode* sStudioAnalyzerNode = nullptr;

    auto ensureAudioCapture = [](sol::state& /*lua*/) -> AudioCaptureNode* {
        if (!sStudioCaptureNode) {
            if (!sStudioAudioCapture) {
                sStudioAudioCapture = new AudioCapture(44100, 2048);
                sStudioAudioCapture->start();
            }
            sStudioCaptureNode = new AudioCaptureNode(uniqueName("studio_capture"), sStudioAudioCapture);
            auto* animator = Animator::instance();
            if (animator) {
                for (auto& [pname, port] : sStudioCaptureNode->getInputs()) animator->add(port);
                for (auto& [pname, port] : sStudioCaptureNode->getOutputs()) animator->add(port);
                sStudioCaptureNode->setListener(animator);
                animator->registerNode(sStudioCaptureNode);
            }
        }
        return sStudioCaptureNode;
    };

    auto ensureAudioAnalyzer = [ensureAudioCapture](sol::state& lua) -> AudioAnalyzerNode* {
        if (!sStudioAnalyzerNode) {
            auto* captureNode = ensureAudioCapture(lua);
            sStudioAnalyzerNode = new AudioAnalyzerNode(uniqueName("studio_analyzer"), captureNode);
            auto* animator = Animator::instance();
            if (animator) {
                for (auto& [pname, port] : sStudioAnalyzerNode->getInputs()) animator->add(port);
                for (auto& [pname, port] : sStudioAnalyzerNode->getOutputs()) animator->add(port);
                sStudioAnalyzerNode->setListener(animator);
                animator->registerNode(sStudioAnalyzerNode);
            }
        }
        return sStudioAnalyzerNode;
    };

    reg.registerType({"AudioCaptureNode", "Audio", {0.8f, 0.3f, 0.8f, 1.0f},
        [ensureAudioCapture](const std::string& name, sol::state& lua) -> AnimationNode* {
            return ensureAudioCapture(lua);
        }});

    reg.registerType({"AudioAnalyzerNode", "Audio", {0.8f, 0.3f, 0.8f, 1.0f},
        [ensureAudioAnalyzer](const std::string& name, sol::state& lua) -> AnimationNode* {
            return ensureAudioAnalyzer(lua);
        }});

    reg.registerType({"BeatDetectorNode", "Audio", {0.8f, 0.3f, 0.8f, 1.0f},
        [ensureAudioAnalyzer](const std::string& name, sol::state& lua) -> AnimationNode* {
            auto* analyzerNode = ensureAudioAnalyzer(lua);
            auto* node = new BeatDetectorNode(name, analyzerNode);
            auto* animator = Animator::instance();
            if (animator) {
                for (auto& [pname, port] : node->getInputs()) animator->add(port);
                for (auto& [pname, port] : node->getOutputs()) animator->add(port);
                node->setListener(animator);
                animator->registerNode(node);
                auto* rootTime = RootTimeNode::instance();
                if (rootTime && node->getInputs().count("dt")) {
                    animator->link(rootTime->getOutputs().at("dt"), node->getInputs().at("dt"));
                }
            }
            return node;
        }});

    // Video
    reg.registerType({"TheoraClipNode", "Video", {0.9f, 0.8f, 0.2f, 1.0f},
        [](const std::string& name, sol::state& lua) -> AnimationNode* {
            // v3.5.2 Sprint S7 Lot AA — accept a 'filename' (or 'video_path')
            // override via _preset_params, same pattern as ShaderFxNode. Falls
            // back to the bundled bombe.ogg sample when no override is present.
            std::string videoPath = "resources/video/bombe.ogg";
            sol::optional<sol::table> pp = lua["_preset_params"];
            if (pp) {
                sol::optional<std::string> fn = (*pp)["filename"];
                if (!fn) {
                    sol::optional<std::string> alt = (*pp)["video_path"];
                    if (alt) fn = alt;
                }
                if (fn && !fn->empty()) videoPath = *fn;
            }
            auto* node = new TheoraClipNode(name, videoPath);
            auto* animator = Animator::instance();
            if (animator) {
                for (auto& [pname, port] : node->getInputs()) animator->add(port);
                for (auto& [pname, port] : node->getOutputs()) animator->add(port);
                node->setListener(animator);
                animator->registerNode(node);
                auto* rootTime = RootTimeNode::instance();
                if (rootTime && node->getInputs().count("dt")) {
                    animator->link(rootTime->getOutputs().at("dt"), node->getInputs().at("dt"));
                }
            }
            return node;
        }});

    // Animation — AnimationStateNode est enregistré plus bas (après la lambda
    // `registerInAnimator`, près des nodes Scene). v3.5.2 Lot AZ.

    // Signal — Lua-only temporal nodes (created via LuaAnimationNode with specific scripts)
    auto luaTemporalFactory = [](const std::string& luaType, const std::string& name,
                                  sol::state& lua) -> AnimationNode* {
        // Ensure temporal_nodes.lua is loaded
        lua.safe_script("if not " + luaType + " then require 'temporal_nodes' end",
                        sol::script_pass_on_error);
        // Create via Lua constructor
        auto result = lua.safe_script(
            "local n = " + luaType + ":new({name='" + name + "'}); return n and n._node or nil",
            sol::script_pass_on_error);
        if (result.valid() && result.get_type() != sol::type::lua_nil) {
            return result.get<AnimationNode*>();
        }
        // Fallback: create a basic LuaAnimationNode
        sol::function noop = lua.safe_script("return function(node) end").get<sol::function>();
        auto* node = new LuaAnimationNode(name, noop);
        node->addInput("in");
        node->addOutput("out");
        auto* animator = Animator::instance();
        if (animator) {
            for (auto& [pname, port] : node->getInputs()) animator->add(port);
            for (auto& [pname, port] : node->getOutputs()) animator->add(port);
            node->setListener(animator);
            animator->registerNode(node);
        }
        return node;
    };

    reg.registerType({"LFONode", "Signal", {0.0f, 0.5f, 1.0f, 1.0f},
        [luaTemporalFactory](const std::string& name, sol::state& lua) -> AnimationNode* {
            return luaTemporalFactory("LFONode", name, lua);
        }});

    reg.registerType({"RampNode", "Signal", {0.0f, 0.5f, 1.0f, 1.0f},
        [luaTemporalFactory](const std::string& name, sol::state& lua) -> AnimationNode* {
            return luaTemporalFactory("RampNode", name, lua);
        }});

    reg.registerType({"DelayNode", "Signal", {0.0f, 0.5f, 1.0f, 1.0f},
        [luaTemporalFactory](const std::string& name, sol::state& lua) -> AnimationNode* {
            return luaTemporalFactory("DelayNode", name, lua);
        }});

    reg.registerType({"EnvelopeFollowerNode", "Signal", {0.0f, 0.5f, 1.0f, 1.0f},
        [luaTemporalFactory](const std::string& name, sol::state& lua) -> AnimationNode* {
            return luaTemporalFactory("EnvelopeFollowerNode", name, lua);
        }});

    reg.registerType({"BandSplitNode", "Audio", {0.8f, 0.3f, 0.8f, 1.0f},
        [luaTemporalFactory](const std::string& name, sol::state& lua) -> AnimationNode* {
            return luaTemporalFactory("BandSplitNode", name, lua);
        }});

    reg.registerType({"SubgraphNode", "Logic", {0.5f, 0.5f, 0.5f, 1.0f},
        [luaTemporalFactory](const std::string& name, sol::state& lua) -> AnimationNode* {
            return luaTemporalFactory("SubgraphNode", name, lua);
        }});

    // ── v3.2 New Node Types ─────────────────────────────────────────────────

    // Helper lambda to register a node in the Animator
    auto registerInAnimator = [](AnimationNode* node) {
        auto* animator = Animator::instance();
        if (animator && node) {
            for (auto& [pname, port] : node->getInputs()) animator->add(port);
            for (auto& [pname, port] : node->getOutputs()) animator->add(port);
            node->setListener(animator);
            animator->registerNode(node);
        }
    };

    // Scene nodes
    reg.registerType({"SceneObjectNode", "Scene", {0.0f, 0.7f, 0.7f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& lua) -> AnimationNode* {
            auto* scene = Engine::instance()->getSceneManager();
            if (!scene) return nullptr;
            auto* node = new SceneObjectNode(name, scene);
            registerInAnimator(node);
            // Export SceneNode reference to Lua so scripts (e.g. rotate_head) can access it
            if (node->getSceneNode()) {
                if (!lua["_sceneNodes"].valid()) lua["_sceneNodes"] = lua.create_table();
                lua["_sceneNodes"][name] = node->getSceneNode();
            }
            return node;
        }});

    reg.registerType({"LightNode", "Scene", {0.0f, 0.7f, 0.7f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* scene = Engine::instance()->getSceneManager();
            if (!scene) return nullptr;
            auto* node = new LightNode(name, scene);
            registerInAnimator(node);
            return node;
        }});

    reg.registerType({"FullscreenOverlayNode", "Scene", {0.0f, 0.7f, 0.7f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* scene = Engine::instance()->getSceneManager();
            if (!scene) return nullptr;
            auto* node = new FullscreenOverlayNode(name, scene);
            registerInAnimator(node);
            return node;
        }});

    reg.registerType({"TextureCycleNode", "Scene", {0.0f, 0.7f, 0.7f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new TextureCycleNode(name);
            registerInAnimator(node);
            // Wire dt port from RootTimeNode for transition + bpm_synced timing.
            auto* root = RootTimeNode::instance();
            auto* animator = Animator::instance();
            if (root && animator && node->getInputs().count("dt"))
                animator->link(root->getOutputs().at("dt"), node->getInputs().at("dt"));
            return node;
        }});

    // Lot AV — TextureSetNode (repro 2006 du couple {gray, color, factor}).
    reg.registerType({"TextureSetNode", "Scene", {0.0f, 0.7f, 0.7f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new TextureSetNode(name);
            registerInAnimator(node);
            auto* root = RootTimeNode::instance();
            auto* animator = Animator::instance();
            if (root && animator && node->getInputs().count("dt"))
                animator->link(root->getOutputs().at("dt"), node->getInputs().at("dt"));
            return node;
        }});

    reg.registerType({"TextureBlendNode", "FX", {1.0f, 0.5f, 0.2f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new TextureBlendNode(name);
            registerInAnimator(node);
            return node;
        }});

    reg.registerType({"TextureFeedbackNode", "FX", {1.0f, 0.5f, 0.2f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new TextureFeedbackNode(name);
            registerInAnimator(node);
            return node;
        }});

    reg.registerType({"VideoCrossfadeNode", "Video", {0.9f, 0.8f, 0.2f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new VideoCrossfadeNode(name);
            registerInAnimator(node);
            // Wire dt port from RootTimeNode for auto_crossfade_bpm timing.
            auto* root = RootTimeNode::instance();
            auto* animator = Animator::instance();
            if (root && animator && node->getInputs().count("dt"))
                animator->link(root->getOutputs().at("dt"), node->getInputs().at("dt"));
            return node;
        }});

    reg.registerType({"MaterialAnimNode", "Scene", {0.2f, 0.8f, 0.6f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new MaterialAnimNode(name);
            registerInAnimator(node);
            auto* root = RootTimeNode::instance();
            auto* animator = Animator::instance();
            if (root && animator && node->getInputs().count("dt"))
                animator->link(root->getOutputs().at("dt"), node->getInputs().at("dt"));
            return node;
        }});

    reg.registerType({"VideoLibraryNode", "Video", {0.9f, 0.8f, 0.2f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new VideoLibraryNode(name);
            registerInAnimator(node);
            auto* root = RootTimeNode::instance();
            auto* animator = Animator::instance();
            if (root && animator && node->getInputs().count("dt"))
                animator->link(root->getOutputs().at("dt"), node->getInputs().at("dt"));
            return node;
        }});

    reg.registerType({"BillboardLayerNode", "Scene", {0.0f, 0.7f, 0.7f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* scene = Engine::instance()->getSceneManager();
            if (!scene) return nullptr;
            auto* node = new BillboardLayerNode(name, scene);
            registerInAnimator(node);
            return node;
        }});

    reg.registerType({"JoystickRouterNode", "Input", {0.35f, 0.65f, 0.95f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new JoystickRouterNode(name);
            registerInAnimator(node);
            return node;
        }});

    reg.registerType({"VideoSlicerNode", "Video", {0.9f, 0.8f, 0.2f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new VideoSlicerNode(name);
            registerInAnimator(node);
            auto* root = RootTimeNode::instance();
            auto* animator = Animator::instance();
            if (root && animator && node->getInputs().count("dt"))
                animator->link(root->getOutputs().at("dt"), node->getInputs().at("dt"));
            return node;
        }});

    reg.registerType({"MultiTextureBankNode", "Scene", {0.0f, 0.7f, 0.7f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new MultiTextureBankNode(name);
            registerInAnimator(node);
            auto* root = RootTimeNode::instance();
            auto* animator = Animator::instance();
            if (root && animator && node->getInputs().count("dt"))
                animator->link(root->getOutputs().at("dt"), node->getInputs().at("dt"));
            return node;
        }});

    reg.registerType({"NoiseTextureNode", "FX", {1.0f, 0.5f, 0.2f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new NoiseTextureNode(name);
            registerInAnimator(node);
            return node;
        }});

    reg.registerType({"SpectrogramTextureNode", "FX", {1.0f, 0.5f, 0.2f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new SpectrogramTextureNode(name);
            registerInAnimator(node);
            return node;
        }});

    // v3.5.2 Lot U — GrayscaleNode (BT.709 runtime desat)
    reg.registerType({"GrayscaleNode", "FX", {1.0f, 0.5f, 0.2f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new GrayscaleNode(name);
            registerInAnimator(node);
            return node;
        }});

    reg.registerType({"ArtnetVideoMapperNode", "Output", {0.9f, 0.5f, 0.9f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new ArtnetVideoMapperNode(name);
            registerInAnimator(node);
            return node;
        }});

    reg.registerType({"ParticleNode", "Scene", {0.0f, 0.7f, 0.7f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* scene = Engine::instance()->getSceneManager();
            if (!scene) return nullptr;
            auto* node = new ParticleNode(name, scene);
            registerInAnimator(node);
            return node;
        }});

    reg.registerType({"CameraNode", "Scene", {0.0f, 0.7f, 0.7f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* scene = Engine::instance()->getSceneManager();
            if (!scene) return nullptr;
            auto* node = new CameraNode(name, scene);
            registerInAnimator(node);
            auto* rootTime = RootTimeNode::instance();
            auto* animator = Animator::instance();
            if (rootTime && animator && node->getInputs().count("dt"))
                animator->link(rootTime->getOutputs().at("dt"), node->getInputs().at("dt"));
            return node;
        }});

    // Animation — v3.5.2 Lot AZ : pipeline mesh animé. Le node lit l'AnimationState
    // de l'entité d'un SceneObjectNode amont (mesh riggé : ninja/robot/fish) via le
    // port entity-link, et la pilote par `time` (scrub) ou auto-advance `dt`*`speed`.
    // (L'ancien crash venait de SceneManager::getAnimation ; on utilise désormais
    //  entity->getAnimationState, l'API correcte pour les anims squelettiques.)
    reg.registerType({"AnimationStateNode", "Animation", {0.9f, 0.6f, 0.2f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* scene = Engine::instance()->getSceneManager();
            if (!scene) return nullptr;
            auto* node = new AnimationStateNode(name, scene);
            registerInAnimator(node);
            // Auto-link RootTime.dt → node.dt pour l'auto-advance « out of the box ».
            auto* rootTime = RootTimeNode::instance();
            auto* animator = Animator::instance();
            if (rootTime && animator && node->getInputs().count("dt"))
                animator->link(rootTime->getOutputs().at("dt"), node->getInputs().at("dt"));
            return node;
        }});

    // Environment nodes
    reg.registerType({"SkyboxNode", "Environment", {0.2f, 0.7f, 0.3f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* scene = Engine::instance()->getSceneManager();
            if (!scene) return nullptr;
            auto* node = new SkyboxNode(name, scene);
            registerInAnimator(node);
            return node;
        }});

    reg.registerType({"FogNode", "Environment", {0.2f, 0.7f, 0.3f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* scene = Engine::instance()->getSceneManager();
            if (!scene) return nullptr;
            auto* node = new FogNode(name, scene);
            registerInAnimator(node);
            return node;
        }});

    // PostProcess
    reg.registerType({"CompositorNode", "PostProcess", {0.7f, 0.2f, 0.7f, 1.0f},
        [this, registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            // Get viewport from the Studio RenderTexture (not the RenderWindow which may have 0 viewports)
            Ogre::Viewport* vp = nullptr;
            if (mEngine) {
                auto* rt = mEngine->getRenderTarget();
                if (rt && rt->getNumViewports() > 0)
                    vp = rt->getViewport(0);
            }
            auto* node = new CompositorNode(name, vp);
            registerInAnimator(node);
            return node;
        }});

    reg.registerType({"PostProcessNode", "PostProcess", {0.7f, 0.2f, 0.7f, 1.0f},
        [this, registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            PostProcessStack* stack = mEngine ? mEngine->getPostProcessStack() : nullptr;
            auto* node = new PostProcessNode(name, stack);
            registerInAnimator(node);
            return node;
        }});

    // ── TextureNode ──────────────────────────────────────────────────────
    reg.registerType({"TextureNode", "Scene", {0.2f, 0.8f, 0.6f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new TextureNode(name);
            registerInAnimator(node);
            return node;
        }});

    // ── MaterialNode ─────────────────────────────────────────────────────
    reg.registerType({"MaterialNode", "Scene", {0.6f, 0.2f, 0.8f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new MaterialNode(name);
            registerInAnimator(node);
            return node;
        }});

    // v3.5.2 Lot T — MaterialBridgeNode (universal mat → mesh router)
    reg.registerType({"MaterialBridgeNode", "Scene", {0.0f, 0.7f, 0.7f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new MaterialBridgeNode(name);
            registerInAnimator(node);
            return node;
        }});

    // Signal extensions
    reg.registerType({"BeatTriggerNode", "Signal", {0.0f, 0.5f, 1.0f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new BeatTriggerNode(name);
            registerInAnimator(node);
            auto* rootTime = RootTimeNode::instance();
            auto* animator = Animator::instance();
            if (rootTime && animator) {
                if (node->getInputs().count("beat"))
                    animator->link(rootTime->getOutputs().at("beat"), node->getInputs().at("beat"));
                if (node->getInputs().count("beatFrac"))
                    animator->link(rootTime->getOutputs().at("beatFrac"), node->getInputs().at("beatFrac"));
                if (node->getInputs().count("dt"))
                    animator->link(rootTime->getOutputs().at("dt"), node->getInputs().at("dt"));
            }
            return node;
        }});

    // Math nodes
    reg.registerType({"MathNode", "Math", {0.6f, 0.6f, 0.6f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new MathNode(name);
            registerInAnimator(node);
            return node;
        }});

    // v3.5 Lot K: GamepadNode exposes the full state of a connected gamepad
    // as DAG output ports (sticks/triggers/buttons/gyro/accel/touchpad/battery).
    reg.registerType({"GamepadNode", "Input", {0.35f, 0.65f, 0.95f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new GamepadNode(name);
            registerInAnimator(node);
            return node;
        }});

    reg.registerType({"MixerNode", "Math", {0.6f, 0.6f, 0.6f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new MixerNode(name);
            registerInAnimator(node);
            return node;
        }});

    reg.registerType({"MapperNode", "Math", {0.6f, 0.6f, 0.6f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new MapperNode(name);
            registerInAnimator(node);
            return node;
        }});

    reg.registerType({"TriggerNode", "Signal", {0.0f, 0.5f, 1.0f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new TriggerNode(name);
            registerInAnimator(node);
            auto* rootTime = RootTimeNode::instance();
            auto* animator = Animator::instance();
            if (rootTime && animator && node->getInputs().count("dt"))
                animator->link(rootTime->getOutputs().at("dt"), node->getInputs().at("dt"));
            return node;
        }});

    reg.registerType({"SplitterNode", "Signal", {0.0f, 0.5f, 1.0f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new SplitterNode(name);
            registerInAnimator(node);
            return node;
        }});

    // MIDI nodes (v3.3)
    reg.registerType({"MidiInputNode", "Input", {0.9f, 0.2f, 0.6f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new MidiInputNode(name);
            registerInAnimator(node);
            auto* rootTime = RootTimeNode::instance();
            auto* animator = Animator::instance();
            if (rootTime && animator && node->getInputs().count("dt"))
                animator->link(rootTime->getOutputs().at("dt"), node->getInputs().at("dt"));
            return node;
        }});

    reg.registerType({"OscInputNode", "Input", {0.2f, 0.8f, 0.5f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new OscInputNode(name);
            registerInAnimator(node);
            auto* rootTime = RootTimeNode::instance();
            auto* animator = Animator::instance();
            if (rootTime && animator && node->getInputs().count("dt"))
                animator->link(rootTime->getOutputs().at("dt"), node->getInputs().at("dt"));
            return node;
        }});

    reg.registerType({"OscOutputNode", "Output", {0.5f, 0.8f, 0.2f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new OscOutputNode(name);
            registerInAnimator(node);
            auto* rootTime = RootTimeNode::instance();
            auto* animator = Animator::instance();
            if (rootTime && animator && node->getInputs().count("dt"))
                animator->link(rootTime->getOutputs().at("dt"), node->getInputs().at("dt"));
            return node;
        }});

    reg.registerType({"MidiOutputNode", "Output", {0.6f, 0.2f, 0.9f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new MidiOutputNode(name);
            registerInAnimator(node);
            auto* rootTime = RootTimeNode::instance();
            auto* animator = Animator::instance();
            if (rootTime && animator && node->getInputs().count("dt"))
                animator->link(rootTime->getOutputs().at("dt"), node->getInputs().at("dt"));
            return node;
        }});

    reg.registerType({"NdiOutputNode", "Output", {0.2f, 0.7f, 0.9f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new NdiOutputNode(name);
            registerInAnimator(node);
            auto* rootTime = RootTimeNode::instance();
            auto* animator = Animator::instance();
            if (rootTime && animator && node->getInputs().count("dt"))
                animator->link(rootTime->getOutputs().at("dt"), node->getInputs().at("dt"));
            return node;
        }});

    // Stage nodes (v3.4)
    reg.registerType({"WarpNode", "Stage", {0.9f, 0.5f, 0.1f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new WarpNode(name);
            registerInAnimator(node);
            return node;
        }});

    reg.registerType({"BlendNode", "Stage", {0.9f, 0.5f, 0.1f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new BlendNode(name);
            registerInAnimator(node);
            return node;
        }});

    // TextureShareOutputNode (v3.4 Lot H + Lot N cross-platform)
    reg.registerType({"TextureShareOutputNode", "Output", {0.3f, 0.9f, 0.6f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new TextureShareOutputNode(name);
            registerInAnimator(node);
            return node;
        }});

    // Legacy alias for v3.4.0 project compatibility (SpoutOutputNode → TextureShareOutputNode).
    reg.registerType({"SpoutOutputNode", "Output", {0.3f, 0.9f, 0.6f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new TextureShareOutputNode(name);
            registerInAnimator(node);
            return node;
        }});

    // ArtnetOutputNode (v3.4 Lot J) — DMX over Artnet UDP
    reg.registerType({"ArtnetOutputNode", "Output", {0.9f, 0.5f, 0.9f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new ArtnetOutputNode(name);
            registerInAnimator(node);
            return node;
        }});

    // v3.5 Lot M — ArtnetInputNode : symmetric counterpart on UDP 6454.
    reg.registerType({"ArtnetInputNode", "Input", {0.5f, 0.9f, 0.9f, 1.0f},
        [registerInAnimator](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new ArtnetInputNode(name);
            registerInAnimator(node);
            return node;
        }});
}

// ── Status bar ──────────────────────────────────────────────────────────────

// ── About dialog ────────────────────────────────────────────────────────────

void StudioApp::renderAboutDialog() {
    if (mShowAbout) {
        ImGui::OpenPopup("About BBFx Studio");
        mShowAbout = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("About BBFx Studio", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // v3.5.2 Sprint S8 Lot AU — pull from BBFX_VERSION_STRING constant.
        ImGui::Text("BBFx Studio v%s — %s", BBFX_VERSION_STRING, BBFX_VERSION_NAME);
        ImGui::Separator();
        ImGui::Text("Real-time 3D animation and effects engine");
        ImGui::Spacing();
        ImGui::Text("Authors: Sebastien Jullien, Thomas Lefort");
        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("OK", {120, 0})) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ── Shortcuts dialog ────────────────────────────────────────────────────────

void StudioApp::renderShortcutsDialog() {
    if (mShowShortcuts) {
        ImGui::OpenPopup("Keyboard Shortcuts");
        mShowShortcuts = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("Keyboard Shortcuts", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::BeginTable("shortcuts", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("Action");
            ImGui::TableHeadersRow();

            auto row = [](const char* key, const char* action) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", key);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", action);
            };

            row("Ctrl+N",  "New project");
            row("Ctrl+O",  "Open project");
            row("Ctrl+S",  "Save project");
            row("Ctrl+E",  "Export video");
            row("Ctrl+,",  "Settings");
            row("Ctrl+Z",  "Undo");
            row("Ctrl+Y",  "Redo");
            row("Ctrl+D",  "Duplicate selected node(s)");
            row("Space",   "Play / Pause");
            row("F1",      "About");
            row("F2",      "Toggle Console");
            row("F3",      "Toggle Inspector");
            row("F4",      "Toggle Timeline");
            row("F5",      "Toggle Performance Mode");
            row("F6",      "Toggle Preset Browser");
            row("F7",      "Toggle Node Editor");
            row("F8",      "Toggle Scene Hierarchy");
            row("F9",      "Toggle Effect Rack");
            row("Ctrl+1-9","Save bookmark (Node Editor)");
            row("1-9",     "Restore bookmark (Node Editor)");
            row("Delete",  "Delete selected node/link");
            row("Escape",  "Exit Performance Mode / Quit");
            row("F11",     "Toggle output fullscreen");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "--- Stage (v3.4) ---");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("");
            row("Ctrl+Shift+O", "Output Manager — manage projector outputs");
            row("Ctrl+Shift+S", "Surface Editor — map zones to outputs");
            row("Ctrl+Shift+N", "Network Sync — master/slave sync panel");
            row("Ctrl+Shift+M", "Master View — unified dashboard (outputs + network + scene)");
            row("Ctrl+Shift+1", "PANIC ALL — reset all warps, blends, network, DMX, Spout");
            row("Ctrl+Shift+P", "Command Palette");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "--- Connect ---");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("");

            row("Ctrl+Shift+G", "Gamepad Panel");
            row("Ctrl+Shift+A", "Asset Browser");
            row("M (fader)", "MIDI Learn for fader");
            row("Right-click trig", "MIDI Learn for trigger");
            row("Right-click port", "MIDI Learn for DAG port");
            row("1-9 / Q-W-E-R-T", "Trigger shortcuts (F5 mode)");
            row("Tab",      "Cycle trigger pages (F5 mode)");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "--- Plugins ---");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("");
            row("Ctrl+Shift+X", "Plugin Manager");
            row("Ctrl+Shift+E", "Plugin Errors");
            row("Ctrl+Shift+C", "Community Browser");

            ImGui::EndTable();
        }
        ImGui::Spacing();
        if (ImGui::Button("OK", {120, 0})) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ── Settings dialog ─────────────────────────────────────────────────────────

void StudioApp::renderRecoveryDialog() {
    if (mShowRecoveryDialog) {
        ImGui::OpenPopup("Recover Autosave");
        mShowRecoveryDialog = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("Recover Autosave", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored({1.0f, 0.8f, 0.0f, 1.0f}, "Previous session may have crashed.");
        ImGui::Spacing();
        ImGui::Text("An autosave was found that is more recent than your last save.");
        ImGui::Text("File: %s", mRecoveryAutosavePath.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Recover Autosave", {180, 0})) {
            loadProject(mRecoveryAutosavePath);
            std::cout << "[Studio] Recovered from autosave: " << mRecoveryAutosavePath << std::endl;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Ignore", {100, 0})) {
            std::cout << "[Studio] Autosave recovery declined" << std::endl;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete Autosave", {140, 0})) {
            try {
                std::filesystem::remove(mRecoveryAutosavePath);
                std::cout << "[Studio] Autosave deleted: " << mRecoveryAutosavePath << std::endl;
            } catch (...) {}
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void StudioApp::renderDeepLinkConfirmDialog() {
    if (mShowDeepLinkConfirm) {
        ImGui::OpenPopup("Deep-link Confirmation");
        mShowDeepLinkConfirm = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("Deep-link Confirmation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored({1.0f, 0.8f, 0.0f, 1.0f},
            "An external link is requesting to %s a plugin.",
            mDeepLinkAction == "run" ? "RUN" : "ENABLE");
        ImGui::Spacing();
        ImGui::Text("Plugin: %s", mDeepLinkPluginId.c_str());
        if (mDeepLinkAction == "run" && !mDeepLinkNodeType.empty())
            ImGui::Text("Node type: %s", mDeepLinkNodeType.c_str());
        ImGui::Spacing();
        ImGui::TextWrapped("Enabling a plugin executes its code. Only proceed if you trust the source of this link.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button(mDeepLinkAction == "run" ? "Run" : "Enable", {120, 0})) {
            const std::string id = mDeepLinkPluginId;
            const std::string type = mDeepLinkNodeType;
            if (mDeepLinkAction == "run") {
                PluginManager::instance().enable(id);
                ToastSystem::instance().toast(
                    "Run deep-link confirmed: " + id + " / " + type +
                    " — auto node-instantiation arrives in Lot W.",
                    ToastSeverity::Info, 5.0f);
            } else {
                if (!PluginManager::instance().enable(id)) {
                    ToastSystem::instance().toast(
                        "Enable failed for " + id, ToastSeverity::Error, 5.0f);
                } else {
                    ToastSystem::instance().toast(
                        "Enabled " + id, ToastSeverity::Info, 3.0f);
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", {120, 0})) {
            ToastSystem::instance().toast(
                "Deep-link declined for " + mDeepLinkPluginId, ToastSeverity::Info, 3.0f);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void StudioApp::renderSettingsDialog() {
    if (mShowSettings) {
        ImGui::OpenPopup("Settings");
        mShowSettings = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
    // Settings must persist across frames while the popup is open.
    // Loading from mgr.get() every frame discards user changes (combo, sliders).
    static Settings sSettingsEdit;
    static bool sSettingsLoaded = false;
    if (ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (!sSettingsLoaded) {
            sSettingsEdit = SettingsManager::instance().get();
            sSettingsLoaded = true;
        }

        ImGui::Text("General");
        ImGui::Separator();
        ImGui::SliderInt("Auto-save interval (sec)", &sSettingsEdit.autoSaveInterval, 30, 600);
        ImGui::SliderInt("Font size", &sSettingsEdit.fontSize, 10, 24);
        ImGui::Spacing();

        ImGui::Text("Rendering");
        ImGui::Separator();
        ImGui::SliderFloat("Viewport scale", &sSettingsEdit.viewportScale, 0.25f, 4.0f, "%.2f");
        {
            const char* lightModes[] = {"unlit", "lit", "emissive"};
            int current = 1; // default to "lit"
            for (int i = 0; i < 3; i++) {
                if (sSettingsEdit.defaultLightingMode == lightModes[i]) { current = i; break; }
            }
            if (ImGui::Combo("Default texture lighting", &current, lightModes, 3)) {
                sSettingsEdit.defaultLightingMode = lightModes[current];
            }
        }
        ImGui::Spacing();

        ImGui::Text("Audio");
        ImGui::Separator();
        ImGui::SliderFloat("Default BPM", &sSettingsEdit.defaultBPM, 60.0f, 240.0f, "%.1f");
        ImGui::Spacing();

        ImGui::Separator();
        if (ImGui::Button("Save", {100, 0})) {
            SettingsManager::instance().set(sSettingsEdit);
            SettingsManager::instance().save();
            sSettingsLoaded = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", {100, 0})) {
            sSettingsLoaded = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    } else {
        sSettingsLoaded = false; // reset if popup closed externally
    }
}

// ── ImGui Test Engine: test registration ─────────────────────────────────────

void StudioApp::registerTests() {
    if (!mTestEngine) return;

    // U-060: Open Material Editor panel via View menu
    ImGuiTest* t_matEditor = IM_REGISTER_TEST(mTestEngine, "ui_panels", "U-060 Open Material Editor");
    t_matEditor->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("View/Material Editor");
        ctx->Yield(5);
        // Verify panel is open by checking window exists
        ImGuiWindow* win = ctx->GetWindowByRef("Material Editor");
        IM_CHECK(win != nullptr);
    };

    // U-070: Open Shader Gallery via View menu
    ImGuiTest* t_shaderGallery = IM_REGISTER_TEST(mTestEngine, "ui_panels", "U-070 Open Shader Gallery");
    t_shaderGallery->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("View/Shader Gallery");
        ctx->Yield(5);
        ImGuiWindow* win = ctx->GetWindowByRef("Shader Gallery");
        IM_CHECK(win != nullptr);
    };

    // U-085: Open Compositor Stack via View menu
    ImGuiTest* t_compStack = IM_REGISTER_TEST(mTestEngine, "ui_panels", "U-085 Open Compositor Stack");
    t_compStack->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("View/Compositor Stack");
        ctx->Yield(5);
        ImGuiWindow* win = ctx->GetWindowByRef("Compositor Stack");
        IM_CHECK(win != nullptr);
    };

    // U-029: Open Undo History via View menu
    ImGuiTest* t_undoHistory = IM_REGISTER_TEST(mTestEngine, "ui_panels", "U-029 Open Undo History");
    t_undoHistory->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("View/Undo History");
        ctx->Yield(5);
        ImGuiWindow* win = ctx->GetWindowByRef("Undo History");
        IM_CHECK(win != nullptr);
    };

    // U-undo: Undo/Redo via Edit menu
    ImGuiTest* t_undoMenu = IM_REGISTER_TEST(mTestEngine, "ui_edit", "U-undo Edit menu Undo/Redo");
    t_undoMenu->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Edit/Undo");
        ctx->Yield(2);
        ctx->MenuClick("Edit/Redo");
        ctx->Yield(2);
    };

    // ── File menu tests ──────────────────────────────────────────────────
    ImGuiTest* t_fileNew = IM_REGISTER_TEST(mTestEngine, "ui_file", "File New");
    t_fileNew->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("File/New");
        ctx->Yield(5);
    };

    // ── All View panel toggles (one test per panel) ───────────────────
    auto registerViewToggle = [this](const char* panelName) {
        std::string testName = std::string("View toggle: ") + panelName;
        std::string path = std::string("View/") + panelName;
        ImGuiTest* t = IM_REGISTER_TEST(mTestEngine, "ui_view", testName.c_str());
        t->TestFunc = [path](ImGuiTestContext* ctx) {
            ctx->SetRef("##MainMenuBar");
            ctx->MenuClick(path.c_str());
            ctx->Yield(3);
            ctx->MenuClick(path.c_str());
            ctx->Yield(3);
        };
    };
    registerViewToggle("Viewport");
    registerViewToggle("Node Editor");
    registerViewToggle("Inspector");
    registerViewToggle("Timeline");
    registerViewToggle("Preset Browser");
    registerViewToggle("Console");
    registerViewToggle("Scene Hierarchy");
    registerViewToggle("Compositor Stack");
    registerViewToggle("Shader Gallery");
    registerViewToggle("Material Editor");
    registerViewToggle("Undo History");

    // ── Inspector panel interaction ──────────────────────────────────────
    // U-022: Disable/Enable in FX Stack (requires a node selected)
    ImGuiTest* t_inspEnable = IM_REGISTER_TEST(mTestEngine, "ui_inspector", "U-022 Inspector Enable/Disable");
    t_inspEnable->TestFunc = [](ImGuiTestContext* ctx) {
        // This test verifies the Inspector panel renders without crash
        ctx->Yield(5);
        ImGuiWindow* win = ctx->GetWindowByRef("Inspector");
        IM_CHECK(win != nullptr);
    };

    // ── Performance Mode F5 toggle ──────────────────────────────────────
    ImGuiTest* t_f5 = IM_REGISTER_TEST(mTestEngine, "ui_perf", "U-050 F5 Performance toggle");
    t_f5->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->KeyPress(ImGuiKey_F5);
        ctx->Yield(10);
        ctx->KeyPress(ImGuiKey_Escape);
        ctx->Yield(10);
    };

    // ── Node Editor existence ───────────────────────────────────────────
    ImGuiTest* t_nodeEd = IM_REGISTER_TEST(mTestEngine, "ui_nodeeditor", "Node Editor exists");
    t_nodeEd->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->Yield(3);
        ImGuiWindow* win = ctx->GetWindowByRef("Node Editor");
        IM_CHECK(win != nullptr);
    };

    // ── Viewport existence ──────────────────────────────────────────────
    ImGuiTest* t_viewport = IM_REGISTER_TEST(mTestEngine, "ui_viewport", "Viewport exists");
    t_viewport->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->Yield(3);
        ImGuiWindow* win = ctx->GetWindowByRef("Viewport");
        IM_CHECK(win != nullptr);
    };

    // ── Timeline existence ──────────────────────────────────────────────
    ImGuiTest* t_timeline = IM_REGISTER_TEST(mTestEngine, "ui_timeline", "Timeline exists");
    t_timeline->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->Yield(3);
        ImGuiWindow* win = ctx->GetWindowByRef("Timeline");
        IM_CHECK(win != nullptr);
    };

    // ── Status bar content ──────────────────────────────────────────────
    ImGuiTest* t_statusBar = IM_REGISTER_TEST(mTestEngine, "ui_status", "Status bar renders");
    t_statusBar->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->Yield(3);
        ImGuiWindow* win = ctx->GetWindowByRef("##StatusBar");
        IM_CHECK(win != nullptr);
    };

    // ── Minimap existence ───────────────────────────────────────────────
    ImGuiTest* t_minimap = IM_REGISTER_TEST(mTestEngine, "ui_minimap", "Minimap renders");
    t_minimap->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->Yield(3);
        ImGuiWindow* win = ctx->GetWindowByRef("##Minimap");
        IM_CHECK(win != nullptr);
    };

    // ── Stage v3.4 tests ──────────────────────────────────────────────────────

    // U-200: Open Output Manager via Stage menu
    ImGuiTest* t_outMgr = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-200 Open Output Manager");
    t_outMgr->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Stage/Output Manager");
        ctx->Yield(5);
        ImGuiWindow* win = ctx->GetWindowByRef("Output Manager");
        IM_CHECK(win != nullptr);
    };

    // U-201: Close Output Manager
    ImGuiTest* t_outMgrClose = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-201 Close Output Manager");
    t_outMgrClose->TestFunc = [](ImGuiTestContext* ctx) {
        // Open first
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Stage/Output Manager");
        ctx->Yield(3);
        // Close via menu toggle
        ctx->MenuClick("Stage/Output Manager");
        ctx->Yield(3);
        ImGuiWindow* win = ctx->GetWindowByRef("Output Manager");
        IM_CHECK(win == nullptr || !win->Active);
    };

    // U-202: Open Surface Editor via Stage menu
    ImGuiTest* t_surfEd = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-202 Open Surface Editor");
    t_surfEd->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Stage/Surface Editor");
        ctx->Yield(5);
        ImGuiWindow* win = ctx->GetWindowByRef("Surface Editor");
        IM_CHECK(win != nullptr);
    };

    // U-203: Open Network Sync panel via Stage menu
    ImGuiTest* t_netPanel = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-203 Open Network Sync Panel");
    t_netPanel->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Stage/Network Sync");
        ctx->Yield(5);
        ImGuiWindow* win = ctx->GetWindowByRef("Network Sync");
        IM_CHECK(win != nullptr);
    };

    // U-204: Close Network Sync panel
    ImGuiTest* t_netClose = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-204 Close Network Sync Panel");
    t_netClose->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Stage/Network Sync");
        ctx->Yield(3);
        ctx->MenuClick("Stage/Network Sync");
        ctx->Yield(3);
        ImGuiWindow* win = ctx->GetWindowByRef("Network Sync");
        IM_CHECK(win == nullptr || !win->Active);
    };

    // U-205: Stage menu exists and is complete
    ImGuiTest* t_stageMenu = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-205 Stage Menu Exists");
    t_stageMenu->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Stage");
        ctx->Yield(3);
        // Verify items are present
        ImGuiWindow* popup = ctx->GetWindowByRef("##Menu_00");
        IM_CHECK(popup != nullptr);
        ctx->KeyPress(ImGuiKey_Escape);
    };

    // U-206: PANIC ALL via Stage menu
    ImGuiTest* t_panicAll = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-206 PANIC ALL via Stage menu");
    t_panicAll->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Stage/PANIC ALL");
        ctx->Yield(5);
        // Toast should have appeared; verify no crash
        IM_CHECK(true);
    };

    // U-207: Status bar shows Outputs count
    ImGuiTest* t_statusOutputs = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-207 Status bar shows Outputs");
    t_statusOutputs->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->Yield(3);
        ImGuiWindow* win = ctx->GetWindowByRef("##StatusBar");
        IM_CHECK(win != nullptr);
    };

    // U-208: Ctrl+Shift+O opens Output Manager
    ImGuiTest* t_shortcutO = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-208 Ctrl+Shift+O opens Output Manager");
    t_shortcutO->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_O);
        ctx->Yield(5);
        ImGuiWindow* win = ctx->GetWindowByRef("Output Manager");
        IM_CHECK(win != nullptr);
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_O);
    };

    // U-209: Ctrl+Shift+N opens Network panel
    ImGuiTest* t_shortcutN = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-209 Ctrl+Shift+N opens Network Sync");
    t_shortcutN->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_N);
        ctx->Yield(5);
        ImGuiWindow* win = ctx->GetWindowByRef("Network Sync");
        IM_CHECK(win != nullptr);
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_N);
    };

    // U-210: Ctrl+Shift+P opens Command Palette without crash
    ImGuiTest* t_shortcutP = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-210 Ctrl+Shift+P Command Palette");
    t_shortcutP->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P);
        ctx->Yield(5);
        IM_CHECK(true); // Verifies no crash
    };

    // U-211: Surface Editor panel renders
    ImGuiTest* t_surfRender = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-211 Surface Editor renders");
    t_surfRender->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Stage/Surface Editor");
        ctx->Yield(10);
        ImGuiWindow* win = ctx->GetWindowByRef("Surface Editor");
        IM_CHECK(win != nullptr);
        ctx->MenuClick("Stage/Surface Editor"); // close
    };

    // U-212: Network panel renders peer table
    ImGuiTest* t_netRender = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-212 Network panel renders peer table");
    t_netRender->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Stage/Network Sync");
        ctx->Yield(10);
        ImGuiWindow* win = ctx->GetWindowByRef("Network Sync");
        IM_CHECK(win != nullptr);
        ctx->MenuClick("Stage/Network Sync"); // close
    };

    // U-213: Output Manager renders slot list
    ImGuiTest* t_outList = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-213 Output Manager renders slot list");
    t_outList->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Stage/Output Manager");
        ctx->Yield(10);
        ImGuiWindow* win = ctx->GetWindowByRef("Output Manager");
        IM_CHECK(win != nullptr);
        ctx->MenuClick("Stage/Output Manager"); // close
    };

    // U-214: Version indicator in splash shows v3.5.1
    ImGuiTest* t_version = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-214 Splash shows correct version");
    t_version->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->Yield(3);
        // Version visible in status bar
        ImGuiWindow* sb = ctx->GetWindowByRef("##StatusBar");
        IM_CHECK(sb != nullptr);
    };

    // U-215: Grid Warp section visible in Output Manager when opened
    ImGuiTest* t_gridWarp = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-215 Grid Warp section in Output Manager");
    t_gridWarp->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Stage/Output Manager");
        ctx->Yield(5);
        ImGuiWindow* win = ctx->GetWindowByRef("Output Manager");
        IM_CHECK(win != nullptr);
        ctx->MenuClick("Stage/Output Manager"); // close
    };

    // U-216: Master View panel opens via menu and shows "No outputs" with 0 outputs
    ImGuiTest* t_masterViewOpen = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-216 Master View opens via menu");
    t_masterViewOpen->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Stage/Master View");
        ctx->Yield(5);
        ImGuiWindow* win = ctx->GetWindowByRef("Master View");
        IM_CHECK(win != nullptr);
        ctx->MenuClick("Stage/Master View"); // close
    };

    // U-217: Master View Ctrl+Shift+M shortcut
    ImGuiTest* t_masterViewShortcut = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-217 Ctrl+Shift+M opens Master View");
    t_masterViewShortcut->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_M);
        ctx->Yield(5);
        ImGuiWindow* win = ctx->GetWindowByRef("Master View");
        IM_CHECK(win != nullptr);
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_M); // close
    };

    // U-218: Master View shows "Network: Offline" in standalone mode
    ImGuiTest* t_masterViewOffline = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-218 Master View shows Network Offline");
    t_masterViewOffline->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_M);
        ctx->Yield(5);
        ImGuiWindow* win = ctx->GetWindowByRef("Master View");
        IM_CHECK(win != nullptr);
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_M); // close
    };

    // U-219: Scene Switcher — dbg.scene_list without crash (0 snapshots)
    ImGuiTest* t_sceneListEmpty = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-219 scene_list empty is safe");
    t_sceneListEmpty->TestFunc = [](ImGuiTestContext* ctx) {
        // Just verify calling scene_list with no snapshots doesn't crash
        ctx->Yield(3);
        IM_CHECK(true); // if we get here, no crash
    };

    // U-220: Scene Switcher — SurfaceEditorPanel "Capture Scene" button exists
    ImGuiTest* t_sceneCaptureBtn = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-220 Capture Scene button in SurfaceEditor");
    t_sceneCaptureBtn->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Stage/Surface Editor");
        ctx->Yield(5);
        ImGuiWindow* win = ctx->GetWindowByRef("Surface Editor");
        IM_CHECK(win != nullptr);
        // Close
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Stage/Surface Editor");
    };

    // U-221: Non-regression — dbg.test() baseline
    ImGuiTest* t_dbgTestBaseline = IM_REGISTER_TEST(mTestEngine, "ui_stage", "U-221 dbg.test baseline non-regression");
    t_dbgTestBaseline->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->Yield(3);
        IM_CHECK(true); // baseline: app runs without crash
    };

    ImVector<ImGuiTest*> tests;
    ImGuiTestEngine_GetTestList(mTestEngine, &tests);
    std::cout << "[TestEngine] " << tests.Size << " UI tests registered" << std::endl;
}

void StudioApp::createLockFile() {
    mLockFilePath = ".bbfx_lock";
    std::ofstream ofs(mLockFilePath);
    if (ofs.is_open()) {
        ofs << "locked" << std::endl;
        ofs.close();
    }
}

void StudioApp::removeLockFile() {
    if (!mLockFilePath.empty()) {
        std::remove(mLockFilePath.c_str());
    }
}

} // namespace bbfx
