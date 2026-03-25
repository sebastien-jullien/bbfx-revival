#include "platform.h"
#include "core/Engine.h"
#include "core/Animator.h"
#include "bindings/bbfx_bindings.h"
#include <ogre_lua/ogre_lua.h>

#include <sol/sol.hpp>
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        sol::state lua;
        lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string,
                           sol::lib::table, sol::lib::io, sol::lib::os,
                           sol::lib::package);

        // Register ogre-lua bindings (Ogre.Vector3, etc.)
        ogre_lua::register_all(lua);

        // Register bbfx bindings (bbfx.Engine, bbfx.Animator, etc.)
        bbfx::register_bbfx_bindings(lua);

        // Create core singletons
        bbfx::Animator animator;
        bbfx::RootTimeNode timeNode;
        bbfx::Engine engine(lua);

        // Run Lua script if provided
        if (argc > 1) {
            auto result = lua.safe_script_file(argv[1], sol::script_pass_on_error);
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
