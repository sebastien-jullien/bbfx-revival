#include "StudioApp.h"
#include "DagSnapshot.h"
#include "commands/NodeCommands.h"
#include "commands/SceneCommands.h"
#include "../core/Animator.h"
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
#include "../midi/MidiDeviceManager.h"
#include "../midi/MidiLearnManager.h"
#include "nodes/LightNode.h"
#include "nodes/ParticleNode.h"
#include "nodes/CompositorNode.h"
#include "nodes/TextureNode.h"
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

StudioApp::StudioApp(sol::state& lua, const std::string& initialScript, bool forceDefault, bool forceReset)
    : mLua(lua), mInitialScript(initialScript), mForceDefault(forceDefault), mForceReset(forceReset),
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
    mThumbCache           = std::make_unique<TextureThumbnailCache>();
    mPresetBrowserPanel   = std::make_unique<PresetBrowserPanel>(mNodeEditorPanel.get(), lua);
    mPresetBrowserPanel->setThumbCache(mThumbCache.get());
    // ShaderPreviewRenderer connection deferred until after initialization (see below)
    mInspectorPanel->setThumbCache(mThumbCache.get());
    mPerformanceModePanel = std::make_unique<PerformanceModePanel>(lua);
    mConsolePanel         = std::make_unique<ConsolePanel>(mLua);
    mSetEditorPanel       = std::make_unique<SetEditorPanel>(mLua);
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
    mPresetBrowserPanel->setPreviewRenderer(mPreviewRenderer.get());
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
    mOutputPanel = std::make_unique<OutputPanel>();
    mOscBrowserPanel = std::make_unique<OscBrowserPanel>();

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
    SettingsManager::instance().load();

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
                    } else if (nodeType == "CompositorNode") {
                        auto* p = n->getParamSpec()->getParam("compositor");
                        if (p) p->stringVal = paramValue;
                    } else if (nodeType == "TextureNode") {
                        auto* p = n->getParamSpec()->getParam("texture");
                        if (p) p->stringVal = paramValue;
                    } else if (nodeType == "MaterialNode") {
                        auto* p = n->getParamSpec()->getParam("material");
                        if (p) p->stringVal = paramValue;
                    }
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
                                std::cout << "[CreateNode] Linked " << targetNode
                                          << ".entity → " << name << ".entity" << std::endl;
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

    // Load project: --default/--reset → template, else last saved project, else template
    {
        auto& settings = SettingsManager::instance();
        const auto& lastPath = settings.get().lastProjectPath;
        bool loadTemplate = mForceDefault || lastPath.empty() || !std::filesystem::exists(lastPath);

        if (!loadTemplate) {
            std::cout << "[Studio] Loading last project: " << lastPath << std::endl;
            loadProject(lastPath);
        } else {
            std::string templatePath = "data/templates/default.bbfx-project";
            if (std::filesystem::exists(templatePath)) {
                std::cout << "[Studio] Loading default template" << std::endl;
                loadProject(templatePath);
                mProjectPath.clear();
                SDL_SetWindowTitle(mEngine->getSDLWindow(), "BBFx Studio v3.3");
            }
        }
        if (mRecentProjects.empty() && !settings.get().recentProjects.empty()) {
            mRecentProjects = settings.get().recentProjects;
        }
    }

    while (mRunning) {
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
        if (animator) {
            for (auto& name : animator->getRegisteredNodeNames()) {
                auto* node = animator->getRegisteredNode(name);
                if (node && node->isEnabled() && node->getTypeName() != "RootTimeNode") {
                    node->update();
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

        // ── Auto-save ─────────────────────────────────────────────────────────
        tickAutoSave();

        // ── ImGui frame ───────────────────────────────────────────────────────
        renderFrame();
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

    // Global events that always apply
    if (evt.type == SDL_EVENT_QUIT) {
        mRunning = false;
        return;
    }

    if (evt.type == SDL_EVENT_KEY_DOWN) {
        if (evt.key.key == SDLK_ESCAPE) {
            if (mPerformanceMode) {
                mPerformanceMode = false;
                if (mPerformanceModePanel && mEngine)
                    mPerformanceModePanel->removeCompositorChain(mEngine.get());
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

        bool ctrl = (evt.key.mod & SDL_KMOD_CTRL) != 0;
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
        if (ctrl && evt.key.key == SDLK_D) {
            // Duplicate placeholder — actual duplication handled by NodeEditorPanel
            return;
        }
        if (ctrl && evt.key.key == SDLK_N) {
            mProjectPath.clear();
            mProjectDirty = false;
            SDL_SetWindowTitle(mEngine->getSDLWindow(), "BBFx Studio v3.3");
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
            }
        }
    }

    if (evt.type == SDL_EVENT_WINDOW_RESIZED) {
        // Resize handled by ViewportPanel when it detects size change
    }

    // Forward to REPL/input only if ImGui is not capturing keyboard/mouse
    if (!io.WantCaptureKeyboard && !io.WantCaptureMouse) {
        if (auto* im = mEngine->getInputManager()) {
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
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
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
        ImGui::DockSpace(dockId, {0, 0}, ImGuiDockNodeFlags_PassthruCentralNode);

        // First-launch layout: dock all panels programmatically
        static bool firstFrame = true;
        if (firstFrame) {
            firstFrame = false;
            ImGui::DockBuilderRemoveNode(dockId);
            ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockId, viewport->WorkSize);

            ImGuiID dockLeft, dockCenter, dockRight;
            ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Left, 0.20f, &dockLeft, &dockCenter);
            ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Right, 0.25f, &dockRight, &dockCenter);

            ImGuiID dockBottom;
            ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Down, 0.30f, &dockBottom, &dockCenter);

            ImGuiID dockViewport, dockNodeEditor;
            ImGui::DockBuilderSplitNode(dockCenter, ImGuiDir_Left, 0.50f, &dockViewport, &dockNodeEditor);

            ImGui::DockBuilderDockWindow("Presets",     dockLeft);
            ImGui::DockBuilderDockWindow("Viewport",    dockViewport);
            ImGui::DockBuilderDockWindow("Node Editor", dockNodeEditor);
            ImGui::DockBuilderDockWindow("Inspector",   dockRight);
            ImGui::DockBuilderDockWindow("Timeline",    dockBottom);

            ImGui::DockBuilderFinish(dockId);
        }

        ImGui::End();
    }

    // Panels
    renderPanels();

    // Status bar (design mode only)
    if (!mPerformanceMode) {
        renderStatusBar();
    }

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
            // Clear all non-singleton nodes from the DAG
            auto* animator = Animator::instance();
            if (animator) {
                auto names = animator->getRegisteredNodeNames();
                for (auto& n : names) {
                    if (n == "time") continue; // keep RootTimeNode
                    auto* nd = animator->getRegisteredNode(n);
                    if (nd) {
                        animator->removeNode(nd);
                        nd->cleanup();
                        delete nd;
                    }
                }
            }
            mProjectPath.clear();
            mProjectDirty = false;
            SDL_SetWindowTitle(mEngine->getSDLWindow(), "BBFx Studio v3.3");
            std::cout << "[Studio] New project — DAG cleared" << std::endl;
        }
        if (ImGui::MenuItem("Open...", "Ctrl+O")) {
            static const char* filter = "BBFx Project (*.bbfx-project)\0*.bbfx-project\0All Files\0*.*\0";
            auto path = openFileDialog(mEngine->getSDLWindow(), filter, "Open BBFx Project");
            if (!path.empty()) loadProject(path);
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
        ImGui::MenuItem("Output",         nullptr, &mShowOutput);
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
                if (mViewportPanel)
                    mViewportPanel->invalidateSize();
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Connect")) {
        ImGui::MenuItem("MIDI Activity",  nullptr, &mShowMidiActivity);
        ImGui::MenuItem("MIDI Mapping",   nullptr, &mShowMidiMapping);
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
    if (mShowMidiActivity && mMidiActivityPanel) mMidiActivityPanel->render();
    if (mShowMidiMapping && mMidiMappingPanel) mMidiMappingPanel->render();
    if (mShowOutput && mOutputPanel) mOutputPanel->render(mEngine.get());
    if (mShowOscBrowser && mOscBrowserPanel) mOscBrowserPanel->render();

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

        // Mode
        ImGui::TextDisabled("Studio");
        ImGui::SameLine(500);

        // Version
        ImGui::TextDisabled("v3.3.0");

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
    renderSplashScreen();
}

// ── Project save / load / auto-save ──────────────────────────────────────────

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
        // Persist in settings for auto-load on next startup
        auto& settings = SettingsManager::instance();
        auto s = settings.get();
        s.lastProjectPath = path;
        s.recentProjects = mRecentProjects;
        settings.set(s);
        settings.save();
        SDL_SetWindowTitle(mEngine->getSDLWindow(), ("BBFx Studio — " + path).c_str());
        std::cout << "[Studio] Saved: " << path << std::endl;
    } else {
        std::cerr << "[Studio] Save failed: " << mSerializer.getLastError() << std::endl;
    }
}

void StudioApp::loadProject(const std::string& path) {
    if (path.empty()) return;
    ProjectSerializer::ProjectState state;
    if (mSerializer.load(path, mLua, &state)) {
        mProjectPath = path;
        mRecentProjects.erase(
            std::remove(mRecentProjects.begin(), mRecentProjects.end(), path),
            mRecentProjects.end());
        mRecentProjects.insert(mRecentProjects.begin(), path);
        if (mRecentProjects.size() > 10) mRecentProjects.resize(10);
        SDL_SetWindowTitle(mEngine->getSDLWindow(), ("BBFx Studio — " + path).c_str());
        // Persist last project for auto-reload on next startup (skip built-in templates)
        if (path.find("data/templates/") == std::string::npos) {
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

        // Rebuild fader/trigger MIDI bindings from MidiLearnManager (single source of truth)
        if (mPerformanceModePanel) {
            mPerformanceModePanel->syncMidiBindingsFromManager();
            mPerformanceModePanel->resetRestSnapshot(); // recapture rest state from loaded project
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
            auto* sceneMgr = Engine::instance()->getSceneManager();
            if (!sceneMgr) return nullptr;
            // Find the first SceneObjectNode's entity to get materials
            Ogre::Entity* targetEntity = nullptr;
            auto* animator = Animator::instance();
            if (animator) {
                for (auto& nodeName : animator->getRegisteredNodeNames()) {
                    auto* node = animator->getRegisteredNode(nodeName);
                    if (node && node->getTypeName() == "SceneObjectNode") {
                        auto* soNode = dynamic_cast<SceneObjectNode*>(node);
                        if (soNode && soNode->getEntity()) {
                            targetEntity = soNode->getEntity();
                            if (targetEntity) break;
                        }
                    }
                }
            }
            if (!targetEntity) return nullptr; // No SceneObjectNode in scene
            std::string firstMat = targetEntity->getSubEntity(0)->getMaterialName();
            auto* node = new ColorShiftNode(firstMat, name);
            for (unsigned i = 1; i < targetEntity->getNumSubEntities(); i++) {
                node->addMaterial(targetEntity->getSubEntity(i)->getMaterialName());
            }
            auto* anim2 = Animator::instance();
            if (anim2) {
                for (auto& [pname, port] : node->getInputs()) anim2->add(port);
                for (auto& [pname, port] : node->getOutputs()) anim2->add(port);
                node->setListener(anim2);
                anim2->registerNode(node);
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
        [](const std::string& name, sol::state& /*lua*/) -> AnimationNode* {
            // TheoraClipNode will be dormant if the video file doesn't exist
            auto* node = new TheoraClipNode("video/bombe.ogg");
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

    // Animation
    // AnimationStateNode disabled — ninja.mesh skeleton triggers OGRE crash
    // (SceneManager::getAnimation "Attack1" not found). Will be re-enabled
    // when a proper animated mesh pipeline is implemented.
    // reg.registerType({"AnimationStateNode", ...});

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
}

// ── Status bar ──────────────────────────────────────────────────────────────

void StudioApp::renderStatusBar() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float barHeight = 24.0f;
    ImVec2 barPos = {viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - barHeight};
    ImVec2 barSize = {viewport->WorkSize.x, barHeight};

    ImGui::SetNextWindowPos(barPos);
    ImGui::SetNextWindowSize(barSize);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.0f, 4.0f});
    if (ImGui::Begin("##StatusBar", nullptr, flags)) {
        float fps = ImGui::GetIO().Framerate;
        ImGui::Text("FPS: %.0f", fps);

        auto* animator = Animator::instance();
        if (animator) {
            int nodeCount = static_cast<int>(animator->getRegisteredNodeNames().size());
            // Cache link count (getLinks() iterates the Boost graph — expensive per frame)
            static int cachedLinkCount = 0;
            static float linkTimer = 0.0f;
            linkTimer += ImGui::GetIO().DeltaTime;
            if (linkTimer >= 1.0f) {
                cachedLinkCount = static_cast<int>(animator->getLinks().size());
                linkTimer = 0.0f;
            }
            int linkCount = cachedLinkCount;

            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            ImGui::Text("Nodes: %d", nodeCount);

            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            ImGui::Text("Links: %d", linkCount);
        }

        // Audio status
        bool audioActive = false;
        if (animator) {
            for (auto& name : animator->getRegisteredNodeNames()) {
                auto* node = animator->getRegisteredNode(name);
                if (node && node->getTypeName() == "AudioAnalyzerNode") {
                    audioActive = true;
                    break;
                }
            }
        }
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        if (audioActive) {
            ImGui::TextColored({0.0f, 1.0f, 0.5f, 1.0f}, "Audio: On");
        } else {
            ImGui::TextDisabled("Audio: Off");
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

        // Output window indicator
        {
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            if (mEngine && mEngine->isOutputOpen()) {
                ImGui::TextColored({0.3f, 0.8f, 1.0f, 1.0f}, "Output: On");
            } else {
                ImGui::TextDisabled("Output: Off");
            }
        }

        // Project dirty indicator
        if (mProjectDirty) {
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            ImGui::TextColored({1.0f, 0.8f, 0.0f, 1.0f}, "*");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

// ── About dialog ────────────────────────────────────────────────────────────

void StudioApp::renderAboutDialog() {
    if (mShowAbout) {
        ImGui::OpenPopup("About BBFx Studio");
        mShowAbout = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("About BBFx Studio", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("BBFx Studio v3.3.0");
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
            row("Ctrl+1-9","Save bookmark (Node Editor)");
            row("1-9",     "Restore bookmark (Node Editor)");
            row("Delete",  "Delete selected node/link");
            row("Escape",  "Exit Performance Mode / Quit");
            row("F11",     "Toggle output fullscreen");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "--- Connect ---");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("");

            row("M (fader)", "MIDI Learn for fader");
            row("Right-click trig", "MIDI Learn for trigger");
            row("Right-click port", "MIDI Learn for DAG port");
            row("1-9 / Q-W-E-R-T", "Trigger shortcuts (F5 mode)");
            row("Tab",      "Cycle trigger pages (F5 mode)");

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

void StudioApp::renderSettingsDialog() {
    if (mShowSettings) {
        ImGui::OpenPopup("Settings");
        mShowSettings = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto& mgr = SettingsManager::instance();
        Settings settings = mgr.get();

        ImGui::Text("General");
        ImGui::Separator();
        ImGui::SliderInt("Auto-save interval (sec)", &settings.autoSaveInterval, 30, 600);
        ImGui::SliderInt("Font size", &settings.fontSize, 10, 24);
        ImGui::Spacing();

        ImGui::Text("Rendering");
        ImGui::Separator();
        ImGui::SliderFloat("Viewport scale", &settings.viewportScale, 0.25f, 4.0f, "%.2f");
        ImGui::Spacing();

        ImGui::Text("Audio");
        ImGui::Separator();
        ImGui::SliderFloat("Default BPM", &settings.defaultBPM, 60.0f, 240.0f, "%.1f");
        ImGui::Spacing();

        ImGui::Separator();
        if (ImGui::Button("Save", {100, 0})) {
            mgr.set(settings);
            mgr.save();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", {100, 0})) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
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
    registerViewToggle("Presets");
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

    ImVector<ImGuiTest*> tests;
    ImGuiTestEngine_GetTestList(mTestEngine, &tests);
    std::cout << "[TestEngine] " << tests.Size << " UI tests registered" << std::endl;
}

void StudioApp::renderSplashScreen() {
    if (!mShowSplash) return;

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 winSize = {500, 350};
    ImGui::SetNextWindowPos({(displaySize.x - winSize.x) / 2, (displaySize.y - winSize.y) / 2});
    ImGui::SetNextWindowSize(winSize);
    ImGui::Begin("BBFx Studio", &mShowSplash,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

    ImGui::TextColored({0.0f, 1.0f, 0.8f, 1.0f}, "BBFx Studio v3.3.0 Connect");
    ImGui::TextDisabled("Performance Pro & Final Polish");
    ImGui::Separator();

    ImGui::TextUnformatted("Welcome to BBFx Studio!");
    ImGui::Spacing();

    if (ImGui::Button("New Empty Project", {200, 30})) {
        mShowSplash = false;
    }
    if (ImGui::Button("Open Project...", {200, 30})) {
        mShowSplash = false;
    }

    ImGui::End();
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
