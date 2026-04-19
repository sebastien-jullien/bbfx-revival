# SDF Raymarch

Signed-distance-field raymarcher showcased as a v3.5 example plugin.

![SDF Raymarch preview](thumbnail.png)

## Features

- 3 primitives : sphere / box / torus, selectable per frame.
- Inigo-Quilez smooth-union blend with tunable softness.
- Generates a polygonal proxy mesh via `bbfx.sdf.toMesh` (Marching Cubes)
  so the result can be lit, shadow-mapped and composited like any
  OGRE mesh.
- Fragment shader `shaders/sdf_raymarch.frag` for the GPU path.
- No permissions required.

## Ports

| Port       | Type  | Range       | Description                              |
|------------|-------|-------------|------------------------------------------|
| `shape`    | int   | 0..2        | 0 = dual sphere smoothUnion, 1 = box, 2 = torus |
| `distance` | float | 1..10       | Camera distance (GPU path only)          |
| `softness` | float | 0.01..1.0   | smoothUnion blend factor                 |
| `meshName` | str   | —           | Output OGRE mesh name (Marching Cubes)   |

## Usage

1. Install from Community Browser.
2. Enable in Plugin Manager.
3. NodeEditor → right-click → PostProcess → SDFRaymarch.
4. Feed `meshName` into a `SceneObjectNode.mesh_file` port.

## Changelog

### 1.0.0 — 2026-04-18
Initial release for BBFx v3.5.
