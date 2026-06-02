#include "StudioApp.h"
#include "SettingsManager.h"
#include "../platform.h"
#include "../core/Animator.h"
#include "../core/PrimitiveNodes.h"
#include "../bindings/bbfx_bindings.h"
#include "bbfx_imgui_bindings.h"
#include "../plugin/PluginManager.h"
#include "../plugin/PluginValidator.h"
#include "../plugin/DeepLinkHandler.h"
#include "../plugin/DeepLinkRegistry.h"
#include "SettingsManager.h"
#include <ogre_lua/ogre_lua.h>

#include <sol/sol.hpp>
#include <SDL3/SDL.h>
#include <iostream>
#include <filesystem>
#include <cstdlib>

namespace {
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
} // namespace

int main(int argc, char* argv[]) {
    try {
        // Set working directory to executable location (so resources.cfg is found)
        auto exePath = std::filesystem::path(argv[0]).parent_path();
        if (!exePath.empty()) {
            std::filesystem::current_path(exePath);
        }
        auto projectRoot = findProjectRoot(std::filesystem::current_path());

        sol::state lua;
        lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string,
                           sol::lib::table, sol::lib::io, sol::lib::os,
                           sol::lib::package, sol::lib::coroutine);

        // Prepend lua/ to package.path
        std::string packagePath = lua["package"]["path"].get<std::string>();
        if (!projectRoot.empty()) {
            packagePath = (projectRoot / "lua" / "?.lua").string() + ";" + packagePath;
        }
        packagePath = std::string("lua/?.lua;") + packagePath;
        lua["package"]["path"] = packagePath;

        // Register bindings
        ogre_lua::register_all(lua);
        bbfx::register_bbfx_bindings(lua);
        // v3.5 Lot P — ImGui widget bindings (Studio-only, headless has no
        // ImGui context). Must come before plugin scan so plugins with
        // `ui` permission see bbfx.ui.* populated.
        bbfx::registerImguiBindings(lua);

        // v3.5 Lot D: load settings first so autoEnableFromSettings() below
        // knows which plugins to turn on. StudioApp will load() again in its
        // constructor, which is idempotent.
        bbfx::SettingsManager::instance().load();

        // v3.5 Lot A: scan installed plugins so they are discoverable by
        // bbfx.plugin.list() and by the Studio panels registered later.
        // v3.5 Lot B: also register the sol::state so load/enable can run
        // init.lua inside the plugin sandbox.
        bbfx::PluginManager::instance().setLuaState(lua);
        bbfx::PluginManager::instance().scanDirectories();
        {
            const auto ids = bbfx::PluginManager::instance().listPlugins();
            std::cout << "[Studio] Plugins: found " << ids.size()
                      << " at " << bbfx::PluginManager::instance().getUserPluginsDir()
                      << std::endl;
        }

        // v3.5 Lot D: auto-enable plugins persisted from previous sessions.
        bbfx::PluginManager::instance().autoEnableFromSettings();

        // v3.5 Lot I: register the bbfx:// URL scheme (HKCU, idempotent).
        {
            auto exe = std::filesystem::canonical(argv[0]).string();
            bbfx::DeepLinkRegistry::registerScheme(exe);
        }

        // Create core singletons (owned here, shared via singletons)
        bbfx::Animator animator;
        bbfx::RootTimeNode timeNode;

        // Full registration: add ports to DAG graph + set listener (same as Lua addNode)
        for (auto& [name, port] : timeNode.getInputs())  animator.add(port);
        for (auto& [name, port] : timeNode.getOutputs()) animator.add(port);
        timeNode.setListener(&animator);
        animator.registerNode(&timeNode);

        // Parse arguments
        std::string initialScript;
        std::string initialDemo;
        std::string deepLink;
        bool forceDefault = false;
        bool forceReset = false;
        bool fullscreen = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--default" || arg == "--reset") {
                forceDefault = true;
                forceReset = true;
            } else if (arg == "--clear") {
                // Factory reset: delete settings + layout from disk
                auto& settings = bbfx::SettingsManager::instance();
                settings.set(bbfx::Settings{}); // defaults
                settings.save();
                std::filesystem::remove("imgui.ini");
                std::cout << "[Studio] --clear: factory reset (settings + layout deleted)" << std::endl;
                forceDefault = true;
            } else if (arg == "--build") {
                std::cout << "[Studio] --build: rebuilding..." << std::endl;
#ifdef BBFX_WIN32
                char exePathBuf[MAX_PATH] = {};
                GetModuleFileNameA(nullptr, exePathBuf, MAX_PATH);
                auto exeDir = std::filesystem::path(exePathBuf).parent_path();
                // exe is at build/windows-debug/Debug/ → cmake bundled 3 levels up
                auto cmakeExe = (exeDir / ".." / ".." / ".." / "extern" / "cmake" / "bin" / "cmake.exe").lexically_normal();
                auto buildDir = (exeDir / "..").lexically_normal();
                std::string cmd = "\"" + cmakeExe.string() + "\" --build \"" + buildDir.string() + "\" --config Debug --target bbfx-studio";
#else
                std::string cmd = "cmake --build .. --config Debug --target bbfx-studio";
#endif
                int rc = std::system(cmd.c_str());
                if (rc != 0) {
                    std::cerr << "[Studio] Build failed (exit " << rc << ")" << std::endl;
                    return 1;
                }
                std::cout << "[Studio] Build OK, continuing..." << std::endl;
            } else if (arg == "--d3d11") {
                bbfx::Engine::setRendererOverride("Direct3D11 Rendering Subsystem");
                std::cout << "[Studio] --d3d11: using Direct3D11 renderer" << std::endl;
            } else if (arg == "--fullscreen" || arg == "-f") {
                fullscreen = true;
            } else if (arg == "--demo" && i + 1 < argc) {
                // Lot AV.5 — charge directement le builder d'une démo au démarrage
                // (skip default project load). Usage : --demo demo_video_wall
                initialDemo = argv[++i];
            } else if (arg == "--deep-link" && i + 1 < argc) {
                deepLink = argv[++i];
            } else if (arg.rfind("bbfx://", 0) == 0) {
                // Browsers may pass the URL directly as the single argument.
                deepLink = arg;
            } else if ((arg == "--install" || arg == "--uninstall" ||
                         arg == "--validate-plugin" || arg == "--export-plugin" ||
                         arg == "--list-plugins")
                        /* no trailing arg needed for --list-plugins */) {
                // v3.5 Lot U — plugin CLI ops. Execute non-interactively
                // and exit with status 0 on success, 1 on failure. The
                // caller chains these before opening the GUI.
                std::string op = arg;
                std::string target;
                if (op != "--list-plugins") {
                    if (i + 1 >= argc) {
                        std::cerr << "[Studio] " << op << " requires an argument" << std::endl;
                        return 1;
                    }
                    target = argv[++i];
                }
                bbfx::PluginManager::instance().scanDirectories();
                int rc = 0;
                if (op == "--install") {
                    std::string err;
                    auto id = bbfx::PluginManager::instance().installFromPath(target, &err);
                    if (id.empty()) {
                        std::cerr << "install failed: " << err << std::endl;
                        rc = 1;
                    } else {
                        bbfx::PluginManager::instance().enable(id);
                        std::cout << "installed " << id << std::endl;
                    }
                } else if (op == "--uninstall") {
                    if (!bbfx::PluginManager::instance().uninstall(target)) {
                        std::cerr << "uninstall failed: " << target << std::endl;
                        rc = 1;
                    } else {
                        std::cout << "uninstalled " << target << std::endl;
                    }
                } else if (op == "--validate-plugin") {
                    auto res = bbfx::PluginValidator::validatePath(target);
                    if (!res.ok) {
                        for (auto& e : res.errors) std::cerr << "  - " << e << std::endl;
                        std::cerr << "validate failed: " << target << std::endl;
                        rc = 1;
                    } else {
                        std::cout << "valid: " << target << std::endl;
                    }
                } else if (op == "--export-plugin") {
                    std::cout << "export: interactive mode — use File > Export Plugin"
                               << std::endl;
                    rc = 0;
                } else if (op == "--list-plugins") {
                    for (const auto& id : bbfx::PluginManager::instance().listPlugins()) {
                        std::cout << id << std::endl;
                    }
                }
                return rc;
            } else if (arg[0] != '-') {
                initialScript = arg;
            }
        }

        // --reset: bypass saved project + layout for this session (like first launch)
        // Does NOT modify settings on disk.
        if (forceReset) {
            auto cwd = std::filesystem::current_path();
            std::cout << "[Studio] --reset: cwd=" << cwd.string() << std::endl;
            bool r1 = std::filesystem::remove("imgui.ini");
            bool r2 = std::filesystem::remove("node_editor.json");
            std::cout << "[Studio] --reset: removed imgui.ini=" << r1
                      << " node_editor.json=" << r2 << std::endl;
            forceDefault = true;
            std::cout << "[Studio] --reset: fresh session (default template + reset layout)" << std::endl;
        }

        if (forceDefault) {
            std::cout << "[Studio] --default: will load default template" << std::endl;
        }

        // Run the Studio application
        bbfx::StudioApp app(lua, initialScript, forceDefault, forceReset, initialDemo);
        if (fullscreen) {
            SDL_SetWindowFullscreen(app.getEngine()->getSDLWindow(), true);
        }

        // v3.5 Lot I — dispatch deep-link if any. StudioApp::ctor has
        // already wired DeepLinkHandler callbacks.
        if (!deepLink.empty()) {
            auto a = bbfx::DeepLinkHandler::parse(deepLink);
            bbfx::DeepLinkHandler::instance().handle(a);
        }

        app.run();

    } catch (const Ogre::Exception& e) {
        std::cerr << "OGRE Exception: " << e.getFullDescription() << '\n';
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
