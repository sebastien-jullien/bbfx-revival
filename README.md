# BBFx Revival — v3.2.2

**Real-time 3D animation and effects engine** — a modern C++20 revival of the 2006 BBFx (BonneBalle Effects) engine.

BBFx provides a Lua-scriptable animation DAG (directed acyclic graph) that drives OGRE 3D rendering in real time. Animation nodes are defined and wired in Lua or via the visual node editor; BBFx propagates values through the graph each frame at render speed.

**v3.0 "BBFx Studio"** adds a full GUI application with Dear ImGui: interactive node editor, inspector, timeline, performance mode (F5), and video export — no code required for artists and VJs.

**v3.1 "BBFx Studio++"** completes and stabilizes the Studio: all stubs wired, BPM-to-DAG sync (beat/beatFrac ports), scene/project separation, Lua source serialization in `.bbfx-project`, CLI arguments (`--default`, `--reset`, `--fullscreen`), native Windows file dialogs, console REPL, full keyboard shortcuts, undo/redo (Command pattern), node duplication, bookmarks, and flow animation on links.

**v3.2 "BBFx Studio Content"** makes all presets functional and the Studio usable as a creative tool: 41 presets across 6 categories, 8 procedural fragment shaders, 13 BBFx compositors, ParamSpec system with auto-generated Inspector widgets, enable/disable nodes with visual [OFF] feedback, preset browser organized by category, and 13 new node types (SceneObject, Light, Particle, Camera, Compositor, Skybox, Fog, Math, Mapper, Mixer, Splitter, Trigger, BeatTrigger).

**v3.2.1 "Interactive Viewport"** adds direct manipulation in the 3D viewport: orbit/pan/zoom camera controller (Alt+LMB/MMB/scroll), ray-query object picking with bidirectional selection sync, translation gizmo with axis constraints and undo, procedural infinite grid, viewport toolbar (Select/Translate modes via Q/W), safe deletion with full OGRE cleanup, mesh-to-FX auto-linking, and "Use Editor Camera" toggle.

**v3.2.2 "Multi-Object Scene"** transforms the Studio from a single-object editor into a multi-object composition engine: Scene Hierarchy panel (F8) with visibility/lock toggles, Blender-style intelligent naming (ogrehead→Ogre with auto-increment), right-click context menus for object creation and FX application, drag-drop mesh/FX from browser to viewport, object duplication (Ctrl+D), parent-child hierarchy with relative transforms, cascade FX (multiple FX on one object), unified entity linking for all node types, and dynamic target resolution.

---

## Features

### Core
- **Animation DAG** — Boost.Graph directed acyclic graph, BFS propagation, pre/post operation queues
- **Lua scripting** — Lua 5.4+ via sol2 (type-safe, no code generation)
- **OGRE 14.5** — D3D11 (Windows), Vulkan/OpenGL (Linux), full resource pipeline
- **SDL3** — Window, keyboard, mouse, gamepad with hotplug support
- **ogre-lua** — Standalone library exposing 50+ OGRE types to Lua (SceneManager, Entities, Particles, Compositors…)
- **Cross-platform** — Windows 10+ and Linux

### Animation Nodes
- **Temporal nodes** — LFONode, RampNode, DelayNode, EnvelopeFollowerNode
- **Catmull-Rom spline** — IM\_SPLINE / IM\_LINEAR modes, looping, play/stop/seek
- **SubgraphNode** — sub-DAG encapsulation with external port interface
- **Preset system** — define, instantiate, save, load
- **Declarative graph builder** — `build({nodes, links})` syntax

### FX Nodes
- **PerlinVertexShader** — CPU 3D Perlin noise mesh deformation
- **PerlinGPU** — GLSL vertex shader, 10–100× faster than CPU version
- **ShaderFxNode** — loads any GLSL shader, auto-parses `uniform float` as input ports
- **TextureBlitter** — manual RGBA texture creation and pixel-level update
- **WaveVertexShader** — sinusoidal vertex displacement
- **ColorShiftNode** — per-frame colour modulation
- **SoftwareVertexShader** — CPU mesh cloning with dynamic vertex buffers

### Audio Reactive
- **AudioCapture** — SDL3 microphone capture, lock-free ring buffer
- **AudioAnalyzer** — Radix-2 Cooley-Tukey FFT, 8 frequency bands
- **BeatDetector** — energy-based onset detection, BPM estimation, 200ms anti-bounce
- **HUD overlay** — real-time BPM + low/mid/high level display

### Video
- **Theora playback** — OggReader → TheoraReader → TheoraBlitter (YUV→RGBA)
- **TheoraClip** — threaded playback via `std::jthread`
- **ReversableClip** — forward/reverse at runtime
- **TextureCrossfader** — manual blend between two texture layers

### Live Scripting
- **REPL console** — non-blocking stdin reader, Lua expression evaluation (`graph`, `ports`, `set`, `reload`, `watch`, `help`, `quit`)
- **TCP remote shell** — WinSock2/POSIX server (port 33195, max 2 clients) + Python client
- **Hot reload** — file watcher, auto-reloads modified Lua files on next frame
- **Logger** — structured logging (info/warn/error), stdout + optional file
- **ErrorHandler** — `pcall` wrapper with `debug.traceback` stack traces

### Production Pipeline
- **InputRecorder** — records keyboard/joystick/audio-beat events to `.bbfx-session` JSON Lines
- **InputPlayer** — replays `.bbfx-session` at correct timestamps
- **Offline mode** — fixed dt (1/fps), renders at max speed without vsync
- **VideoExporter** — captures sequential PNG frames (`frame_000001.png`, …) via `RenderTarget::writeContentsToFile()`
- **End-to-end pipeline** — record → replay offline → export PNG → FFmpeg

### BBFx Studio (v3.0)
- **StudioEngine** — OGRE renders to RenderTexture, shared SDL3/OpenGL context with ImGui
- **Viewport Panel** — live OGRE render with FPS/resolution/mode overlay, dynamic resize
- **Node Editor Panel** — interactive DAG visualization (imgui-node-editor), link create/delete by drag & drop, right-click context menus, color by type, real-time port values
- **Inspector Panel** — float sliders, enum dropdowns, Lua source editor, ShaderFx uniforms, rename/delete
- **Timeline Panel** — beat/bar markers, animated playhead, draggable chord blocks (snap-to-beat), transport controls, BPM, 8-band audio spectrum
- **Preset Browser** — filesystem scan, drag-to-graph instantiation, effect rack with bypass, 8-slot quick access bar
- **Performance Mode (F5)** — fullscreen viewport 80%, 4x4 trigger grid, 8 configurable faders, VU meters, BPM overlay, PANIC button
- **Project Save/Load** — `.bbfx-project` JSON format, auto-save, recent projects
- **Export Dialog** — frame-by-frame PNG export with progress bar, offline rendering
- **CPack NSIS** — Windows installer with shortcuts and `.bbfx-project` file association

### BBFx Studio++ (v3.1)
- **Studio++ completion** — all stubs wired, full feature parity
- **BPM to DAG sync** — beat/beatFrac ports driven by BeatDetector
- **Scene/project separation** — independent scene and project lifecycles
- **Lua source serialization** — full graph state saved in `.bbfx-project`
- **CLI arguments** — `--default`, `--reset`, `--clear`, `--fullscreen`
- **Native Windows file dialogs** — open/save via OS dialogs
- **Console REPL** — `graph`, `ports`, `set`, `help` commands
- **Full keyboard shortcuts** — F1-F8, Space, Ctrl+E/N/O
- **Undo/redo** — Command pattern, Ctrl+Z / Ctrl+Y
- **Node duplication** — Ctrl+D
- **Bookmarks** — Ctrl+1-9
- **Flow animation on links** — animated particles along DAG edges

### BBFx Studio Content (v3.2)
- **ParamSpec system** — 14 typed parameters (FLOAT, INT, BOOL, STRING, ENUM, COLOR, VEC3, MESH, TEXTURE, MATERIAL, SHADER, PARTICLE, COMPOSITOR) with auto-generated Inspector widgets
- **13 new node types** — SceneObjectNode, LightNode, ParticleNode, CameraNode, CompositorNode, SkyboxNode, FogNode, MathNode, MapperNode, MixerNode, SplitterNode, TriggerNode, BeatTriggerNode
- **41 presets** — 6 categories (Geometry, Color, PostProcess, Particle, Camera, Composition), preset format v2 with ParamSpec + `build()` function
- **8 procedural fragment shaders** — plasma, voronoi, mandelbrot, truchet, flowfield, tunnel, reaction_diffusion, sphere_trace
- **13 BBFx compositors** — wrapping existing GLSL shaders for post-processing via CompositorNode
- **MeshGenerator** — runtime procedural meshes (plane, sphere, cube, cylinder, torus, cone)
- **Enable/Disable nodes** — `mEnabled` flag with Animator skip, visual [OFF] feedback, OGRE setVisible() overrides
- **Preset browser** — organized by category in collapsible accordions
- **Demo scene as DAG nodes** — SceneObjectNode + LightNode, fully deletable and disableable
- **LightNode** — dynamic type (point/directional/spot) with color from Inspector color picker
- **Camera restore on delete** — own SceneNode, detach/reattach pattern
- **Particle rendering in Studio** — manual `_update()` call for RenderTexture pipeline
- **GL State Guard** — RAII save/restore of GL buffer bindings for OGRE/ImGui coexistence
- **Deferred clone** — PerlinFxNode/WaveVertexShader create mesh clones at first `frameStarted()` instead of constructor
- **Studio Debugger** — `dbg.*` commands for automated testing and node inspection from Console panel

### BBFx Studio Interactive Viewport (v3.2.1)
- **ViewportCameraController** — orbit (Alt+LMB), pan (Alt+MMB), zoom (scroll wheel), editor/DAG-driven modes, camera reset (F key)
- **ViewportPicker** — OGRE ray query picking, bidirectional selection with NodeEditor, orange wireframe highlight (GLSL clone entity overlay)
- **ViewportGizmo** — translation gizmo (XYZ arrows + center sphere), axis-constrained drag, screen-space interaction via OGRE ManualObject
- **ViewportGrid** — procedural infinite grid (fade-out by distance), axis-colored lines (X=red, Z=blue), Y=0 reference plane
- **ViewportToolbar** — ImGui toolbar strip (Select / Translate mode toggle), keyboard shortcuts (Q/W)
- **Safe deletion** — confirmation dialog, full OGRE cleanup (Entity, SceneNode, Light, ParticleSystem), Animator unlink, undo support via DeleteNodeCommand
- **Mesh→FX linking** — automatic SceneObjectNode↔PerlinFxNode/WaveVertexShader connection, entity name resolution, LinkMeshFxCommand with undo
- **TransformCommands** — undo/redo for gizmo transforms (MoveNodeCommand stores before/after positions)
- **"Use Editor Camera" menu** — toggle between editor orbit camera and DAG-driven CameraNode
- **LMB confirm in keyboard mode** — left mouse button confirms node placement in keyboard navigation mode

### BBFx Studio Multi-Object Scene (v3.2.2)
- **SceneHierarchyPanel** — dockable panel (F8) listing all scene objects with type prefixes ([M]esh, [L]ight, [P]article, [C]amera), visibility eye toggle, lock padlock toggle, context menu (Rename, Delete, Focus, Hide, Lock), drag-drop reparenting
- **SceneObjectNamer** — intelligent Blender-style naming from mesh files (ogrehead→Ogre, geosphere4500→Geosphere), auto-increment (.001, .002)
- **Viewport context menus** — right-click in void: "Add Object" with mesh list; right-click on object: Apply FX, Duplicate, Delete, Focus, Hide, Lock, Rename
- **Drag-drop mesh/FX** — drag mesh from browser to viewport creates SceneObjectNode at raycast position; drag FX preset onto selected object auto-creates and auto-connects
- **Object duplication** — Ctrl+D duplicates selected SceneObjectNode with all parameters and +2 position offset
- **Per-object visibility/lock** — `mUserVisible` and `mLocked` flags on AnimationNode; visibility = AND of mEnabled, mUserVisible, port visible; lock prevents picking and gizmo
- **Parent-child hierarchy** — ParamSpec `parent_node`, OGRE reparenting with world→local transform conversion, ReparentNodeCommand with undo
- **FX badge** — SceneObjectNode shows "FX: N" in node editor with tooltip listing connected FX names
- **Entity link unifie** — auto-creation of entity→entity links on data port connections (CreateLinkCommand, Debugger, ProjectSerializer with Lua source introspection); `getTargetSceneNode()` Lua API for dynamic target resolution; moving entity link instantly changes animation target
- **Cascade FX** — multiple FX nodes can target the same SceneObjectNode simultaneously
- **dbg.test() 11/11 PASS** — fix timing deferred creates, all tests green

---

## Architecture

```
Lua scripts (lua/*.lua, lua/demos/*.lua)
    |
sol2 bindings (src/bindings/bbfx_bindings.cpp)
    |
C++ core
  ├── Engine          -- SDL3 window + OGRE render loop
  ├── Animator        -- Boost.Graph DAG, BFS propagation, pre/post op queues
  ├── PrimitiveNodes  -- RootTimeNode, LuaAnimationNode, AnimationStateNode
  ├── FX              -- Perlin (CPU+GPU), TextureBlitter, WaveVertex, Shader, ColorShift
  ├── Input           -- KeyboardManager, MouseManager, JoystickManager (SDL3)
  ├── Audio           -- AudioCapture, AudioAnalyzer, BeatDetector
  ├── Video           -- OggReader, TheoraReader, TheoraBlitter, TheoraClip, Crossfader
  ├── Network         -- TcpServer (remote REPL)
  ├── Record          -- InputRecorder, InputPlayer, VideoExporter
  └── Studio          -- StudioApp, StudioEngine, NodeTypeRegistry, Debugger
       ├── Nodes      -- SceneObject, Light, Particle, Camera, Compositor, Skybox, Fog, Math, ...
       ├── Panels     -- Viewport, NodeEditor, Inspector, Timeline, Presets, Console, Perf
       ├── Viewport   -- CameraController, Picker, Gizmo, Grid, Toolbar (v3.2.1)
       ├── Hierarchy  -- SceneHierarchyPanel (v3.2.2)
       ├── Commands   -- CommandManager, Undo/Redo (Node/Link/Edit/Transform/Scene/Reparent commands)
       ├── Generators -- MeshGenerator (procedural meshes)
       └── Project    -- ProjectSerializer, ExportDialog
    |
OGRE 14.5 + SDL3  (via vcpkg)
    |
ogre-lua  (standalone: SceneManager, Particles, Compositors, MeshManager…)
```

### C++ Modules

| Module | Description |
|--------|-------------|
| `src/core/Engine` | SDL3 window + OGRE render loop singleton |
| `src/core/Animator` | Animation DAG: add/remove nodes, link/unlink ports, BFS propagation |
| `src/core/PrimitiveNodes` | RootTimeNode (clock), LuaAnimationNode, AnimationStateNode, AccumulatorNode |
| `src/fx/PerlinVertexShader` | 3D Perlin noise CPU vertex deformation |
| `src/fx/ShaderFxNode` | GLSL shader loader with auto-parsed float uniforms as ports |
| `src/fx/TextureBlitter` | Manual RGBA texture, pixel-level write |
| `src/fx/WaveVertexShader` | Sinusoidal vertex displacement |
| `src/input/` | KeyboardManager, MouseManager, JoystickManager, InputManager |
| `src/audio/` | AudioCapture, AudioAnalyzer, BeatDetector |
| `src/video/` | TheoraClip, ReversableClip, TheoraBlitter, TextureCrossfader |
| `src/network/TcpServer` | TCP REPL server, WinSock2/POSIX |
| `src/record/` | InputRecorder, InputPlayer, VideoExporter |
| `src/studio/` | StudioApp, StudioEngine, NodeTypeRegistry, Debugger, SettingsManager, ResourceEnumerator |
| `src/studio/nodes/` | SceneObjectNode, LightNode, ParticleNode, CameraNode, CompositorNode, SkyboxNode, FogNode, MathNode, MapperNode, MixerNode, SplitterNode, TriggerNode, BeatTriggerNode |
| `src/studio/panels/` | ViewportPanel, NodeEditorPanel, InspectorPanel, TimelinePanel, PresetBrowserPanel, ConsolePanel, PerformanceModePanel, SetEditorPanel, SceneHierarchyPanel |
| `src/studio/viewport/` | ViewportCameraController, ViewportPicker, ViewportGizmo, ViewportGrid, ViewportToolbar |
| `src/studio/commands/` | CommandManager, NodeCommands, LinkCommands, EditCommands, ChordCommands, TransformCommands, SceneCommands |
| `src/studio/generators/` | MeshGenerator (plane, sphere, cube, cylinder, torus, cone) |
| `src/studio/project/` | ProjectSerializer, ExportDialog |
| `src/bindings/` | sol2 bindings for all BBFx types |

### Lua Modules

| Module | Description |
|--------|-------------|
| `animation.lua` | Catmull-Rom spline: play/stop/seek, IM\_SPLINE/IM\_LINEAR |
| `audio.lua` | Capture → FFT → BeatDetector + 8-band split |
| `camera.lua` | Camera setup + SphereTrack orbital camera |
| `chord.lua` | Composition state machine (named states + timed notes) |
| `compositors.lua` | Bloom, DOF, Glass, OldTV, B&W, Embossed |
| `console.lua` | REPL console: StdinReader → eval() |
| `declarative.lua` | Graph builder: `build({nodes, links})` |
| `exporter.lua` | PNG frame export: `export_start()`, `stopexport()` |
| `hotreload.lua` | File watcher, auto-reload via `dofile()` |
| `hud.lua` | Audio HUD overlay: BPM, RMS, frequency levels |
| `logger.lua` | Structured logging: info/warn/error |
| `object.lua` | Scene builder: fromMesh, fromBillboard, fromLight, fromPsys |
| `player.lua` | `.bbfx-session` replay: `replay()`, `stopreplay()` |
| `profiler.lua` | Frame-time overlay, `perf()` REPL toggle |
| `recorder.lua` | Input recording: `record()`, `stoprecord()` |
| `sequencer.lua` | Beat-based scheduler: note on/off at BPM-driven beats |
| `shader.lua` | GPU shader: `Shader:load()`, `setUniform()` |
| `subgraph.lua` | SubgraphNode + Preset system |
| `sync.lua` | BPM → beat/bar/cycle, auto-mode from audio |
| `temporal_nodes.lua` | LFONode, RampNode, DelayNode, EnvelopeFollowerNode |
| `threads.lua` | Coroutine scheduler integrated with frame loop |
| `paramspec.lua` | ParamSpec builder: float/int/bool/enum/color/vec3/mesh/texture/material/shader/particle/compositor typed parameters |
| `video.lua` | createClip, overlay, crossfade |

---

## Quick Start

### Prerequisites

- C++20 compiler (MSVC 2022+ or GCC 11+)
- CMake 3.20+
- vcpkg

### Build

```bash
git clone https://github.com/user/bbfx-revival.git
git clone https://github.com/user/ogre-lua.git   # sibling directory

cd bbfx-revival
cmake --preset windows-release    # or linux-release
cmake --build --preset windows-release
ctest --preset windows-release
```

### Run

```bash
# From the build output directory
./bbfx lua/bbfx_minimal.lua
```

---

## Demos

All demos run from the build output directory (`build/windows-debug/Debug/` or equivalent):

| Demo | Launch | Description |
|------|--------|-------------|
| **Studio** | `./bbfx-studio lua/demos/demo_studio.lua` | Full GUI: node editor, inspector, 41 presets, performance mode |
| **Minimal** | `./bbfx lua/bbfx_minimal.lua` | Rotating mesh with a single LuaAnimationNode |
| **Interactive** | `./bbfx lua/demo.lua` | 5 modes: minimal / Perlin / wave / colorshift / combined |
| **Geosphere** | `./bbfx lua/demos/demo_geosphere.lua` | Perlin-deformed head, orbital camera, keyboard+joystick |
| **Particles** | `./bbfx lua/demos/demo_particles.lua` | Aureola, PurpleFountain, Rain particle systems |
| **Video** | `./bbfx lua/demos/demo_video.lua` | Theora video on billboard (P=play/pause, R=rewind, B=reverse) |
| **v2.5 Full** | `./bbfx lua/demos/demo_v25.lua` | LFO + spline + SubgraphNode + presets + declarative |
| **Declarative** | `./bbfx lua/demos/demo_declarative.lua` | Graph defined in <15 lines via `build({…})` |
| **Shell** | `./bbfx lua/demos/demo_shell.lua` | REPL console + TCP remote shell + hot reload |
| **Audio** | `./bbfx lua/demos/demo_audio.lua` | Audio-reactive Perlin + HUD (H=toggle, [/]=frequency) |
| **GPU** | `./bbfx lua/demos/demo_gpu.lua` | PerlinGPU GLSL + audio RMS + profiler (H/P=toggles) |
| **Production** | `./bbfx lua/demos/demo_production.lua` | R=record, P=replay offline, E=export PNG frames |

### Production pipeline

```bash
# 1. Record a session interactively
./bbfx lua/demos/demo_production.lua   # press R to start, R again to stop

# 2. Replay offline and export PNG frames
./bbfx lua/demos/demo_production.lua   # press P → E (exports frame_000001.png …)

# 3. Encode to video
ffmpeg -framerate 60 -i frame_%06d.png -c:v libx264 -pix_fmt yuv420p out.mp4
```

### TCP remote shell

```bash
# Server starts automatically with demo_shell.lua
./bbfx lua/demos/demo_shell.lua

# Connect from another terminal
python lua/shell/client.py            # default: localhost:33195
> graph         -- print DAG node list
> ports spin    -- list ports of node "spin"
> set rms 0.8   -- set a port value
> reload        -- reload all watched Lua files
```

---

## Lua Example

```lua
-- Minimal rotating scene node
local engine   = bbfx.Engine.instance()
local scene    = engine:getSceneManager()
local animator = bbfx.Animator.instance()

local node = scene:getRootSceneNode():createChildSceneNode("Spinner")

local timer = bbfx.RootTimeNode("timer")
animator:addNode(timer)

local spinner = bbfx.LuaAnimationNode("spin", function(self)
    node:yaw(Ogre.Radian(0.02))
end)
animator:addNode(spinner)
```

```lua
-- Audio-reactive GPU shader
require("audio")
require("shader")

Audio:start()
local s = Shader:load("resources/shaders/perlin_gpu.glsl", { amplitude = 0, speed = 1.5 })

local modulate = bbfx.LuaAnimationNode("modulate", function()
    s:setUniform("amplitude", Audio:getRMS() * 3.0)
end)
animator:addNode(modulate)
```

---

## Dependencies (via vcpkg)

| Dependency | Version | Purpose |
|------------|---------|---------|
| OGRE | 14.5.2 | 3D rendering + overlays + compositors |
| SDL3 | 3.x | Window, input, audio capture |
| sol2 | 3.x | Lua/C++ bindings |
| Lua | 5.4+ | Scripting runtime |
| Boost.Graph | 1.90+ | Animation DAG |
| Dear ImGui | 1.91+ | Studio GUI (panels, inspector, menus) |
| imgui-node-editor | — | Visual node graph editor |
| libtheora | 1.2.0 | Theora video decoding |
| libogg | 1.3.6 | Ogg container parsing |

---

## Tests

```bash
ctest --preset windows-release
```

| Test | Description |
|------|-------------|
| `tests/lua/test_regression.lua` | Headless ogre-lua binding regression tests |
| `tests/test_longrun.lua` | 60-second stability test |
| `tests/benchmark.lua` | Frame time and Lua node overhead benchmarks |

---

## Documentation

- [`docs/architecture.md`](docs/architecture.md) — Full architecture reference (v2.0–v3.2.2), all modules, Lua API, design decisions
- [`lua/demos/USAGE.md`](lua/demos/USAGE.md) — Demo and Studio usage reference

---

## History

BBFx was written in 2006 by Sébastien JULLIEN and Thomas LEFORT as a real-time 3D animation engine for demoscene productions: OGRE 1.2, OIS, SWIG, Lua 5.1, SCons on Linux. The v2.x revival (2025–2026) rewrites it from scratch in modern C++20 — same animation DAG architecture, entirely updated stack — and extends it with audio reactivity, GPU shaders, Theora video, live scripting, and a production recording/export pipeline. v3.0 introduces the visual Studio (ImGui + OGRE 14), v3.1 stabilizes it with undo/redo and project serialization, v3.2 delivers 41 presets and 13 new node types, v3.2.1 adds interactive viewport manipulation (picking, gizmos, grid), and v3.2.2 completes the multi-object workflow with scene hierarchy, drag-drop, and cascade FX.

---

## License

See [LICENSE](LICENSE).
