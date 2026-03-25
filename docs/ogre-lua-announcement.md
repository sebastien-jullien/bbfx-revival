# ogre-lua -- OGRE 14.x Lua Bindings via sol2

*Draft announcement for forums.ogre3d.org*

---

**Subject: [Release] ogre-lua v1.0 -- Lua 5.4 bindings for OGRE 14.x via sol2**

Hi everyone,

I'm happy to share **ogre-lua**, a library that exposes OGRE 14.x types to Lua 5.4+ using sol2 (header-only, type-safe C++ Lua bindings).

## What is it?

ogre-lua is a C++ library that registers OGRE types as Lua usertypes via sol2. It covers:

- **Math**: Vector2, Vector3, Vector4, Quaternion, ColourValue, Radian, Degree
- **Scene**: SceneManager, SceneNode, Entity, Light, Camera, BillboardSet, Root
- **Animation**: AnimationState, AnyNumeric (with helpers), AnimableObject, ControllerValueRealPtr

Over 50 OGRE types are exposed with constructors, methods, operators, and constants.

## Why?

The original OGRE Lua bindings used SWIG, which generated large C files, was painful to maintain, and had issues with OGRE's SharedPtr types on modern compilers. sol2 provides:

- Type-safe bindings written directly in C++
- No code generation step
- Clean integration with modern CMake/vcpkg
- Easy to extend with new types

## Quick Example

```lua
-- Create a scene entirely from Lua
local scene = Ogre.Root.getSingleton():getSceneManager("Default")
local root  = scene:getRootSceneNode()

scene:setAmbientLight(Ogre.ColourValue(0.5, 0.5, 0.5))

local light = scene:createLight("MainLight")
light:setType(Ogre.Light.LT_POINT)
light:setDiffuseColour(Ogre.ColourValue.White)

local node = root:createChildSceneNode("Player")
node:setPosition(Ogre.Vector3(0, 10, 0))
node:setScale(Ogre.Vector3(2, 2, 2))

-- Math operations work naturally
local v = Ogre.Vector3(1, 2, 3) + Ogre.Vector3(4, 5, 6)  -- Vector3(5, 7, 9)
local q = Ogre.Quaternion.IDENTITY
local rotated = q * Ogre.Vector3.UNIT_Y                     -- Vector3(0, 1, 0)
```

## Build

ogre-lua uses CMake + vcpkg:

```bash
git clone https://github.com/user/ogre-lua.git
cd ogre-lua
cmake --preset linux-release   # or windows-release
cmake --build --preset linux-release
ctest --preset linux-release   # 50/50 tests pass
```

Dependencies: OGRE 14.x, sol2 3.x, Lua 5.4+, all via vcpkg.

## Integration

```cmake
add_subdirectory(path/to/ogre-lua)
target_link_libraries(myapp PRIVATE ogre-lua)
```

```cpp
#include <ogre_lua/ogre_lua.h>
sol::state lua;
lua.open_libraries(sol::lib::base, sol::lib::math);
ogre_lua::register_all(lua);  // registers all OGRE types
lua.script_file("my_scene.lua");
```

## Test Suite

50 headless tests covering math, scene, and animation bindings:
- `test_math.lua` -- 22 tests (Vector3, Quaternion, ColourValue, Radian, Degree)
- `test_scene.lua` -- 17 tests (SceneNode, Entity, Light, Camera, SceneManager)
- `test_animation.lua` -- 11 tests (AnimationState, AnyNumeric, AnimableObject)

All tests run headless (no GPU required) using OGRE Root in headless mode.

## Context

ogre-lua was created as part of the BBFx Revival project -- a modern C++20 rewrite of a 2006 real-time animation engine that uses OGRE for rendering and Lua for scripting. The original used SWIG for bindings; ogre-lua replaces that with sol2.

## Roadmap

- v1.1: Material, Pass, TextureUnitState bindings
- v1.2: Mesh, SubMesh, VertexData bindings
- v2.0: Full OGRE 15.x support when released

Feedback and contributions welcome!

GitHub: https://github.com/user/ogre-lua

---

*Posted in: Community > Script Bindings*
