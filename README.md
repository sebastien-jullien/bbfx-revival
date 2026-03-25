# BBFx Revival

**Real-time 3D animation and effects engine** -- a modern C++20 revival of the 2006 BBFx (BlackBox Effects) engine.

BBFx provides a Lua-scriptable animation DAG (directed acyclic graph) that drives OGRE 3D rendering in real time. Artists and developers define animation nodes in Lua, wire them together, and BBFx propagates values through the graph each frame.

## Features

- **Animation DAG** -- Boost.Graph-based directed acyclic graph with BFS propagation
- **Lua scripting** -- Lua 5.4+ via sol2 bindings (type-safe, no code generation)
- **OGRE 14.5** -- Modern 3D rendering with D3D11 (Windows) and Vulkan/OpenGL (Linux)
- **SDL3 input** -- Keyboard, mouse, and gamepad with hotplug support
- **Cross-platform** -- Windows 10+ and Linux
- **ogre-lua** -- Standalone library exposing 50+ OGRE types to Lua

## Quick Start

### Prerequisites

- C++20 compiler (MSVC 2019+ or GCC 11+)
- CMake 3.20+
- vcpkg

### Build

```bash
# Clone
git clone https://github.com/user/bbfx-revival.git
git clone https://github.com/user/ogre-lua.git   # sibling directory

# Configure
cd bbfx-revival
cmake --preset windows-release    # or linux-release

# Build
cmake --build --preset windows-release

# Test
ctest --preset windows-release
```

### Run

```bash
./bbfx lua/bbfx_minimal.lua
```

## Architecture

```
Lua scripts (bbfx_minimal.lua, input.lua, sol2_compat.lua)
    |
sol2 bindings (bbfx_bindings.cpp)
    |
C++ core:
  Engine       -- SDL3 window + OGRE render loop
  Animator     -- Boost.Graph DAG, BFS propagation
  InputManager -- SDL3 keyboard/mouse/gamepad
    |
OGRE 14.5 + SDL3 (via vcpkg)
```

### Key Modules

| Module | Description |
|--------|-------------|
| `src/core/Engine` | Singleton owning the SDL3 window and OGRE render loop |
| `src/core/Animator` | Animation DAG with pre/post operation queues |
| `src/core/PrimitiveNodes` | RootTimeNode, AccumulatorNode, LuaAnimationNode, AnimationStateNode |
| `src/input/` | KeyboardManager, MouseManager, JoystickManager (all SDL3) |
| `src/fx/` | PerlinVertexShader, TextureBlitter, SoftwareVertexShader |
| `src/bindings/` | sol2 bindings for all BBFx types |
| `lua/` | Lua scripts: minimal scene, input, compatibility layer |

## Lua Example

```lua
-- Create a Lua animation node that rotates a scene node
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

## Dependencies (via vcpkg)

| Dependency | Version | Purpose |
|------------|---------|---------|
| OGRE | 14.5.2 | 3D rendering |
| SDL3 | 3.x | Window, input, events |
| sol2 | 3.x | Lua C++ bindings |
| Lua | 5.4+ | Scripting runtime |
| Boost.Graph | 1.90+ | Animation DAG |

## Tests

- `tests/lua/test_regression.lua` -- Headless ogre-lua binding tests
- `tests/test_longrun.lua` -- 60-second stability test
- `tests/benchmark.lua` -- Frame time and Lua node overhead benchmarks

## License

See LICENSE file.

## History

BBFx was originally written in 2006 by Gron as a real-time 3D animation engine using OGRE 1.2, OIS, SWIG, Lua 5.1, and SCons on Linux. This revival rewrites it from scratch in modern C++20 with contemporary dependencies while preserving the core animation DAG architecture.
