#pragma once

#include "StudioEngine.h"
#include "panels/ViewportPanel.h"
#include "panels/NodeEditorPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/TimelinePanel.h"
#include "panels/PresetBrowserPanel.h"
#include "panels/PerformanceModePanel.h"
#include "panels/ConsolePanel.h"
#include "commands/CommandManager.h"
#include "NodeTypeRegistry.h"
#include "SettingsManager.h"
#include "project/ProjectSerializer.h"
#include "project/ExportDialog.h"

#include <sol/sol.hpp>
#include <memory>
#include <string>
#include <vector>
#include <chrono>

namespace bbfx {

/// Top-level application class for BBFx Studio.
///
/// Owns the full render loop: SDL3 events → ImGui frame → OGRE RenderTexture → swap.
/// Orchestrates all panels (Viewport, Node Editor, Inspector, Timeline, Presets, Performance).
class StudioApp {
public:
    explicit StudioApp(sol::state& lua, const std::string& initialScript = "",
                       bool forceDefault = false, bool forceReset = false);
    ~StudioApp();

    /// Run the main loop until the user closes the window.
    void run();

    StudioEngine* getEngine() { return mEngine.get(); }

private:
    // ── Init ─────────────────────────────────────────────────────────────────
    void initImGui();
    void shutdownImGui();
    void applyDarkTheme();

    // ── Per-frame ─────────────────────────────────────────────────────────────
    void handleEvents();
    void handleEvent(const SDL_Event& evt);
    void renderFrame();
    void renderMenuBar();
    void renderPanels();
    void renderStatusBar();
    void renderAboutDialog();
    void renderShortcutsDialog();
    void renderSettingsDialog();

    // ── Node type registration ──────────────────────────────────────────────
    void initNodeTypeRegistry();

    // ── Project / IO ─────────────────────────────────────────────────────────
    void saveProject(const std::string& path);
    void loadProject(const std::string& path);
    void tickAutoSave();

    // ── State ────────────────────────────────────────────────────────────────
    sol::state& mLua;
    std::unique_ptr<StudioEngine> mEngine;
    bool mRunning = true;
    bool mPerformanceMode = false;
    bool mForceDefault = false;
    bool mForceReset = false;
    std::string mInitialScript;

    // Project state
    std::string mProjectPath;
    std::vector<std::string> mRecentProjects;
    ProjectSerializer mSerializer;
    ExportDialog mExportDialog;

    // Auto-save
    std::chrono::steady_clock::time_point mLastAutoSave;
    static constexpr int kAutoSaveIntervalSec = 120; // 2 minutes

    // ── Panels ───────────────────────────────────────────────────────────────
    std::unique_ptr<ViewportPanel>         mViewportPanel;
    std::unique_ptr<NodeEditorPanel>       mNodeEditorPanel;
    std::unique_ptr<InspectorPanel>        mInspectorPanel;
    std::unique_ptr<TimelinePanel>         mTimelinePanel;
    std::unique_ptr<PresetBrowserPanel>    mPresetBrowserPanel;
    std::unique_ptr<PerformanceModePanel>  mPerformanceModePanel;
    std::unique_ptr<ConsolePanel>          mConsolePanel;

    // ── Panel visibility toggles ──────────────────────────────────────────────
    bool mShowViewport      = true;
    bool mShowNodeEditor    = true;
    bool mShowInspector     = true;
    bool mShowTimeline      = true;
    bool mShowPresetBrowser = true;
    bool mShowConsole       = false;
    bool mShowAbout         = false;
    bool mShowShortcuts     = false;
    bool mShowSettings      = false;
    bool mProjectDirty      = false;
};

} // namespace bbfx
