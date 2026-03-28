#include "StudioApp.h"
#include "../core/Animator.h"
#include "../core/PrimitiveNodes.h"

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

namespace bbfx {

// ── Construction ─────────────────────────────────────────────────────────────

StudioApp::StudioApp(sol::state& lua, const std::string& initialScript)
    : mLua(lua), mInitialScript(initialScript),
      mLastAutoSave(std::chrono::steady_clock::now())
{
    // Engine creates SDL3 window + GL context + OGRE
    mEngine = std::make_unique<StudioEngine>(lua);

    // Panels (created after engine so OGRE is ready)
    mViewportPanel        = std::make_unique<ViewportPanel>(mEngine.get());
    mNodeEditorPanel      = std::make_unique<NodeEditorPanel>(lua);
    mInspectorPanel       = std::make_unique<InspectorPanel>();
    mTimelinePanel        = std::make_unique<TimelinePanel>();
    mPresetBrowserPanel   = std::make_unique<PresetBrowserPanel>(mNodeEditorPanel.get());
    mPerformanceModePanel = std::make_unique<PerformanceModePanel>();

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
            mRunning = false;
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
            SDL_SetWindowTitle(mEngine->getSDLWindow(), "BBFx Studio v3.0");
            std::cout << "[Studio] New project" << std::endl;
        }
        if (ImGui::MenuItem("Open...", "Ctrl+O")) {
            loadProject("project.bbfx-project");
        }
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
            if (mProjectPath.empty()) {
                saveProject("project.bbfx-project");
            } else {
                saveProject(mProjectPath);
            }
        }
        if (ImGui::MenuItem("Save As...")) {
            saveProject("project.bbfx-project");
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
        if (ImGui::MenuItem("Exit", "Alt+F4"))       { mRunning = false; }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Viewport",      nullptr, &mShowViewport);
        ImGui::MenuItem("Node Editor",   nullptr, &mShowNodeEditor);
        ImGui::MenuItem("Inspector",     nullptr, &mShowInspector);
        ImGui::MenuItem("Timeline",      nullptr, &mShowTimeline);
        ImGui::MenuItem("Preset Browser",nullptr, &mShowPresetBrowser);
        ImGui::Separator();
        bool pm = mPerformanceMode;
        if (ImGui::MenuItem("Performance Mode", "F5", &pm)) {
            mPerformanceMode = pm;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About BBFx Studio")) { /* TODO */ }
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

    // Export dialog (modal, shown on demand)
    mExportDialog.render(mEngine.get());
}

// ── Project save / load / auto-save ──────────────────────────────────────────

void StudioApp::saveProject(const std::string& path) {
    if (path.empty()) return;
    if (mSerializer.save(path)) {
        mProjectPath = path;
        // Add to recent projects (deduplicate)
        mRecentProjects.erase(
            std::remove(mRecentProjects.begin(), mRecentProjects.end(), path),
            mRecentProjects.end());
        mRecentProjects.insert(mRecentProjects.begin(), path);
        if (mRecentProjects.size() > 10) mRecentProjects.resize(10);
        SDL_SetWindowTitle(mEngine->getSDLWindow(), ("BBFx Studio — " + path).c_str());
        std::cout << "[Studio] Saved: " << path << std::endl;
    } else {
        std::cerr << "[Studio] Save failed: " << mSerializer.getLastError() << std::endl;
    }
}

void StudioApp::loadProject(const std::string& path) {
    if (path.empty()) return;
    if (mSerializer.load(path, mLua)) {
        mProjectPath = path;
        mRecentProjects.erase(
            std::remove(mRecentProjects.begin(), mRecentProjects.end(), path),
            mRecentProjects.end());
        mRecentProjects.insert(mRecentProjects.begin(), path);
        if (mRecentProjects.size() > 10) mRecentProjects.resize(10);
        SDL_SetWindowTitle(mEngine->getSDLWindow(), ("BBFx Studio — " + path).c_str());
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
        if (mSerializer.save(autoPath)) {
            std::cout << "[Studio] Auto-saved → " << autoPath << std::endl;
        }
        mLastAutoSave = now;
    }
}

} // namespace bbfx
