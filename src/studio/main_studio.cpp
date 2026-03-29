#include "StudioApp.h"
#include "SettingsManager.h"
#include "../platform.h"
#include "../core/Animator.h"
#include "../core/PrimitiveNodes.h"
#include "../bindings/bbfx_bindings.h"
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
                           sol::lib::package);

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
        bool forceDefault = false;
        bool forceReset = false;
        bool fullscreen = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--default") {
                forceDefault = true;
            } else if (arg == "--reset") {
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
                int rc = std::system("cmake --build ../.. --config Debug --target bbfx-studio");
                if (rc != 0) {
                    std::cerr << "[Studio] Build failed (exit " << rc << ")" << std::endl;
                    return 1;
                }
                std::cout << "[Studio] Build OK, continuing..." << std::endl;
            } else if (arg == "--fullscreen" || arg == "-f") {
                fullscreen = true;
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
        bbfx::StudioApp app(lua, initialScript, forceDefault, forceReset);
        if (fullscreen) {
            SDL_SetWindowFullscreen(app.getEngine()->getSDLWindow(), true);
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
