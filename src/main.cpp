#include "platform.h"
#include "core/Engine.h"
#include "core/Animator.h"
#include "bindings/bbfx_bindings.h"
#include <ogre_lua/ogre_lua.h>

#include <sol/sol.hpp>
#include <iostream>
#include <filesystem>

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

std::filesystem::path resolveLuaScriptPath(const std::filesystem::path& projectRoot,
                                           const std::filesystem::path& exeDir,
                                           const std::string& argPath) {
    std::filesystem::path requested(argPath);
    if (requested.is_absolute() && std::filesystem::exists(requested)) {
        return requested;
    }
    if (!projectRoot.empty()) {
        auto fromProject = projectRoot / requested;
        if (std::filesystem::exists(fromProject)) {
            return fromProject;
        }
    }
    auto fromExe = exeDir / requested;
    if (std::filesystem::exists(fromExe)) {
        return fromExe;
    }
    return requested;
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

        // Prepend lua/ to package.path so scripts in lua/demos/ can require
        // modules that live in lua/ (helpers, object, effect, etc.)
        std::string packagePath = lua["package"]["path"].get<std::string>();
        if (!projectRoot.empty()) {
            packagePath = (projectRoot / "lua" / "?.lua").string() + ";" + packagePath;
        }
        packagePath = std::string("lua/?.lua;") + packagePath;
        lua["package"]["path"] = packagePath;

        // Register ogre-lua bindings (Ogre.Vector3, etc.)
        ogre_lua::register_all(lua);

        // Register bbfx bindings (bbfx.Engine, bbfx.Animator, etc.)
        bbfx::register_bbfx_bindings(lua);

        // Create core singletons
        bbfx::Animator animator;
        bbfx::RootTimeNode timeNode;
        bbfx::Engine engine(lua);

        // Pass command-line arguments to Lua (standard arg table)
        sol::table luaArg = lua.create_table();
        for (int i = 0; i < argc; i++) {
            luaArg[i] = std::string(argv[i]);
        }
        lua["arg"] = luaArg;

        // Run Lua script if provided
        if (argc > 1) {
            auto scriptPath = resolveLuaScriptPath(projectRoot, exePath, argv[1]);
            auto result = lua.safe_script_file(scriptPath.string(), sol::script_pass_on_error);
            if (!result.valid()) {
                sol::error err = result;
                std::cerr << "Lua error: " << err.what() << '\n';
                return 1;
            }
        }

        // Run the main loop
        engine.startRendering();

    } catch (const Ogre::Exception& e) {
        std::cerr << "OGRE Exception: " << e.getFullDescription() << '\n';
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
