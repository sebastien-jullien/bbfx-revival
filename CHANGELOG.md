# Changelog

All notable changes to BBFx Revival are documented in this file.

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
