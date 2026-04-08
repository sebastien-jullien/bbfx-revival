cmake_minimum_required(VERSION 3.20)
project(BBFx VERSION 3.2.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Renderer selection ──────────────────────────────────────────────────────
# Default: OpenGL 3+ (cross-platform). Use -DBBFX_USE_D3D11=ON for Direct3D11.
option(BBFX_USE_D3D11 "Use Direct3D11 renderer instead of OpenGL 3+" OFF)
if(BBFX_USE_D3D11)
    add_compile_definitions(BBFX_USE_D3D11)
    message(STATUS "BBFx: Using Direct3D11 renderer")
else()
    message(STATUS "BBFx: Using OpenGL 3+ renderer (default)")
endif()

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
    GIT_TAG        v1.92.7-docking
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

# ── Dear ImGui Test Engine ────────────────────────────────────────────────────
FetchContent_Declare(imgui_test_engine
    GIT_REPOSITORY https://github.com/ocornut/imgui_test_engine.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(imgui_test_engine)

# Add test engine sources to imgui_studio library
target_sources(imgui_studio PRIVATE
    ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine/imgui_te_engine.cpp
    ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine/imgui_te_context.cpp
    ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine/imgui_te_coroutine.cpp
    ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine/imgui_te_exporters.cpp
    ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine/imgui_te_perftool.cpp
    ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine/imgui_te_utils.cpp
    ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine/imgui_capture_tool.cpp
    ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine/imgui_te_ui.cpp
)
target_include_directories(imgui_studio PUBLIC
    ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine
)
target_compile_definitions(imgui_studio PUBLIC
    IMGUI_ENABLE_TEST_ENGINE
    IMGUI_TEST_ENGINE_ENABLE_STD_FUNCTION
    IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL=1
)

# ── imgui-node-editor (thedmd) ────────────────────────────────────────────────
# imgui-node-editor requires imgui headers; point it to our FetchContent copy.
set(IMGUI_DIR ${imgui_SOURCE_DIR} CACHE PATH "ImGui source directory for imgui-node-editor" FORCE)

FetchContent_Declare(imgui_node_editor
    GIT_REPOSITORY https://github.com/thedmd/imgui-node-editor.git
    GIT_TAG        develop
)
FetchContent_MakeAvailable(imgui_node_editor)

# ── Patches imgui-node-editor for ImGui v1.92 compatibility ───────────────────
# 1) operator*(float, ImVec2) now defined natively in ImGui v1.92 — guard redefinition
file(READ "${imgui_node_editor_SOURCE_DIR}/imgui_extra_math.inl" _ne_math)
string(REPLACE
    "inline ImVec2 operator*(const float lhs, const ImVec2& rhs)\n{\n    return ImVec2(lhs * rhs.x, lhs * rhs.y);\n}"
    "# if IMGUI_VERSION_NUM < 19200\ninline ImVec2 operator*(const float lhs, const ImVec2& rhs)\n{\n    return ImVec2(lhs * rhs.x, lhs * rhs.y);\n}\n# endif"
    _ne_math "${_ne_math}")
file(WRITE "${imgui_node_editor_SOURCE_DIR}/imgui_extra_math.inl" "${_ne_math}")

# 2) ImRect::Floor() removed in v1.92 — replace 2 in-place calls with manual floor
file(READ "${imgui_node_editor_SOURCE_DIR}/imgui_node_editor.cpp" _ne_cpp)
string(REPLACE "newBounds.Floor();"
    "newBounds.Min = ImFloor(newBounds.Min); newBounds.Max = ImFloor(newBounds.Max);" _ne_cpp "${_ne_cpp}")
string(REPLACE "m_NodeRect.Floor();"
    "m_NodeRect.Min = ImFloor(m_NodeRect.Min); m_NodeRect.Max = ImFloor(m_NodeRect.Max);" _ne_cpp "${_ne_cpp}")
string(REPLACE "node->m_Bounds.Floor();"
    "node->m_Bounds.Min = ImFloor(node->m_Bounds.Min); node->m_Bounds.Max = ImFloor(node->m_Bounds.Max);" _ne_cpp "${_ne_cpp}")
string(REPLACE "node->m_GroupBounds.Floor();"
    "node->m_GroupBounds.Min = ImFloor(node->m_GroupBounds.Min); node->m_GroupBounds.Max = ImFloor(node->m_GroupBounds.Max);" _ne_cpp "${_ne_cpp}")
string(REPLACE "m_CurrentPin->m_Bounds.Floor();"
    "m_CurrentPin->m_Bounds.Min = ImFloor(m_CurrentPin->m_Bounds.Min); m_CurrentPin->m_Bounds.Max = ImFloor(m_CurrentPin->m_Bounds.Max);" _ne_cpp "${_ne_cpp}")
string(REPLACE "m_GroupBounds.Floor();"
    "m_GroupBounds.Min = ImFloor(m_GroupBounds.Min); m_GroupBounds.Max = ImFloor(m_GroupBounds.Max);" _ne_cpp "${_ne_cpp}")
file(WRITE "${imgui_node_editor_SOURCE_DIR}/imgui_node_editor.cpp" "${_ne_cpp}")

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
    src/core/AutomationData.cpp
    src/core/AutomationEngine.cpp
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
    src/studio/TextureThumbnailCache.cpp
    src/studio/ShaderPreviewRenderer.cpp
    src/studio/DagSnapshot.cpp
    src/studio/ToastSystem.cpp
    src/studio/Debugger.cpp
    src/studio/generators/MeshGenerator.cpp
    src/studio/nodes/SceneObjectNode.cpp
    src/studio/nodes/LightNode.cpp
    src/studio/nodes/ParticleNode.cpp
    src/studio/nodes/CompositorNode.cpp
    src/studio/nodes/TextureNode.cpp
    src/studio/nodes/MaterialNode.cpp
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
    src/studio/panels/ShaderGalleryPanel.cpp
    src/studio/panels/MaterialEditorPanel.cpp
    src/studio/panels/UndoHistoryPanel.cpp
    src/studio/panels/PerformanceModePanel.cpp
    src/studio/panels/ConsolePanel.cpp
    src/studio/panels/SetEditorPanel.cpp
    src/studio/panels/CompositorStackPanel.cpp
    src/studio/viewport/ViewportCameraController.cpp
    src/studio/viewport/ViewportPicker.cpp
    src/studio/viewport/ViewportGrid.cpp
    src/studio/viewport/ViewportGizmo.cpp
    src/studio/viewport/ViewportToolbar.cpp
    src/studio/commands/TransformCommands.cpp
    src/studio/commands/SceneCommands.cpp
    src/studio/panels/SceneHierarchyPanel.cpp
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
