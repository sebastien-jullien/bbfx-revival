cmake_minimum_required(VERSION 3.20)
project(BBFx VERSION 3.2.0 LANGUAGES CXX)

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

# ── Dear ImGui (docking branch) ──────────────────────────────────────────────
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6-docking
)
FetchContent_MakeAvailable(imgui)

# Build imgui as a static library (core + backends used by bbfx-studio)
add_library(imgui_studio STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_studio PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_studio PUBLIC SDL3::SDL3)
if(WIN32)
    target_link_libraries(imgui_studio PUBLIC opengl32)
endif()

# ── imgui-node-editor (thedmd) ────────────────────────────────────────────────
# imgui-node-editor requires imgui headers; point it to our FetchContent copy.
set(IMGUI_DIR ${imgui_SOURCE_DIR} CACHE PATH "ImGui source directory for imgui-node-editor" FORCE)

FetchContent_Declare(imgui_node_editor
    GIT_REPOSITORY https://github.com/thedmd/imgui-node-editor.git
    GIT_TAG        develop
)
FetchContent_MakeAvailable(imgui_node_editor)

# imgui-node-editor creates a target named 'imgui-node-editor'
# Ensure it can find imgui headers (CMakeLists in the library uses IMGUI_DIR)
if(TARGET imgui-node-editor)
    target_include_directories(imgui-node-editor PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
endif()

# ── Dépendances via vcpkg ──────────────────────────────────────────────────────
find_package(OGRE  REQUIRED COMPONENTS Overlay)
find_package(SDL3  REQUIRED CONFIG)
find_package(sol2  REQUIRED CONFIG)
find_package(Lua   REQUIRED)
find_package(Boost REQUIRED COMPONENTS graph)
find_package(Ogg REQUIRED)
find_package(nlohmann_json REQUIRED)
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
    src/fx/ShaderFxNode.cpp
    src/bindings/bbfx_bindings.cpp
    src/core/ParamSpec.cpp
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
    src/record/InputRecorder.cpp
    src/record/InputPlayer.cpp
    src/record/VideoExporter.cpp
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

# ── Exécutable principal (headless) ───────────────────────────────────────────
add_executable(bbfx src/main.cpp)
target_link_libraries(bbfx PRIVATE bbfx-core)

# ── Windows-specific link libraries ──────────────────────────────────────────
if(WIN32)
    target_link_libraries(bbfx-core PUBLIC xinput winmm imm32 ws2_32)
endif()

# ── BBFx Studio (ImGui GUI application) ───────────────────────────────────────
add_executable(bbfx-studio
    src/studio/main_studio.cpp
    src/studio/StudioApp.cpp
    src/studio/StudioEngine.cpp
    src/studio/NodeTypeRegistry.cpp
    src/studio/SettingsManager.cpp
    src/studio/ResourceEnumerator.cpp
    src/studio/Debugger.cpp
    src/studio/generators/MeshGenerator.cpp
    src/studio/nodes/SceneObjectNode.cpp
    src/studio/nodes/LightNode.cpp
    src/studio/nodes/ParticleNode.cpp
    src/studio/nodes/CompositorNode.cpp
    src/studio/nodes/CameraNode.cpp
    src/studio/nodes/SkyboxNode.cpp
    src/studio/nodes/FogNode.cpp
    src/studio/nodes/BeatTriggerNode.cpp
    src/studio/nodes/MathNode.cpp
    src/studio/nodes/MixerNode.cpp
    src/studio/nodes/MapperNode.cpp
    src/studio/nodes/TriggerNode.cpp
    src/studio/nodes/SplitterNode.cpp
    src/studio/commands/CommandManager.cpp
    src/studio/commands/NodeCommands.cpp
    src/studio/commands/LinkCommands.cpp
    src/studio/commands/EditCommands.cpp
    src/studio/commands/ChordCommands.cpp
    src/studio/panels/ViewportPanel.cpp
    src/studio/panels/NodeEditorPanel.cpp
    src/studio/panels/InspectorPanel.cpp
    src/studio/panels/TimelinePanel.cpp
    src/studio/panels/PresetBrowserPanel.cpp
    src/studio/panels/PerformanceModePanel.cpp
    src/studio/panels/ConsolePanel.cpp
    src/studio/panels/SetEditorPanel.cpp
    src/studio/project/ProjectSerializer.cpp
    src/studio/project/ExportDialog.cpp
    # imgui-node-editor (compiled directly — FetchContent target unreliable)
    ${imgui_node_editor_SOURCE_DIR}/imgui_node_editor.cpp
    ${imgui_node_editor_SOURCE_DIR}/imgui_node_editor_api.cpp
    ${imgui_node_editor_SOURCE_DIR}/imgui_canvas.cpp
    ${imgui_node_editor_SOURCE_DIR}/crude_json.cpp
)
target_include_directories(bbfx-studio PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_node_editor_SOURCE_DIR}
)
target_compile_definitions(bbfx-studio PRIVATE IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(bbfx-studio PRIVATE
    bbfx-core
    imgui_studio
    nlohmann_json::nlohmann_json
)
if(WIN32)
    target_link_libraries(bbfx-studio PRIVATE opengl32 comdlg32)
    set_target_properties(bbfx-studio PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

# ── Copy OGRE plugin DLLs next to executables (Windows) ──────────────────────
if(WIN32)
    add_custom_command(TARGET bbfx-studio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${OGRE_PLUGIN_DIR}"
            "$<TARGET_FILE_DIR:bbfx-studio>"
        COMMENT "Copying OGRE release plugins to bbfx-studio output directory"
    )
    get_filename_component(_ogre_prefix2 "${OGRE_PLUGIN_DIR}" DIRECTORY)
    get_filename_component(_ogre_prefix2 "${_ogre_prefix2}" DIRECTORY)
    set(_ogre_debug_plugins2 "${_ogre_prefix2}/debug/plugins/ogre")
    if(EXISTS "${_ogre_debug_plugins2}")
        add_custom_command(TARGET bbfx-studio POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${_ogre_debug_plugins2}"
                "$<TARGET_FILE_DIR:bbfx-studio>"
            COMMENT "Copying OGRE debug plugins to bbfx-studio output directory"
        )
    endif()
endif()

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

# ── Copy scripts and resources next to bbfx-studio ───────────────────────────
add_custom_command(TARGET bbfx-studio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/lua"
        "$<TARGET_FILE_DIR:bbfx-studio>/lua"
    COMMENT "Copying Lua scripts to bbfx-studio"
)
add_custom_command(TARGET bbfx-studio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:bbfx-studio>/resources"
    COMMENT "Copying resources to bbfx-studio"
)
add_custom_command(TARGET bbfx-studio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/resources.cfg"
        "$<TARGET_FILE_DIR:bbfx-studio>/resources.cfg"
    COMMENT "Copying resources.cfg to bbfx-studio"
)
add_custom_command(TARGET bbfx-studio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:bbfx-studio>/data"
    COMMENT "Copying data (templates) to bbfx-studio"
)

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

# ── CPack — Windows NSIS installer ────────────────────────────────────────────
set(CPACK_PACKAGE_NAME              "BBFx-Studio")
set(CPACK_PACKAGE_VENDOR            "Sébastien JULLIEN")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BBFx Studio — Real-time 3D Animation Engine")
set(CPACK_PACKAGE_VERSION           "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR     "3")
set(CPACK_PACKAGE_VERSION_MINOR     "1")
set(CPACK_PACKAGE_VERSION_PATCH     "0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "BBFx-Studio")
set(CPACK_RESOURCE_FILE_LICENSE     "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME       "BBFx Studio ${PROJECT_VERSION}")
    set(CPACK_NSIS_PACKAGE_NAME       "BBFx Studio")
    set(CPACK_NSIS_MUI_ICON           "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico")
    set(CPACK_NSIS_MUI_UNIICON        "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\BBFx Studio.lnk' '$INSTDIR\\\\bin\\\\bbfx-studio.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$MUI_TEMP\\\\BBFx Studio.lnk'")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    # .bbfx-project file association
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
        WriteRegStr HKCR '.bbfx-project' '' 'BBFxStudio.Project'
        WriteRegStr HKCR 'BBFxStudio.Project' '' 'BBFx Studio Project'
        WriteRegStr HKCR 'BBFxStudio.Project\\\\shell\\\\open\\\\command' '' '\\\"$INSTDIR\\\\bin\\\\bbfx-studio.exe\\\" \\\"%1\\\"'
    ")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
        DeleteRegKey HKCR '.bbfx-project'
        DeleteRegKey HKCR 'BBFxStudio.Project'
    ")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

# Install rules for packaging
install(TARGETS bbfx-studio RUNTIME DESTINATION bin)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lua"       DESTINATION share/bbfx)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources"  DESTINATION share/bbfx)
install(FILES     "${CMAKE_CURRENT_SOURCE_DIR}/resources.cfg" DESTINATION bin)
install(FILES     "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"    DESTINATION .)

include(CPack)

# ── Informations de build ──────────────────────────────────────────────────────
message(STATUS "BBFx v${PROJECT_VERSION} — C++${CMAKE_CXX_STANDARD}")
message(STATUS "OGRE   : ${OGRE_VERSION}")
message(STATUS "Lua    : ${LUA_VERSION_STRING}")
message(STATUS "OGRE plugins : ${OGRE_PLUGIN_DIR}")
