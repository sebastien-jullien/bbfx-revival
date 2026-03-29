# Changelog

All notable changes to BBFx Revival are documented in this file.

## [3.1.0] - 2026-03-28 — BBFx Studio++

### Added
- **NodeTypeRegistry**: extensible node type registry with categorized context menu (17+ types in 7 categories)
- **CommandManager**: undo/redo system (Command pattern, stack 100) with Ctrl+Z/Ctrl+Y
- **Commands**: CreateNode, DeleteNode, CreateLink, DeleteLink, EditPortValue, RenameNode, AddChord, DeleteChord, RenameChord, ResizeChord
- **ConsolePanel**: integrated Lua REPL with autocompletion, command history, error display
- **SettingsManager**: persistent settings (auto-save interval, BPM, viewport scale, font size) in %APPDATA%/BBFx/settings.json
- **StudioEngine::captureFrame()**: PNG frame capture via OGRE RenderTexture::writeContentsToFile()
- **LuaAnimationNode::setUpdateFunction()**: hot-swap Lua callback at runtime
- **Animator::renameNode()**: rename nodes with automatic port fullname updates
- **Timeline zoom/scroll/seek**: mouse wheel zoom (10-200 px/beat), Ctrl+drag scroll, click-to-seek
- **Chord editing**: add (+), rename, recolor (ColorPicker), delete, resize by edge drag with beat snap
- **Node duplication**: Ctrl+D single/multi with internal link preservation
- **Flow animation**: animated dots on links via ned::Flow()
- **Bookmarks**: Ctrl+1..9 save / 1..9 restore canvas position/zoom
- **Drag & drop**: SDL_EVENT_DROP_FILE for .bbfx-project, .lua, .glsl, .ogg files
- **Help > About**: version, credits dialog
- **Help > Keyboard Shortcuts**: complete shortcut reference
- **Status bar**: FPS, node count, link count, audio status, dirty indicator
- **lua/studio_chord.lua**: name-based chord activation API for Studio GUI
- Lua-only node types in registry (LFONode, RampNode, DelayNode, EnvelopeFollowerNode, BandSplitNode, SubgraphNode)

### Changed
- **ExportDialog**: now calls captureFrame() instead of logging — export actually produces PNG files
- **InspectorPanel**: Lua editor "Apply" button compiles and hot-swaps callback; rename field calls Animator::renameNode()
- **PerformanceModePanel**: triggers connected to ChordSystem, faders connected to DAG ports, VU meters read AudioAnalyzerNode
- **PresetBrowserPanel**: Quick Access buttons activate chords or instantiate presets
- **NodeEditorPanel**: context menu uses NodeTypeRegistry categories, all mutations via CommandManager
- **ProjectSerializer**: format v3.1 with node positions, chords, timeline, performance config, media; backward compatible with v3.0
- **CMakeLists.txt**: version 3.1.0, 8 new source files

### Fixed
- **Export stub**: ExportDialog::tickExport() was logging instead of capturing frames
- **Lua editor stub**: InspectorPanel "Apply" button was not connected to sol2 compilation
- **Rename stub**: InspectorPanel rename field was only updating UI, not the DAG
- **Performance triggers stub**: buttons were logging instead of activating chord states
- **VU meters stub**: were displaying static data instead of reading AudioAnalyzerNode
- **Quick Access stub**: buttons were logging instead of triggering actions
- **Node positions lost**: positions now saved/loaded in .bbfx-project via GetNodePosition/SetNodePosition
- **Limited node creation**: menu now offers 17+ types instead of 2

## [3.0.0] - 2026-03-28 — BBFx Studio

### Added
- **BBFx Studio** (`bbfx-studio`): GUI application with Dear ImGui (docking branch) + imgui-node-editor
- **StudioEngine**: OGRE renders off-screen to `RenderTexture`, shared SDL3/OpenGL context with ImGui
- **Viewport Panel**: live OGRE render with FPS/resolution/mode overlay, dynamic resize
- **Node Editor Panel**: interactive DAG (imgui-node-editor), link create/delete, node context menu, color by type, selection sync
- **Inspector Panel**: float sliders, enum dropdowns (waveform/interpolation/mode), Lua source editor with error display, ShaderFx uniforms, rename/delete
- **Timeline Panel**: beat/bar markers, animated playhead, draggable chord blocks (snap-to-beat), transport (Play/Pause/Stop/Record), BPM, 8-band spectrum
- **Preset Browser Panel**: filesystem scan `lua/presets/`, drag-to-graph, effect rack with bypass, 8-slot quick access bar
- **Performance Mode (F5)**: fullscreen viewport 80%, 4x4 trigger grid, 8 faders, VU meters, BPM overlay, PANIC button, keyboard navigation
- **ProjectSerializer**: `.bbfx-project` JSON save/load (atomic write), auto-save 120s, recent projects
- **ExportDialog**: frame-by-frame PNG export with progress bar, offline rendering
- **CPack NSIS**: Windows installer with shortcuts and `.bbfx-project` file association
- **Dark theme**: #1A1A1A background, cyan #00FFFF accents, auto-dock layout on first launch
- `AnimationNode::getTypeName()` virtual method on all node subclasses
- `Engine` two-phase init for StudioEngine inheritance
- `lua/demos/demo_studio.lua`: default studio scene (ogrehead + rotation)
- `lua/presets/perlin_pulse.lua`: sample preset file
- Launcher script `./bbfx-studio [--build] [script.lua]`

### Fixed
- **OGRE GL3Plus FBO cache bug**: `mActiveRenderTarget` cache causes OGRE to skip FBO re-bind after ImGui renders to framebuffer 0 between frames. Fix: capture FBO ID on first render, force `glBindFramebuffer()` before each `rt->update()`
- **Node Editor links not visible**: `syncLinksFromDAG()` rebuilds `mLinks` from `Animator::getLinks()` each frame so DAG edges created via Lua/REPL appear in the editor
- **Link creation fires during drag**: guarded DAG mutation behind `AcceptNewItem()` return value (true only on mouse release)
- **Right-click link deletion**: added `ShowLinkContextMenu()` with "Delete Link" option that calls `Animator::unlink()`
- **Oscillator min/max**: added `min` and `max` input ports to oscillator node (output remapped to `[min, max]` range)
- **rotate_head speed port**: added `speed` input, set `angle` output, continuous rotation driven by `time * speed`
- **ViewportPanel resize race**: separated size detection (ImGui frame) from OGRE render to prevent rendering to a just-destroyed texture
- `OldTV.material` invalid parameter `frameShape` (removed duplicate line)
- `AudioAnalyzer.h` `Uint64` → `uint64_t` for MSVC portability
- OGRE 14.x API: `TexturePtr.isNull()`/`.setNull()` → `!ptr`/`.reset()`
- imgui-node-editor `BeginDelete()` called twice per frame → merged into single scope
- `GetSelectedNodes()` called outside `Begin()/End()` scope → moved inside

### Changed
- CMake version bumped to 3.0.0
- Tri-cible build: `bbfx-core` (static lib), `bbfx` (headless), `bbfx-studio` (GUI)
- FetchContent: imgui (docking), imgui-node-editor (develop branch)
- vcpkg: nlohmann-json added

## [2.9.0] - 2026-03-27

### Added
- **InputRecorder** (`src/record/InputRecorder.h/.cpp`): records keyboard/joystick/audio beat events with timestamps to `.bbfx-session` files (JSON Lines format, flushed per event)
- **InputPlayer** (`src/record/InputPlayer.h/.cpp`): replays `.bbfx-session` files, dispatching events at correct timestamps. Simple JSON parser, no external dependency
- **VideoExporter** (`src/record/VideoExporter.h/.cpp`): captures each rendered frame as PNG via `RenderTarget::writeContentsToFile()`, sequential numbering (frame_000001.png)
- **Mode offline** (`Engine::setOfflineMode/setOnlineMode`): fixed dt (1/fps) instead of real-time clock, renders at max speed without vsync
- **Lua wrappers**: `recorder.lua` (record/stoprecord), `player.lua` (replay/stopreplay), `exporter.lua` (export_start/stopexport)
- **functional.lua**: `map(t, fn)`, `filter(t, fn)`, `reduce(t, fn, init)`, `keys(t)`, `values(t)` — Lua functional utilities
- **remdebug.lua**: mobdebug integration for remote Lua debugging via VS Code, `debug()` REPL command
- **Demo production** (`lua/demos/demo_production.lua`): pipeline end-to-end — R=record, P=replay offline, E=export PNG

### Technical
- `.bbfx-session` format: JSON Lines (NDJSON), one event per line, flushed immediately
- InputPlayer: simple JSON key extraction without external lib
- VideoExporter uses `std::filesystem::create_directories` for output dir
- Engine offline mode modifies dt source for entire DAG chain

## [2.8.0] - 2026-03-27

### Added
- **PerlinGPU** (`resources/shaders/perlin_deform.glsl`): GLSL 330 vertex shader with Perlin 3D noise (Gustavson simplex), uniforms time/displacement/frequency/speed — replaces CPU PerlinVertexShader for 10-100x performance gain
- **Passthrough fragment shader** (`resources/shaders/passthrough.frag`): basic diffuse+ambient lighting for GPU-deformed meshes
- **ShaderFxNode** (`src/fx/ShaderFxNode.h/.cpp`): AnimationNode that loads any GLSL vertex shader, auto-parses float uniforms, creates DAG input ports, pushes values to GPU each frame via `GpuProgramParameters::setNamedConstant()`
- **Shader Lua wrapper** (`lua/shader.lua`): `Shader:load(path, {mesh=entity, uniform=value})` — one-liner to load a GPU shader, wire to DAG, and set initial uniforms. `setUniform()`, `list()`, `ShaderManager` registry
- **Profiler overlay** (`lua/profiler.lua`): real-time frame time display, toggle via `perf()` REPL command
- **Demo GPU** (`lua/demos/demo_gpu.lua`): Perlin GPU + audio RMS → displacement + HUD + profiler

### Technical
- GLSL loaded via `Ogre::HighLevelGpuProgramManager::createProgram()`, uniforms via `GpuProgramParameters`
- Auto-param mapping: worldViewProj, world, lightDiffuse, ambientLight, materialDiffuse
- ShaderFxNode parses `uniform float xxx;` lines from .glsl source to discover custom uniforms
- Shader directory `resources/shaders/` added to `resources.cfg`
- CPU PerlinVertexShader remains available as fallback

## [2.7.0] - 2026-03-26

### Added
- **Audio capture** (`src/audio/AudioCapture.h/.cpp`): SDL3_audio microphone capture, mono 44100Hz float32, ring buffer thread-safe, graceful fallback if no mic
- **AudioCaptureNode**: AnimationNode wrapping AudioCapture, `samples_ready` output port, polls ring buffer each frame
- **FFT analysis** (`src/audio/AudioAnalyzer.h/.cpp`): Radix-2 Cooley-Tukey FFT (header-only `kiss_fft.h`), Hann window, 8 frequency bands + RMS + peak as output ports
- **Beat detection** (`src/audio/BeatDetector.h/.cpp`): onset detection (energy > moving average × threshold), BPM estimation via beat interval averaging, `beat` trigger + `bpm` output ports, `sensitivity` input, 200ms anti-bounce
- **BandSplitNode** (`lua/audio.lua`): LuaAnimationNode aggregating band_0..2 → low, band_3..5 → mid, band_6..7 → high with exponential smoothing
- **Audio Lua wrapper** (`lua/audio.lua`): `Audio:start()` one-liner creates full chain (Capture → Analyzer → BeatDetector + BandSplit), `getRMS/getPeak/getBPM/getBand` accessors
- **OGRE Overlay bindings**: OverlayManager, Overlay, OverlayContainer, TextAreaOverlayElement exposed via sol2 for in-engine HUD
- **Audio HUD** (`lua/hud.lua`): real-time overlay showing BPM, RMS, low/mid/high levels, toggle via `hud()` REPL command
- **Sync auto-mode** (`lua/sync.lua`): `Sync:setAutoMode(audio)` — sequencer follows auto-detected BPM
- **REPL `audio()` command**: displays capture status, BPM, RMS, band levels
- **Demo audio** (`lua/demos/demo_audio.lua`): Perlin geosphere reactive to audio — RMS modulates LFO amplitude, beat triggers camera shake, HUD active

### Technical
- FFT: minimal Radix-2 Cooley-Tukey implementation in `kiss_fft.h` (~60 lines, header-only, public domain)
- SDL3_audio callback runs in separate thread, communicates via `std::mutex` + ring buffer — no Lua in callback
- Beat detection: energy-based onset with moving average (43 frames), BPM via rolling interval median, clamped 40-300 BPM
- vcpkg baseline updated for SDL3 compatibility

## [2.6.0] - 2026-03-26

### Added
- **REPL Lua console** (`lua/console.lua`): LuaConsoleNode in the DAG reads stdin non-blocking via StdinReader C++, evaluates Lua expressions in real-time during rendering
- **Introspection commands**: `graph()`, `ports(name)`, `set(name, port, val)`, `reload(script)`, `help()`, `quit()` — all global Lua functions usable from REPL and TCP
- **StdinReader C++** (`src/input/StdinReader.h/.cpp`): non-blocking stdin reader using `_kbhit()`/`_getch()` on Windows, with line buffer and backspace support
- **TcpServer C++** (`src/network/TcpServer.h/.cpp`): WinSock2 TCP server with `std::thread` listener, `std::mutex` + queue thread safety, max 2 clients, non-blocking I/O
- **ShellServer Lua** (`lua/shell/server.lua`): port of 2006 TCP shell — LuaAnimationNode polls TcpServer each frame, evaluates expressions via ErrorHandler, sends results back
- **Shell client** (`lua/shell/client.py`): Python TCP REPL client for connecting to the BBFx shell from any terminal
- **Hot reload** (`lua/hotreload.lua`): watches Lua files for modification, auto-reloads via `dofile()` with pcall protection; commands `watch()`, `unwatch()`, `watchlist()`
- **Logger** (`lua/logger.lua`): structured logging with info/warn/error levels, stdout + optional file output, timestamped format
- **ErrorHandler** (`lua/errorhandler.lua`): pcall wrapper for eval/dofile with `debug.traceback` stack traces, automatic `Logger.error()` on failure
- **Animator introspection**: `getNodeNames()`, `getNodeByName(name)`, `registerNode()` on Animator; `getInputNames()`, `getOutputNames()` on AnimationNode
- **Demo shell** (`lua/demos/demo_shell.lua`): complete v2.6 demo with REPL + TCP server + hot reload + Perlin scene

### Dependencies
- Added `ws2_32` (WinSock2) link on Windows for TCP networking

### Technical
- `StdinReader` uses `_kbhit()`/`_getch()` (Windows) or `select()` (POSIX) for non-blocking stdin
- `TcpServer` thread never touches `lua_State` — all Lua evaluation happens on the main thread via message queue
- `fileModTime` binding uses `std::filesystem::last_write_time` for portable file timestamp
- Hot reload checks timestamps once per second (not every frame) for performance

## [2.5.0] - 2026-03-26

### Added
- **Temporal nodes** (`lua/temporal_nodes.lua`): LFONode (sin/tri/square/saw), RampNode, DelayNode, EnvelopeFollowerNode — all implemented as LuaAnimationNode wrappers in the DAG
- **Animation spline** (`lua/animation.lua`): Animation:new/addFrames/create/bind/play/stop/setTime — Catmull-Rom spline interpolation in pure Lua, IM_SPLINE and IM_LINEAR modes, looping support
- **SubgraphNode** (`lua/subgraph.lua`): encapsulates a sub-DAG with named internal nodes, exposeInput/exposeOutput for external interface
- **Preset system** (`lua/subgraph.lua`, `lua/presets/`): Preset:define/instantiate/list/save/load with key=value file serialization; built-in preset "PerlinPulse"
- **Declarative graph builder** (`lua/declarative.lua`): `build({nodes, links})` — constructs animation DAG from a table description; types: LFO, Ramp, Delay, Envelope, existing, preset
- **Compo port** (`lua/compo.lua`): port of 2006 production compo.lua — Compo.starwars() with 10 particle systems, 13 spline animations, safe_psys/safe_bind guards, Dictionary:proxy stub
- **Fanions demo** (`lua/sets/demo_fanions.lua`): port of 2006 setFanions.textures.lua with two TextureCycle + two TextureSet, preset {gray, color, factor, rotate} format, swap
- **TextureSet API extended** (`lua/textureset.lua`): TextureSet:new(settings, cycle), on(), off(), setSwap()
- **Complete v2.5 demo** (`lua/demos/demo_v25.lua`): geosphere PerlinFxNode + spline animation + TextureSet + LFO→Ramp→displacement + PerlinPulse preset + declarative wiring + keyboard controls
- **Declarative demo** (`lua/demos/demo_declarative.lua`): LFO→Ramp→PerlinFxNode built in < 15 declarative lines

### Technical
- Catmull-Rom spline: pure Lua implementation (Ogre::SimpleSpline not in ogre-lua bindings)
- Animation:bind() targets Ogre SceneNode directly via LuaAnimationNode internal update
- Temporal node wrappers expose `_node` field for raw bbfx node access
- All particle system creation wrapped in pcall for graceful degradation

## [2.4.0] - 2026-03-26

### Added
- **Theora video system**: complete port of 2006 C++ code (10 files) to C++20/OGRE 14.5
  - `OggReader`: Ogg container format reader via libogg
  - `TheoraReader`: Theora video decoder using libtheora 1.2 (th_* API)
  - `TheoraBlitter`: YUV→RGBA conversion with static lookup tables, OGRE texture upload
  - `TheoraClip`: Threaded video playback via `std::jthread` with `std::atomic`/`std::condition_variable`
  - `ReversableClip`: Forward/reverse playback with reader swapping
  - `TextureCrossfader`: Manual blend between two texture layers
  - `TheoraSeekMap`: Frame-level seek index with JSON serialization (replaces Lua C API)
- **TheoraClipNode**: AnimationNode wrapper for video clips (ports: dt, time_control → playing, frame_ready)
- **sol2 bindings**: TheoraClip, ReversableClip, TextureCrossfader, TheoraClipNode
- **video.lua**: Lua video management module (createClip, overlay, crossfade)
- **textureset.lua**: Texture control system (TextureControl, SweepControl, TextureCycle, TextureSet)
- **demo_video.lua**: Video playback demo with keyboard controls (P play/pause, S stop)
- **Media**: bombe.ogg video file in resources/video/

### Dependencies
- Added `libtheora` 1.2.0 and `libogg` 1.3.6 via vcpkg

### Technical
- Thread-safe video decoding: `std::jthread` with `stop_token`, `std::atomic<bool>` flags
- Seek map cache in system temp directory (`std::filesystem::temp_directory_path()`)
- No pthreads, no volatile, no SWIG — fully C++20/sol2

## [2.3.0] - 2026-03-26

### Added
- **ogre-lua bindings**: ParticleSystem (emitters, affectors via StringInterface), CompositorManager (add/remove/toggle), Viewport, RenderWindow extensions, MeshManager, Plane
- **Scene Builder** (`object.lua`): Factory pattern ported from 2006 production code — fromMesh, fromBillboard, fromLight, fromPsys, fromCamera, fromFloorPlane + transform helpers
- **Scene Effects** (`effect.lua`): skybox, sky dome, fog, background color, shadows, ambient light, clearScene
- **Camera** (`camera.lua`): Camera setup + SphereTrack orbital camera controlled by azimuth/elevation/distance
- **Compositors** (`compositors.lua`): Wrapper for OGRE CompositorManager — add/remove/toggle Bloom, B&W, OldTV, DOF, Glass, Embossed
- **Joystick Mapping** (`joystick_mapping.lua`): SDL3 joystick enumeration, lookup by name, axis/button binding to AnimationNode ports
- **Controller** (`controller.lua`): Value mapping as AnimationNodes — linear, smooth (exponential), slide (ramp)
- **Waveforms** (`ogre_controller.lua`): Pure Lua waveform generators — sin, triangle, square, sawtooth
- **Keymap** (`keymap.lua`): SDL3 keyboard hotkey binding system with chord state shortcuts
- **Note system** (`note.lua`): Polymorphic note dispatch — Animation, Object, Effect, Action subtypes with on/off
- **Chord system** (`chord.lua`): State machine for compositions — named states containing timed note events
- **Sequencer** (`sequencer.lua`): Beat-based scheduler in pure Lua — schedules note on/off at BPM-driven beats
- **Sync** (`sync.lua`): Time synchronization — BPM to beat/bar/cycle mapping with event scheduling
- **Threads** (`threads.lua`): Coroutine scheduler integrated with frame loop via LuaAnimationNode
- **Helpers** (`helpers.lua`): UID generator, curry function, global accessors
- **Demo Geosphere**: Perlin noise on mesh + SphereTrack camera + joystick control
- **Demo Particles**: Particle system toggle (Aureola, PurpleFountain, Rain) + compositor toggle
- **Demo Set Jouable** (`sets/demo_set.lua`): First playable VJ set — geosphere + particles + compositors + chord states (F4-F7) + camera orbit
- **Media assets**: Compositor scripts (Bloom, DOF, Glass, OldTV, B&W, Embossed), particle scripts (Aureola, PurpleFountain, Rain, Snow, Smoke)
- Architecture documentation: section 15 "Composition Engine & Live Pipeline (v2.3)"

### Changed
- ogre-lua `scene_bindings.cpp` extended with 8 new OGRE types
- `resources.cfg` includes particles/ directory

### Recovery
- All 6 "lost" Lua modules from 2006 production code successfully recovered and ported
- 87 original production Lua files analyzed for architecture insights
- Global roadmap updated with recovered modules and corrected architecture descriptions

## [2.2.0] - 2026-03-26

### Added
- **Perlin optimization**: all noise functions use `float` instead of `double`, zero heap allocations in render loop
- **Stats overlay**: FPS counter with `Ogre::OverlayManager`, toggle with F3
- **Screenshot**: capture PNG with F12 via `RenderWindow::writeContentsToTimestampedFile`
- **Fullscreen toggle**: F11 via `SDL_SetWindowFullscreen`
- **PerlinFxNode**: AnimationNode wrapper for PerlinVertexShader (composition pattern), with ports: displacement, density, timeDensity, enable → mesh_dirty
- **TextureBlitterNode**: AnimationNode wrapper for TextureBlitter, with ports: r, g, b, a → texture_dirty
- **WaveVertexShader**: sinusoidal vertex deformation effect (AnimationNode + SoftwareVertexShader), with ports: amplitude, frequency, speed, axis → mesh_dirty
- **ColorShiftNode**: dynamic HSV→RGB material color shift (AnimationNode), with ports: hue_shift, saturation, brightness
- **Export DOT**: `Animator::exportDOT(filename)` serializes the animation DAG to Graphviz DOT format
- **Safe node cleanup**: `Animator::removeNode(node)` removes all connected edges before vertex deletion; `~AnimationNode()` auto-calls removeNode
- **Interactive demo**: `demo.lua` with 5 modes (minimal, perlin, wave, colorshift, combined), keyboard control (1-5 modes, ↑↓ params)
- Architecture documentation: FX-as-AnimationNode pattern documented in `docs/architecture.md` section 14

### Changed
- `PerlinVertexShader` pre-allocates position and normal buffers as members (no per-frame allocation)
- `Perlin.h` uses `float` throughout (was `double`)
- `Engine` handles F3/F11/F12 key events in the render loop
- `demo.lua` rewritten as multi-mode interactive demo (was minimal + perlin only)

## [2.1.0] - 2026-03-25

### Added
- OGRE resource loading via `resources.cfg` (19 models, 5 materials, 86 textures)
- AnimationPort Lua bindings (getName, getValue, setValue, getNode)
- SoftwareVertexShader with CPU mesh cloning and dynamic vertex buffers
- PerlinVertexShader with 3D Perlin noise deformation (C++20 constexpr)
- TextureBlitter for manual texture creation and pixel blitting
- FX Lua bindings (PerlinVertexShader, TextureBlitter)
- Unified demo.lua with minimal and perlin modes
- Command-line argument passing to Lua via `arg` table

### Changed
- Windows renderer switched to OpenGL 3+ (D3D11 debug abort workaround)

## [2.0.0] - 2026-03-25

### Added
- Complete rewrite from scratch in C++20
- OGRE 14.5.2 rendering engine (upgraded from OGRE 1.2)
- SDL3 window management and input (replaces OIS and raw X11/Win32)
- sol2 Lua bindings (replaces SWIG code generation)
- Lua 5.4+ scripting (upgraded from Lua 5.1)
- ogre-lua library: 50+ OGRE types exposed to Lua via sol2
- Boost.Graph 1.90 for animation DAG (upgraded from older Boost)
- CMake + vcpkg build system (replaces SCons + manual dependencies)
- Cross-platform support: Windows (D3D11) and Linux (Vulkan/OpenGL)
- GitHub Actions CI for Ubuntu and Windows
- Headless test runner for ogre-lua bindings
- Regression test suite (test_regression.lua)
- 60-second stability test (test_longrun.lua)
- Performance benchmarks (benchmark.lua)

### Changed
- `Engine` constructor takes `sol::state&` instead of raw `lua_State*`
- `Animator` uses `std::recursive_mutex` instead of POSIX `pthread_mutex_t`
- `LuaAnimationNode` uses `sol::function` instead of `luaL_ref`
- Input devices use SDL3 gamepad/keyboard/mouse APIs instead of OIS/libjsw
- All `std::auto_ptr` replaced with `std::unique_ptr`
- All `NULL` replaced with `nullptr`
- Platform abstraction via `platform.h` (no `#ifdef` in main code)

### Removed
- SWIG bindings and `swig.lua` reflection layer (replaced by sol2)
- SCons build system (`SConstruct`, `SConscript`)
- OIS input library dependency
- libjsw joystick library dependency
- POSIX pthread dependency (uses `<mutex>`)
- X11/OpenGL direct includes in core code
- Python 2 build-time dependency

### Architecture
- **Rendering**: OGRE 14.5.2 with D3D11 (Windows) / Vulkan (Linux), OpenGL 3+ fallback
- **Windowing**: SDL3 (cross-platform, replaces per-platform code)
- **Scripting**: Lua 5.4+ via sol2 (type-safe, header-only)
- **Animation**: Boost.Graph DAG with BFS propagation (unchanged architecture)
- **Build**: CMake 3.20+ with vcpkg dependency management
- **CI**: GitHub Actions (Ubuntu + Windows)
