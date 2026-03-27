# BBFx Revival — v2.x

**Real-time 3D animation and effects engine** — a modern C++20 revival of the 2006 BBFx (BlackBox Effects) engine.

BBFx provides a Lua-scriptable animation DAG (directed acyclic graph) that drives OGRE 3D rendering in real time. Animation nodes are defined and wired in Lua; BBFx propagates values through the graph each frame at render speed.

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
  └── Record          -- InputRecorder, InputPlayer, VideoExporter
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

- [`docs/architecture.md`](docs/architecture.md) — Full architecture reference (v2.0–v2.9), all modules, Lua API, design decisions
- [`lua/demos/USAGE.md`](lua/demos/USAGE.md) — Demo command reference

---

## History

BBFx was written in 2006 by Sébastien JULLIEN and Thomas LEFORT as a real-time 3D animation engine for demoscene productions: OGRE 1.2, OIS, SWIG, Lua 5.1, SCons on Linux. The v2.x revival (2025–2026) rewrites it from scratch in modern C++20 — same animation DAG architecture, entirely updated stack — and extends it with audio reactivity, GPU shaders, Theora video, live scripting, and a production recording/export pipeline.

---

## License

See [LICENSE](LICENSE).
