#include "StudioApp.h"
#include "../platform.h"
#include "../core/Animator.h"
#include "../core/PrimitiveNodes.h"
#include "../bindings/bbfx_bindings.h"
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

        // Optional initial Lua script (e.g. to pre-populate DAG)
        std::string initialScript;
        if (argc > 1) {
            initialScript = argv[1];
        }

        // Run the Studio application
        bbfx::StudioApp app(lua, initialScript);
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
