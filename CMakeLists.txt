cmake_minimum_required(VERSION 3.20)
project(BBFx VERSION 3.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Renderer selection ──────────────────────────────────────────────────────
# Default: OpenGL 3+ (cross-platform). Use -DBBFX_USE_D3D11=ON for Direct3D11.
option(BBFX_USE_D3D11 "Use Direct3D11 renderer instead of OpenGL 3+" OFF)

# ── Optional feature flags (v3.4) ───────────────────────────────────────────
option(BBFX_HAS_SPOUT  "Enable Spout output (Windows, requires Spout SDK)" OFF)
option(BBFX_HAS_DMABUF "Enable DMA-BUF texture sharing (Linux, requires EGL + GBM)" OFF)
if(BBFX_HAS_SPOUT)
    message(STATUS "BBFx: Spout output enabled — link Spout SDK manually")
    # target_include_directories(bbfx-studio PRIVATE <path-to-spout-sdk>)
    # target_link_libraries(bbfx-studio PRIVATE <spout-lib>)
else()
    message(STATUS "BBFx: Spout output disabled (BBFX_HAS_SPOUT=OFF)")
endif()
if(BBFX_HAS_DMABUF)
    message(STATUS "BBFx: DMA-BUF texture sharing enabled")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(EGL REQUIRED egl)
    pkg_check_modules(GBM REQUIRED gbm)
else()
    message(STATUS "BBFx: DMA-BUF texture sharing disabled (BBFX_HAS_DMABUF=OFF)")
endif()
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
find_package(rtmidi CONFIG REQUIRED)

# v3.5 Lot E/F/R dependencies
find_package(CURL       REQUIRED)                  # HTTP client (Lot E)
find_package(minizip-ng CONFIG REQUIRED)           # ZIP extraction (Lot F)
find_package(asio       CONFIG QUIET)              # async I/O — optional
# v3.5 Lot Q — assimp for 3D model import (OBJ/FBX/glTF/...), stb for
# multi-frame GIF decoding. Both optional: the factories in
# src/media/*.cpp fall back to a Null path when the SDK is absent.
find_package(assimp     CONFIG QUIET)
find_path(STB_INCLUDE_DIR stb_image.h)
# websocketpp and fastnoiselite are not in the current vcpkg baseline.
# - WebSocket: WebSocketClient.cpp ships a clean Unsupported stub; flipping
#   it on requires a baseline bump or vendoring websocketpp headers.
# - Noise: Lot N vendors FastNoiseLite.h under extern/ when it lands.
find_path(FASTNOISELITE_INCLUDE_DIR FastNoiseLite.h)
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
    src/input/GamepadManager.cpp
    src/input/GamepadNode.cpp
    src/input/GamepadMappingProfile.cpp
    src/input/InputManager.cpp
    src/input/StdinReader.cpp
    src/network/TcpServer.cpp
    src/network/HttpClient.cpp
    src/network/WebSocketClient.cpp
    src/network/ZipExtractor.cpp
    # v3.5 Lot M — Art-Net listener singleton (UDP 6454). Core because the
    # Lua bindings in bbfx-core access it directly.
    src/network/ArtnetInput.cpp
    # v3.5 Lot M — UdpServer + OscMessage + OscBus are dependencies of the
    # bbfx.osc.* bindings in bbfx-core. They are pure network code (no
    # Studio dependency) so it's safe to ship them here — bbfx-studio
    # continues to link them transitively via bbfx-core.
    src/network/UdpServer.cpp
    src/osc/OscMessage.cpp
    src/osc/OscBus.cpp
    # v3.5 Lot M — MIDI stack moved to bbfx-core so bbfx_bindings.cpp can
    # reference it from the headless executable as well. rtmidi is linked
    # into bbfx-core a few lines below.
    src/midi/MidiDeviceManager.cpp
    src/midi/MidiLearnManager.cpp
    src/midi/MidiClockGenerator.cpp
    src/midi/MappingProfile.cpp
    # v3.5 Lot M — Texture sharing receiver. OGRE-only dependency, fits in
    # core without pulling ImGui or any Studio infra.
    src/studio/TextureShareReceiver.cpp
    # v3.5 Lot N — noise + tempo + timeline (all pure C++ + OGRE texture)
    src/procedural/NoiseGenerator.cpp
    src/timing/TempoManager.cpp
    src/timing/LuaTimeline.cpp
    # v3.5 Lot R — procedural geometry + SDF + L-system
    src/procedural/GeometryGenerator.cpp
    src/procedural/SDFPrimitives.cpp
    src/procedural/LSystem.cpp
    # MeshGenerator moved to bbfx-core so GeometryGenerator can delegate
    # to its primitive builders without a reverse dependency on
    # bbfx-studio. Pure OGRE + std, no ImGui or Studio infra.
    src/studio/generators/MeshGenerator.cpp
    # v3.5 Lot Q — media pipeline (ffmpeg subprocess + image loader +
    # sequence player + assimp-backed mesh import). All optional — the
    # factories fall back to an inert handle when the backend is absent.
    src/media/FFmpegBridge.cpp
    src/media/ImageLoader.cpp
    src/media/SequencePlayer.cpp
    src/media/MeshImporter.cpp
    # Dedicated TU for the stb_image implementation (see comments inside).
    src/media/stb_image_impl.cpp
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
    src/plugin/PluginManifest.cpp
    src/plugin/PluginValidator.cpp
    src/plugin/PluginManager.cpp
    src/plugin/PluginRegistry.cpp
    src/plugin/PluginSandbox.cpp
    src/plugin/PluginSandboxApi.cpp
    src/plugin/InspectorWidgetRegistry.cpp
    src/plugin/PluginErrorLog.cpp
    src/plugin/CommunityIndex.cpp
    src/plugin/DeepLinkHandler.cpp
    src/plugin/DeepLinkRegistry.cpp
    src/plugin/GithubReactionsFetcher.cpp
    src/plugin/PluginAuthoringBackend.cpp
    src/plugin/PluginHotReloader.cpp
    src/plugin/GitHubPublisher.cpp
    # ResourceEnumerator is used by PluginManager::load/unload to invalidate
    # the AssetBrowser cache when plugins come or go. Pure OGRE dependency,
    # fits naturally in core alongside NodeTypeRegistry.
    src/studio/ResourceEnumerator.cpp
    # SettingsManager is used by PluginManager to persist enabled plugins
    # across sessions (auto-enable at startup). Pure nlohmann/json dep,
    # already available in core.
    src/studio/SettingsManager.cpp
    # SceneObjectNode is referenced by PerlinFxNode + WaveVertexShader (both
    # in bbfx-core) via SceneObjectNode::isNodeVisible(). It has no Studio-
    # specific dependencies (only AnimationNode/Ogre headers), so it lives
    # in core even though the file sits under src/studio/nodes/ for
    # historical reasons.
    src/studio/nodes/SceneObjectNode.cpp
    # NodeTypeRegistry is needed in core because the plugin sandbox API
    # (src/plugin/PluginSandboxApi.cpp) has to register custom node types
    # even when the headless engine is used. It only depends on ImGui for
    # the ImVec4 colour field — imgui is header-only so core just needs the
    # include path (target_include_directories below).
    src/studio/NodeTypeRegistry.cpp
)

target_include_directories(bbfx-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LUA_INCLUDE_DIR}
    # NodeTypeRegistry.h uses ImVec4 from imgui.h (header-only); expose the
    # include path so bbfx-core compiles without linking imgui_studio.
    ${imgui_SOURCE_DIR}
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
    nlohmann_json::nlohmann_json
    CURL::libcurl
    MINIZIP::minizip-ng
    # v3.5 Lot M — MIDI now lives in bbfx-core (bindings reference it).
    RtMidi::rtmidi
)
if(TARGET asio::asio)
    target_link_libraries(bbfx-core PUBLIC asio::asio)
endif()
if(FASTNOISELITE_INCLUDE_DIR)
    target_include_directories(bbfx-core PUBLIC ${FASTNOISELITE_INCLUDE_DIR})
endif()
# v3.5 Lot Q — optional assimp + stb
if(TARGET assimp::assimp)
    target_link_libraries(bbfx-core PUBLIC assimp::assimp)
    target_compile_definitions(bbfx-core PUBLIC BBFX_HAS_ASSIMP)
    message(STATUS "BBFx: assimp enabled (3D model import)")
else()
    message(STATUS "BBFx: assimp disabled (3D model import = Null fallback)")
endif()
if(STB_INCLUDE_DIR)
    target_include_directories(bbfx-core PUBLIC ${STB_INCLUDE_DIR})
    target_compile_definitions(bbfx-core PUBLIC BBFX_HAS_STB)
    message(STATUS "BBFx: stb enabled (GIF decode)")
else()
    message(STATUS "BBFx: stb disabled (GIF decode = Null fallback)")
endif()
if(THEORA_LIBRARY AND THEORADEC_LIBRARY)
    target_link_libraries(bbfx-core PUBLIC ${THEORA_LIBRARY} ${THEORADEC_LIBRARY})
    target_include_directories(bbfx-core PRIVATE ${THEORA_INCLUDE_DIR})
endif()

# ── Exécutable principal (headless) ───────────────────────────────────────────
add_executable(bbfx src/main.cpp)
target_link_libraries(bbfx PRIVATE bbfx-core)

# ── Windows-specific link libraries ──────────────────────────────────────────
if(WIN32)
    target_link_libraries(bbfx-core PUBLIC xinput winmm imm32 ws2_32 shell32 ole32)
endif()

# ── BBFx Studio (ImGui GUI application) ───────────────────────────────────────
add_executable(bbfx-studio
    src/studio/main_studio.cpp
    src/studio/StudioApp.cpp
    src/studio/StudioEngine.cpp
    # v3.5 Lot P — ImGui Lua bindings + script-panel registry
    src/studio/bbfx_imgui_bindings.cpp
    src/studio/ScriptPanelRegistry.cpp
    # NodeTypeRegistry.cpp + ResourceEnumerator.cpp + SettingsManager.cpp moved to bbfx-core above
    src/studio/TextureThumbnailCache.cpp
    src/studio/ShaderPreviewRenderer.cpp
    src/studio/DagSnapshot.cpp
    src/studio/ToastSystem.cpp
    src/studio/dialogs/PermissionPromptDialog.cpp
    src/studio/panels/PluginManagerPanel.cpp
    src/studio/panels/PluginErrorsPanel.cpp
    src/studio/panels/CommunityBrowserPanel.cpp
    src/studio/panels/CommandPalette.cpp
    src/studio/panels/AuthorProfilePanel.cpp
    src/studio/panels/GamepadPanel.cpp
    src/studio/panels/PluginAuthoringDialog.cpp
    src/studio/MarkdownRenderer.cpp
    # MIDI stack + OscMessage.cpp + UdpServer.cpp moved to bbfx-core
    # (v3.5 Lot M).
    src/studio/nodes/MidiInputNode.cpp
    src/studio/nodes/MidiOutputNode.cpp
    src/studio/nodes/OscInputNode.cpp
    src/studio/nodes/OscOutputNode.cpp
    src/studio/nodes/NdiOutputNode.cpp
    src/studio/Debugger.cpp
    # MeshGenerator.cpp moved to bbfx-core (v3.5 Lot R).
    # SceneObjectNode.cpp moved to bbfx-core above
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
    src/studio/commands/PluginCommands.cpp
    src/studio/panels/ViewportPanel.cpp
    src/studio/panels/NodeEditorPanel.cpp
    src/studio/panels/InspectorPanel.cpp
    src/studio/panels/TimelinePanel.cpp
    src/studio/panels/PresetBrowserPanel.cpp
    src/studio/panels/ShaderGalleryPanel.cpp
    src/studio/panels/MaterialEditorPanel.cpp
    src/studio/panels/UndoHistoryPanel.cpp
    src/studio/panels/MidiActivityPanel.cpp
    src/studio/panels/MidiMappingPanel.cpp
    src/studio/panels/OutputManagerPanel.cpp
    src/studio/OutputManager.cpp
    src/studio/WarpProfile.cpp
    src/studio/BlendProfile.cpp
    src/studio/GridWarpProfile.cpp
    src/studio/WarpWizard.cpp
    src/studio/SurfaceMap.cpp
    src/studio/ZoneSnapshot.cpp
    src/studio/panels/SurfaceEditorPanel.cpp
    src/studio/panels/NetworkPanel.cpp
    src/studio/panels/MasterViewPanel.cpp
    src/studio/TextureShareSender.cpp
    src/studio/SpoutTextureSender.cpp
    src/studio/DmaBufTextureSender.cpp
    # TextureShareReceiver.cpp moved to bbfx-core (v3.5 Lot M).
    src/studio/nodes/TextureShareOutputNode.cpp
    src/studio/nodes/ArtnetOutputNode.cpp
    src/studio/nodes/ArtnetInputNode.cpp
    src/network/SyncManager.cpp
    src/studio/nodes/WarpNode.cpp
    src/studio/nodes/BlendNode.cpp
    src/studio/panels/OscBrowserPanel.cpp
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
if(BBFX_HAS_SPOUT)
    target_compile_definitions(bbfx-studio PRIVATE BBFX_HAS_SPOUT)
endif()
if(BBFX_HAS_DMABUF)
    target_compile_definitions(bbfx-studio PRIVATE BBFX_HAS_DMABUF)
    target_link_libraries(bbfx-studio PRIVATE ${EGL_LIBRARIES} ${GBM_LIBRARIES})
    target_include_directories(bbfx-studio PRIVATE ${EGL_INCLUDE_DIRS} ${GBM_INCLUDE_DIRS})
endif()
target_link_libraries(bbfx-studio PRIVATE
    bbfx-core
    imgui_studio
    nlohmann_json::nlohmann_json
    RtMidi::rtmidi
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
set(CPACK_PACKAGE_VERSION_MINOR     "3")
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
