# Changelog

All notable changes to BBFx Revival are documented in this file.

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
