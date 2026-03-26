cmake_minimum_required(VERSION 3.20)
project(BBFx VERSION 2.7.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── MSVC: /bigobj for sol2 heavy template instantiation ──────────────────────
if(MSVC)
    add_compile_options(/bigobj)
endif()

# ── ogre-lua dependency (fetched from GitHub) ────────────────────────────────
# Override with: cmake -DFETCHCONTENT_SOURCE_DIR_OGRE-LUA=/path/to/local/ogre-lua
include(FetchContent)
FetchContent_Declare(ogre-lua
    GIT_REPOSITORY https://github.com/sebastien-jullien/ogre-lua.git
    GIT_TAG        v0.1.0-alpha
)
FetchContent_MakeAvailable(ogre-lua)

# ── Dépendances via vcpkg ──────────────────────────────────────────────────────
find_package(OGRE  REQUIRED COMPONENTS Overlay)
find_package(SDL3  REQUIRED CONFIG)
find_package(sol2  REQUIRED CONFIG)
find_package(Lua   REQUIRED)
find_package(Boost REQUIRED COMPONENTS graph)
find_package(Ogg REQUIRED)
find_package(PkgConfig QUIET)
# libtheora doesn't have a CMake config — use manual find
find_path(THEORA_INCLUDE_DIR theora/theoradec.h)
find_library(THEORA_LIBRARY NAMES theora)
find_library(THEORADEC_LIBRARY NAMES theoradec)

# ── Core library ─────────────────────────────────────────────────────────────
add_library(bbfx-core
    src/core/AnimationPort.cpp
    src/core/AnimationNode.cpp
    src/core/Animator.cpp
    src/core/PrimitiveNodes.cpp
    src/core/Engine.cpp
    src/core/StatsOverlay.cpp
    src/fx/SoftwareVertexShader.cpp
    src/fx/PerlinVertexShader.cpp
    src/fx/PerlinFxNode.cpp
    src/fx/TextureBlitter.cpp
    src/fx/TextureBlitterNode.cpp
    src/fx/WaveVertexShader.cpp
    src/fx/ColorShiftNode.cpp
    src/bindings/bbfx_bindings.cpp
    src/input/KeyboardManager.cpp
    src/input/MouseManager.cpp
    src/input/JoystickManager.cpp
    src/input/InputManager.cpp
    src/input/StdinReader.cpp
    src/network/TcpServer.cpp
    src/audio/AudioCapture.cpp
    src/audio/AudioAnalyzer.cpp
    src/audio/BeatDetector.cpp
    src/video/OggReader.cpp
    src/video/TheoraSeekMap.cpp
    src/video/TheoraReader.cpp
    src/video/TheoraBlitter.cpp
    src/video/TheoraClip.cpp
    src/video/ReversableClip.cpp
    src/video/TextureCrossfader.cpp
    src/video/TheoraClipNode.cpp
)

target_include_directories(bbfx-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LUA_INCLUDE_DIR}
)

target_link_libraries(bbfx-core PUBLIC
    OgreMain
    OgreOverlay
    OgreRTShaderSystem
    SDL3::SDL3
    sol2::sol2
    ${LUA_LIBRARIES}
    Boost::graph
    ogre-lua
    Ogg::ogg
)
if(THEORA_LIBRARY AND THEORADEC_LIBRARY)
    target_link_libraries(bbfx-core PUBLIC ${THEORA_LIBRARY} ${THEORADEC_LIBRARY})
    target_include_directories(bbfx-core PRIVATE ${THEORA_INCLUDE_DIR})
endif()

# ── Exécutable principal ───────────────────────────────────────────────────────
add_executable(bbfx src/main.cpp)
target_link_libraries(bbfx PRIVATE bbfx-core)

# ── Windows-specific link libraries ──────────────────────────────────────────
if(WIN32)
    target_link_libraries(bbfx-core PUBLIC xinput winmm imm32 ws2_32)
endif()

# ── Copy OGRE plugin DLLs next to executable (Windows) ───────────────────────
if(WIN32)
    add_custom_command(TARGET bbfx POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${OGRE_PLUGIN_DIR}"
            "$<TARGET_FILE_DIR:bbfx>"
        COMMENT "Copying OGRE release plugins to output directory"
    )
    get_filename_component(_ogre_prefix "${OGRE_PLUGIN_DIR}" DIRECTORY)
    get_filename_component(_ogre_prefix "${_ogre_prefix}" DIRECTORY)
    set(_ogre_debug_plugins "${_ogre_prefix}/debug/plugins/ogre")
    if(EXISTS "${_ogre_debug_plugins}")
        add_custom_command(TARGET bbfx POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${_ogre_debug_plugins}"
                "$<TARGET_FILE_DIR:bbfx>"
            COMMENT "Copying OGRE debug plugins to output directory"
        )
    endif()
endif()

# ── Copy scripts and resources next to executable ────────────────────────────
add_custom_command(TARGET bbfx POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/lua"
        "$<TARGET_FILE_DIR:bbfx>/lua"
    COMMENT "Copying Lua scripts"
)
add_custom_command(TARGET bbfx POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
        "$<TARGET_FILE_DIR:bbfx>/tests"
    COMMENT "Copying test scripts"
)
add_custom_command(TARGET bbfx POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/particles"
        "$<TARGET_FILE_DIR:bbfx>/resources/particles"
    COMMENT "Copying particle scripts"
)
add_custom_command(TARGET bbfx POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/resources.cfg"
        "$<TARGET_FILE_DIR:bbfx>/resources.cfg"
    COMMENT "Copying resources.cfg"
)
add_custom_command(TARGET bbfx POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:bbfx>/resources"
    COMMENT "Copying OGRE resources (models, materials, textures)"
)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()

add_test(
    NAME test_regression
    COMMAND $<TARGET_FILE:test_runner> --headless
        tests/lua/test_regression.lua
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:test_runner>"
)

# ── Informations de build ──────────────────────────────────────────────────────
message(STATUS "BBFx v${PROJECT_VERSION} — C++${CMAKE_CXX_STANDARD}")
message(STATUS "OGRE   : ${OGRE_VERSION}")
message(STATUS "Lua    : ${LUA_VERSION_STRING}")
message(STATUS "OGRE plugins : ${OGRE_PLUGIN_DIR}")
