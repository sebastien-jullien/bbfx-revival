# Changelog

All notable changes to BBFx Revival are documented in this file.

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
