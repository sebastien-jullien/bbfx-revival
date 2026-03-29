#include "StudioApp.h"
#include "../core/Animator.h"
#include "../core/PrimitiveNodes.h"
#include "../fx/PerlinFxNode.h"
#include "../fx/ShaderFxNode.h"
#include "../fx/TextureBlitterNode.h"
#include "../fx/WaveVertexShader.h"
#include "../fx/ColorShiftNode.h"
#include "../audio/AudioAnalyzer.h"
#include "../audio/BeatDetector.h"
#include "../audio/AudioCapture.h"
#include "../video/TheoraClipNode.h"

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

#ifdef _WIN32
#include <commdlg.h> // GetOpenFileNameA / GetSaveFileNameA
#endif

namespace bbfx {

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
    mInspectorPanel       = std::make_unique<InspectorPanel>(lua);
    mTimelinePanel        = std::make_unique<TimelinePanel>();
    mPresetBrowserPanel   = std::make_unique<PresetBrowserPanel>(mNodeEditorPanel.get(), lua);
    mPerformanceModePanel = std::make_unique<PerformanceModePanel>(lua);
    mConsolePanel         = std::make_unique<ConsolePanel>(mLua);

    initNodeTypeRegistry();
    SettingsManager::instance().load();

    // Load studio chord system (provides ChordSystem global for triggers/quick access)
    mLua.safe_script("require 'studio_chord'", sol::script_pass_on_error);

    // Wire inspector to node editor selection
    mNodeEditorPanel->setSelectionCallback([this](const std::string& nodeName) {
        mInspectorPanel->setSelectedNode(nodeName);
    });

    initImGui();
}

StudioApp::~StudioApp() {
    shutdownImGui();
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

    // Load initial script if provided
    if (!mInitialScript.empty()) {
        auto result = mLua.safe_script_file(mInitialScript, sol::script_pass_on_error);
        if (!result.valid()) {
            sol::error err = result;
            std::cerr << "[Studio] Lua error: " << err.what() << '\n';
        }
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
                SDL_SetWindowTitle(mEngine->getSDLWindow(), "BBFx Studio v3.1");
            }
        }
        if (mRecentProjects.empty() && !settings.get().recentProjects.empty()) {
            mRecentProjects = settings.get().recentProjects;
        }
    }

    while (mRunning) {
        // ── Events ───────────────────────────────────────────────────────────
        handleEvents();

        // ── Animation DAG ─────────────────────────────────────────────────────
        if (time && !mTimelinePanel->isPaused()) time->update();
        if (animator) animator->renderOneFrame();

        // ── Update all registered nodes (DAG only updates connected ones) ────
        if (animator) {
            for (auto& name : animator->getRegisteredNodeNames()) {
                auto* node = animator->getRegisteredNode(name);
                if (node && node->getTypeName() != "RootTimeNode") {
                    node->update();
                }
            }
        }

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
                // In Performance Mode, Escape = exit Performance Mode (not quit app)
                mPerformanceMode = false;
            } else {
                mRunning = false;
            }
            return;
        }
        if (evt.key.key == SDLK_F5) {
            mPerformanceMode = !mPerformanceMode;
            return;
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
            SDL_SetWindowTitle(mEngine->getSDLWindow(), "BBFx Studio v3.1");
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
        // Space = Play/Pause toggle (when not typing in a text field)
        if (evt.key.key == SDLK_SPACE && !io.WantCaptureKeyboard) {
            if (mTimelinePanel) {
                mTimelinePanel->togglePause();
            }
            return;
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
    SDL_GL_SwapWindow(mEngine->getSDLWindow());
}

void StudioApp::renderMenuBar() {
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New", "Ctrl+N")) {
            mProjectPath.clear();
            mProjectDirty = false;
            SDL_SetWindowTitle(mEngine->getSDLWindow(), "BBFx Studio v3.1");
            std::cout << "[Studio] New project" << std::endl;
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
        ImGui::Separator();
        bool pm = mPerformanceMode;
        if (ImGui::MenuItem("Performance Mode", "F5", &pm)) {
            mPerformanceMode = pm;
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

    // Export dialog (modal, shown on demand)
    mExportDialog.render(mEngine.get());

    // Modal dialogs
    renderAboutDialog();
    renderShortcutsDialog();
    renderSettingsDialog();
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
            state.chords.push_back({cb.name, cb.startBeat, cb.endBeat, cb.hue});
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
                chords.push_back({cd.name, cd.startBeat, cd.endBeat, cd.hue});
            }
            mTimelinePanel->setChordBlocks(chords);
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
                autoState.chords.push_back({cb.name, cb.startBeat, cb.endBeat, cb.hue});
            }
        }
        if (mSerializer.save(autoPath, autoState)) {
            std::cout << "[Studio] Auto-saved → " << autoPath << std::endl;
        }
        mLastAutoSave = now;
    }
}

// ── Node type registration ──────────────────────────────────────────────────

void StudioApp::initNodeTypeRegistry() {
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
        [](const std::string& /*name*/, sol::state& /*lua*/) -> AnimationNode* {
            auto* node = new AccumulatorNode();
            auto* animator = Animator::instance();
            if (animator) {
                for (auto& [pname, port] : node->getInputs()) animator->add(port);
                for (auto& [pname, port] : node->getOutputs()) animator->add(port);
                node->setListener(animator);
                animator->registerNode(node);
            }
            return node;
        }});

    // FX — these need OGRE scene objects, register with placeholder factories
    reg.registerType({"PerlinFxNode", "FX", {1.0f, 0.5f, 0.2f, 1.0f},
        [](const std::string& /*name*/, sol::state& /*lua*/) -> AnimationNode* {
            std::cout << "[Studio] PerlinFxNode requires scene context — use Lua to create" << std::endl;
            return nullptr;
        }});

    reg.registerType({"ShaderFxNode", "FX", {1.0f, 0.5f, 0.2f, 1.0f},
        [](const std::string& /*name*/, sol::state& /*lua*/) -> AnimationNode* {
            std::cout << "[Studio] ShaderFxNode requires scene context — use Lua to create" << std::endl;
            return nullptr;
        }});

    reg.registerType({"TextureBlitterNode", "FX", {1.0f, 0.5f, 0.2f, 1.0f},
        [](const std::string& /*name*/, sol::state& /*lua*/) -> AnimationNode* {
            std::cout << "[Studio] TextureBlitterNode requires scene context — use Lua to create" << std::endl;
            return nullptr;
        }});

    reg.registerType({"WaveVertexShader", "FX", {1.0f, 0.5f, 0.2f, 1.0f},
        [](const std::string& /*name*/, sol::state& /*lua*/) -> AnimationNode* {
            std::cout << "[Studio] WaveVertexShader requires scene context — use Lua to create" << std::endl;
            return nullptr;
        }});

    reg.registerType({"ColorShiftNode", "FX", {1.0f, 0.5f, 0.2f, 1.0f},
        [](const std::string& /*name*/, sol::state& /*lua*/) -> AnimationNode* {
            std::cout << "[Studio] ColorShiftNode requires scene context — use Lua to create" << std::endl;
            return nullptr;
        }});

    // Audio
    reg.registerType({"AudioAnalyzerNode", "Audio", {0.8f, 0.3f, 0.8f, 1.0f},
        [](const std::string& /*name*/, sol::state& /*lua*/) -> AnimationNode* {
            std::cout << "[Studio] AudioAnalyzerNode requires AudioCaptureNode — use Lua to create" << std::endl;
            return nullptr;
        }});

    reg.registerType({"BeatDetectorNode", "Audio", {0.8f, 0.3f, 0.8f, 1.0f},
        [](const std::string& /*name*/, sol::state& /*lua*/) -> AnimationNode* {
            std::cout << "[Studio] BeatDetectorNode requires AudioAnalyzerNode — use Lua to create" << std::endl;
            return nullptr;
        }});

    reg.registerType({"AudioCaptureNode", "Audio", {0.8f, 0.3f, 0.8f, 1.0f},
        [](const std::string& /*name*/, sol::state& /*lua*/) -> AnimationNode* {
            std::cout << "[Studio] AudioCaptureNode requires AudioCapture — use Lua to create" << std::endl;
            return nullptr;
        }});

    // Video
    reg.registerType({"TheoraClipNode", "Video", {0.9f, 0.8f, 0.2f, 1.0f},
        [](const std::string& /*name*/, sol::state& /*lua*/) -> AnimationNode* {
            std::cout << "[Studio] TheoraClipNode requires video file — use Lua to create" << std::endl;
            return nullptr;
        }});

    // Animation
    reg.registerType({"AnimationStateNode", "Animation", {0.2f, 0.8f, 0.8f, 1.0f},
        [](const std::string& /*name*/, sol::state& /*lua*/) -> AnimationNode* {
            std::cout << "[Studio] AnimationStateNode requires OGRE AnimationState — use Lua to create" << std::endl;
            return nullptr;
        }});

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
        ImGui::Text("BBFx Studio v3.1.0");
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
            row("Ctrl+1-9","Save bookmark (Node Editor)");
            row("1-9",     "Restore bookmark (Node Editor)");
            row("Delete",  "Delete selected node/link");
            row("Escape",  "Exit Performance Mode / Quit");

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

} // namespace bbfx
