# L-System Tree

Procedural L-system tree generator showcased as a v3.5 example plugin.

![L-System Tree preview](thumbnail.png)

## Features

- Classic Koch-style branching : axiom `F`, rule `F → F[+F]F[-F]F`.
- Exposed iteration count (1..5), branch angle (5°..90°), segment
  length (0.1..5.0).
- Generates an OGRE line-list mesh via `bbfx.lsystem.generateMesh`.
- Caches the generated mesh keyed on (iterations, angle, length)
  so re-evaluation is free when parameters don't change.

## Ports

| Port         | Type  | Range       | Description                   |
|--------------|-------|-------------|-------------------------------|
| `iterations` | int   | 1..5        | Rewriting depth               |
| `angle`      | float | 5..90 (°)   | Branch deviation              |
| `length`     | float | 0.1..5      | Segment length                |
| `meshName`   | str   | —           | Output OGRE mesh name         |

## Usage

Enable the plugin, create an `LSystemTree` node, and wire its
`meshName` output into a `SceneObjectNode.mesh_file` input.

## Changelog

### 1.0.0 — 2026-04-18
Initial release for BBFx v3.5.
