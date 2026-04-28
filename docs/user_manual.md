# BBFx Studio — User Manual

> **Manual version:** v3.5.1 "Asset Library & Polish"
> **Product:** `bbfx-studio.exe`
> **Audience:** end users (VJs, stage designers, creatives) — not developers
> **Language:** English

This manual describes **every visible and interactive feature** of BBFx Studio: interface, panels, menus, shortcuts, nodes, and workflows. It does not cover the internal C++ API, implementation choices or architecture — those belong to the CDC, Roadmap and Epics.

---

## What's new in v3.5.1 "Asset Library & Polish"

BBFx Studio v3.5.1 transforms the Studio from infrastructure-perfect to content-rich. Headlines:

- **Asset Browser panel** (`Ctrl+Shift+A`, **chapter 9bis**) — 9 categories sidebar (Meshes, Textures, Materials, Shaders, Particles, Effects, Cameras, Presets, Templates) with instant search, AND multi-select tags, persisted favorites, info tooltip, and **8 typed drag&drop payloads** (Mesh/Texture/Material/Shader/Particle/Preset/Compositor/Template) + double-click actions. Ships with the new default layout (tabbed with Timeline).
- **Effect Rack panel** (`F9`, **section 10.3**) — standalone panel (extracted from Preset Browser) with LED-style green/red feedback per node, undoable bypass via `SetEnabledCommand`, and **MIDI / Gamepad / Keyboard learn buttons per row** (M/G/K) with badges showing the binding. Bindings stay active even when the panel is closed.
- **Camera with 6 modes** — `orbit` / `fly_through` / `shake` / `dolly_zoom` / `track` / `crane`, with smooth transitions (`transitionTo` smoothstep + Slerp + lerp FOV). DAG ports `target.x/y/z`, `shake_intensity`, `transition_time`, `fly_speed`, `damping`, `crane_amplitude`. 5 ready-to-use camera presets.
- **Colored particles** — ParticleNode now has 8 DAG ports (`color.r/g/b/a`, `particle_size`, `velocity`, `lifetime`, `emission_rate`). Defaults of `-1.0` preserve the template values; non-negative values override (multiplier mode for size/velocity).
- **10 new BBFx particle templates** — Fire, ElectricArc, Confetti, MagicDust, NeonTrail, Bubbles, LaserBeam, Galaxy, MatrixRain, and the **signature 2006 ParticleTunnel** (Ring emitter, gamepad-controllable `ring_radius`/`ring_speed`/`ring_density`).
- **PostProcess in Studio viewport** — 29 effects available via `PostProcessNode` (16 BBFx + 6 OGRE adapted single-pass + 7 new). Effects appear immediately in the Studio Viewport, not just in Performance Mode. Use the `compositor` parameter on a PostProcessNode to pick from: Vignette, FilmGrain, Invert, Posterize, EdgeDetect, Pixelate, Barrel, Kaleidoscope, ChromaticAberration, VHS, HeatDistort, ASCII, MotionTrail, EdgeBlend, QuadWarp, GridWarp, Bloom, DOF, BlackAndWhite, Embossed, OldTV, Glass, Halftone, CrossHatch, OilPaint, ColorLUT, RadialBlur, FeedbackZoom, MotionTrailFB.
- **TextureNode lighting modes** — new ENUM parameter `lighting_mode` with **unlit / lit / emissive** options. `unlit` shows raw texture colors, `lit` adds standard lighting, `emissive` makes the surface glow at full brightness. A global default is configurable in `File → Settings... → Default texture lighting`.
- **15 new shaders** — 5 vertex (`explode`, `inflate`, `spiral`, `audio_pulse`, `flatten`) + 8 fragment generators (`glitch_block`, `julia`, `fire`, `fbm_warp`, `cellular_automata`, `hexagonal`, `moire`, `waveform`) + 7 post-process (`halftone`, `cross_hatch`, `oil_paint`, `color_lut`, `radial_blur`, `feedback_zoom`, `pp_motion_trail`). ShaderFxNode now accepts vec2/vec3/vec4 uniforms (decomposed as `.x/.y/.z/.w` ports).
- **8 new VJ materials** — `BBFx/Chrome`, `BBFx/Neon`, `BBFx/GlassVJ`, `BBFx/Wireframe`, `BBFx/Hologram`, `BBFx/Emissive`, `BBFx/Gradient`, `BBFx/Reflective` — pick from the Asset Browser or via a MaterialNode.
- **5 new procedural meshes** — `mobius.mesh`, `lissajous.mesh`, `helix.mesh`, `diamond.mesh`, `star3d.mesh`. Plus the 7 canonical procedural meshes are now reliably available at startup (`torus.mesh`, `cylinder.mesh`, `cone.mesh`, `plane_1m.mesh`, `torusknot.mesh`, `cube_1m.mesh`, `bbfx_plane.mesh`).
- **5 functional skyboxes** — Stormy, Morning, Evening, EarlyMorning, CloudyNoon (with bundled cubemaps).
- **14 functional scene templates** — every template in `File → Recent Templates ▶` now creates a complete starting scene (mesh, FX, camera, light, etc.) instead of just setting the BPM. Genre-tuned templates: ambient (BPM 70), hiphop (90), house (124), techno (135), dubstep (140), dnb (172), beat_machine (128), particle_show (140), shader_lab (120), audio_reactive (0), full_performance (≥ 8 nodes), video_mix (120), bonneballe_basic.
- **93 curated presets** + 6 backward-compatibility aliases — preset library cleaned up (101 → 93 active), 26 composition presets corrected to the proper `CompositionNode` format, misleading names renamed, phantom parameters removed. Renamed presets keep working through aliases (color_shift, monochrome_fade, perlin_pulse, perlin_breath, geosphere_explode, vertex_noise).
- **Audio "fallback" without a microphone** — ShaderFxNode shaders that use `bass`/`mid`/`high` uniforms (e.g. `audio_pulse.vert`) now produce a tempo-pulsed envelope automatically when no AudioCaptureNode is connected (quarters/eighths/sixteenths). Plug an AudioCaptureNode and the synthetic values are bypassed.
- **Persistent docking layout** — your panel arrangement is saved on `Ctrl+S` and restored on next launch (no more reset to default).
- **Persistent viewport camera** — orbit position (yaw, pitch, distance, center) is saved with the project.
- **Master View output toggling** — show/hide each output window independently from `MasterViewPanel` (rendering and texture sharing are skipped for hidden outputs).
- **27-panel UI audit** — double status bar removed, complete auto-save (zone snapshots, outputs, surface map, network, effect rack, MIDI bindings now persisted), `SetEditorPanel` playback (cut/crossfade/fade transitions), `SurfaceEditorPanel` 8 resize handles, full `Ctrl+N` / `Ctrl+D` parity with menu actions, Console history (`Up`/`Down`), `MidiActivity` device filter, `MidiMapping` Clear All confirmation, shortcuts dialog corrected.

---

## Table of contents

1. [Getting started](#1-getting-started)
2. [Anatomy of the interface](#2-anatomy-of-the-interface)
3. [Main menu bar](#3-main-menu-bar)
4. [The 3D Viewport](#4-the-3d-viewport)
5. [The Node Editor](#5-the-node-editor)
6. [The Inspector](#6-the-inspector)
7. [The Timeline](#7-the-timeline)
8. [The Scene Hierarchy](#8-the-scene-hierarchy)
9. [The Preset Browser](#9-the-preset-browser)
9bis. [The Asset Browser (v3.5.1)](#9bis-the-asset-browser-v351)
10. [Set Editor & Performance Mode](#10-set-editor--performance-mode)
11. [Audio & MIDI](#11-audio--midi)
12. [Video (Theora)](#12-video-theora)
13. [Shader Gallery & Material Editor](#13-shader-gallery--material-editor)
14. [Compositor Stack](#14-compositor-stack)
15. [Console & Debugger](#15-console--debugger)
16. [Project management](#16-project-management)
17. [Workflow tutorials](#17-workflow-tutorials)
18. [Plugins & Community (v3.5)](#18-plugins--community-v35)
19. [Gamepad (v3.5)](#19-gamepad-v35)

**Appendices:**

- [A. Complete node reference](#appendix-a--complete-node-reference)
- [B. All keyboard shortcuts](#appendix-b--all-keyboard-shortcuts)
- [C. Preset catalog](#appendix-c--preset-catalog)
- [D. Template catalog](#appendix-d--template-catalog)
- [E. Glossary](#appendix-e--glossary)
- [F. Troubleshooting / FAQ](#appendix-f--troubleshooting--faq)

---

## 1. Getting started

### 1.1 First launch

When launched (on Windows: `build/windows-debug/Debug/bbfx-studio.exe`), a centered **splash window** titled **"BBFx Studio"** appears. It contains:

- Coloured title: `BBFx Studio v3.5.1 Asset Library & Polish`
- Subtitle: *Curated assets, post-process pipeline, asset browser, effect rack*
- Welcome message
- Two buttons:
  - **"New Empty Project"** — closes the splash, opens an empty project
  - **"Open Project..."** — closes the splash; use **File → Open...** next to load an existing project

The splash is a modal, non-movable window. Clicking the cross also closes it.

### 1.2 Crash recovery (Autosave)

If the previous session ended abnormally, the **"Recover Autosave"** dialog appears on start-up:

- Yellow banner: *"Previous session may have crashed."*
- Shows the path of the autosave file found
- Two buttons:
  - **Recover Autosave** — reloads the autosave as current project
  - **Ignore** — declines recovery and proceeds to the usual splash

Autosave interval is configurable in **File → Settings...** (default: every N seconds).

### 1.3 Session lock file

On start-up the Studio creates a `.bbfx_lock` file in its working directory and removes it on clean shutdown. This file is used to detect a previous crash (to trigger the **Recover Autosave** dialog). You do not need to touch it; delete it manually only if a stale lock blocks launch after a hard power-cut.

### 1.4 Drag-and-drop a file

BBFx Studio accepts files dropped on the main window:

- `.bbfx-project` → loads the project (same as **File → Open...**)
- `.lua` → runs the script in the embedded Lua interpreter

### 1.5 Command-line arguments

A Lua script can be passed as argument (executed on start-up):

```
bbfx-studio.exe lua/demos/demo_studio.lua
bbfx-studio.exe lua/dbg_autotest.lua
```

Typically used to load a demo, run a sequence of debug commands at launch, or automate a test run.

---

## 2. Anatomy of the interface

### 2.1 Overview

The Studio layout consists of three horizontal regions:

1. **Menu bar** (top): File, Edit, View, Connect, Stage, Help.
2. **Main workspace** (dockspace): holds every resizable panel (Viewport, Node Editor, Inspector, Timeline, etc.).
3. **Status bar** (bottom): real-time indicators (FPS, node count, audio, MIDI, output, active scene).

### 2.2 Docking and panel layout

The dockspace uses **Dear ImGui docking**. You can:

- **Move a panel**: click-drag its title bar. Anchor zones appear (top, bottom, left, right, center) — release to dock.
- **Create tabs**: drop a panel onto the center of another → they share a single tabbed container.
- **Detach**: drag a panel out of any anchor zone → floating window on the desktop.
- **Resize**: hover the separator between two panels, click-drag.
- **Close**: click the cross in the panel title bar. Re-open from **View** (or **Connect** / **Stage** depending on category).
- **Hide temporarily**: every **View** entry is a checkbox.

Layout is persisted across sessions (file `imgui.ini` next to the executable).

### 2.3 Status bar (bottom of the window)

Left to right, separated by vertical bars:

| Indicator | Description |
|---|---|
| **FPS: N** | Interface frames per second |
| **Nodes: N** | Number of nodes currently in the graph |
| **Links: N** | Number of connections between nodes (refreshed every second) |
| **Audio: On / Off** | Green when an `AudioAnalyzerNode` is active; grey otherwise |
| **MIDI: N dev, N bind** | Green: detected MIDI device count + active binding count. Grey *Off* if no device. |
| **Output: On / Off** | Blue when at least one output window (projector) is open. Hover tooltip: *"Active output windows: N — Ctrl+Shift+O → Output Manager"*. |
| **[SCN]** | Blue: Performance Mode is replaying a scene (keyboard chord or trigger). Hover tooltip: *"Scene: chord 'X' (N zones)"*. Grey *"Scene: inactive"* otherwise. |
| **`*`** (yellow) | Appears when the project contains unsaved changes. |

### 2.4 Tooltips and contextual help

Many UI elements show a hover tooltip (parameters, buttons, status indicators). When a shortcut exists, it is shown inside the tooltip or to the right of the menu entry.

### 2.5 Theme

The Studio uses the default Dear ImGui dark theme. There is no theme selector in Settings — the only customisable appearance is **Font size** (**File → Settings... → Font size**, range 10–24).

---

## 3. Main menu bar

Six menus. All entries are listed below in their on-screen order. Shortcuts appear to the right of the entries that have one — the exhaustive list is in [Appendix B](#appendix-b--all-keyboard-shortcuts).

### 3.1 File

| Entry | Shortcut | Action |
|---|---|---|
| **New** | Ctrl+N | New empty project: clears non-singleton nodes and resets the project path. |
| **Open...** | Ctrl+O | Opens a native file picker to load a `.bbfx-project` file. |
| **Save** | Ctrl+S | Saves to the current path; if none, writes to `project.bbfx-project`. |
| **Save As...** | — | Forces a *Save As...* with file picker. |
| **Recent Projects ▶** | — | Submenu listing recently opened projects — click to reload directly. |
| **Export...** | Ctrl+E | Opens the video export dialog (see chapter 16). |
| **Settings...** | Ctrl+, | Opens the settings dialog (see 3.7). |
| **Exit** | Alt+F4 | Cleanly exits (removes the session lock). |

### 3.2 Edit

| Entry | Shortcut | Action |
|---|---|---|
| **Undo** | Ctrl+Z | Undoes the last command (greyed out when nothing to undo). |
| **Redo** | Ctrl+Y | Redoes the last undone action (greyed out when nothing to redo). |

Every modification routed through the **Command Manager** is undoable: create/delete node, create/delete link, parameter change via Inspector, duplicate, position change in the Node Editor, etc. Full history is visible in **View → Undo History**.

### 3.3 View

This menu controls visibility of the "general" panels. Each entry is a checkbox:

| Entry | Shortcut | Opens |
|---|---|---|
| **Viewport** | — | The interactive 3D render (see chapter 4) |
| **Node Editor** | F7 | The DAG graph editor (see chapter 5) |
| **Inspector** | F3 | Properties panel for the selected node (see chapter 6) |
| **Timeline** | F4 | Timeline / keyframes / BPM (see chapter 7) |
| **Preset Browser** | F6 | Preset library (see chapter 9) |
| **Asset Browser** | Ctrl+Shift+A | Library navigation: meshes, textures, materials, shaders, particles, effects, presets, templates (v3.5.1) |
| **Console** | F2 | Lua REPL and logs (see chapter 15) |
| **Set Editor** | — | Set editor for live performance (see chapter 10) |
| **Scene Hierarchy** | F8 | Scene tree (see chapter 8) |
| **Compositor Stack** | — | Post-processing stack (see chapter 14) |
| **Effect Rack** | F9 | Standalone effect rack with MIDI/Gamepad/Keyboard learn (v3.5.1, see section 10.3) |
| **Shader Gallery** | — | Shader catalog (see chapter 13) |
| **Material Editor** | — | OGRE material editor (see chapter 13) |
| **Undo History** | — | Clickable undo/redo history |
| **Test Engine UI** | — | (Debug only) ImGui test engine automation UI |

Below a separator:

- **Use Editor Camera** — toggles between free editor camera (mouse) and DAG-driven camera (`CameraNode`).
- **Performance Mode** (F5) — fullscreen toggle for the live VJ performance view (trigger grid, faders, VU meters — see chapter 10).

### 3.4 Connect

Everything related to external interaction (MIDI / OSC):

| Entry | Action |
|---|---|
| **MIDI Activity** | Toggles the incoming MIDI activity panel (see chapter 11) |
| **MIDI Mapping** | Toggles the MIDI mapping panel (learn & bindings table) |
| **OSC Browser** | Toggles the incoming OSC address browser |
| **Load Mapping Preset ▶** | Submenu: loads a pre-configured MIDI mapping preset |
| **Save Mapping As...** | Saves the current mapping as a user preset |
| **Clear All Bindings** | Clears all active MIDI/OSC bindings |

### 3.5 Stage (v3.4)

Dedicated to multi-output / live / projection mapping:

| Entry | Shortcut | Action |
|---|---|---|
| **Output Manager** | Ctrl+Shift+O | Manages output windows (projectors): creation, resolution, monitor, warp, blend |
| **Surface Editor** | Ctrl+Shift+S | Edits zones (parts of the render sent to each output) |
| **Network Sync** | Ctrl+Shift+N | Master/slave network sync panel |
| **Master View** | Ctrl+Shift+M | Unified dashboard (outputs + network + scene) |
| **PANIC ALL** | Ctrl+Shift+P | Emergency: resets all warps, blends, network, DMX, Spout — no confirmation. |

> ⚠ **PANIC ALL** is an emergency live command. Use it if a calibration goes wrong during a show; it resets *all* warp/blend/network profiles in one click.

### 3.6 Plugins (v3.5)

Plugin ecosystem management:

| Entry | Shortcut | Action |
|---|---|---|
| **Plugin Manager** | Ctrl+Shift+X | Opens the Plugin Manager panel (installed plugins, enable/disable, install) |
| **Plugin Errors** | Ctrl+Shift+E | Opens the Plugin Errors panel (sandbox violations, load failures) |
| **Community Browser** | — | Opens the Community Browser (VS Code Marketplace-style, browse/install/rate plugins) |
| **Command Palette** | Ctrl+Shift+P | Quick command search (type to filter all available commands) |

### 3.7 Help

| Entry | Shortcut | Action |
|---|---|---|
| **About BBFx Studio** | F1 | Shows the *About* dialog: version, subtitle, authors |
| **Keyboard Shortcuts** | — | Opens the full shortcut table (mirror of [Appendix B](#appendix-b--all-keyboard-shortcuts)) |

### 3.8 Settings dialog

Opened via **File → Settings...** or **Ctrl+,**. Global parameters:

**General**
- **Auto-save interval (sec)** — 30 to 600 s. Autosave writes a backup file regularly for crash recovery.
- **Font size** — 10 to 24 px.

**Rendering**
- **Viewport scale** — 0.25 to 4.0. Scale factor for the internal render (>1: super-sampling, <1: faster render).
- **Default texture lighting** *(v3.5.1)* — `unlit` / `lit` / `emissive`. Sets the default `lighting_mode` for newly created `TextureNode`s. Each TextureNode can override locally via the Inspector. `unlit` = raw texture colours, `lit` = standard lighting/ambient, `emissive` = self-illuminated (texture is never darker than its source).

**Audio**
- **Default BPM** — 60 to 240 BPM. Fallback BPM when no `BeatDetector` node is active.

Buttons **Save** (applies + persists to the settings file) / **Cancel**. (v3.5.1: the dialog now uses a persistent edit buffer — combo boxes correctly remember selections between frames.)

---

## 4. The 3D Viewport

The Viewport is the interactive 3D render window. It displays the scene in *cover* mode (fills the panel without distortion, crops any overflow at the centre). Window title: **Viewport**.

### 4.1 Toolbar (top of the panel)

Left to right:

| Button | Role | Shortcut |
|---|---|---|
| **A:Move** | Translation tool — the object follows the translate gizmo | A |
| **E:Rot** | Rotation tool — 3-axis rotation gizmo | E |
| **R:Scl** | Scale tool | R |
| `\|` | *(visual separator)* | — |
| **World / Local** | Toggles transform space (global vs object-local) | — |
| **Snap** | Enables grid snapping (hold Ctrl for temporary snap); numeric field = snap step | Ctrl (held) |
| **Grid** | Shows/hides the ground grid | — |
| **Overlays** | Shows/hides overlays (gizmos, outlines, labels, axis indicator) | — |

Active buttons are coloured (green for Snap, blue for Grid, yellow for Overlays).

### 4.2 Rendering and overlays

- The rendered image fills the panel. If panel aspect ratio differs from the render target, the image is centered and cropped on the overflowing sides (no black bars).
- **Top-left overlay** (when *Overlays* is on): FPS, effective resolution (`WxH`), cyan **"Design Mode"** tag.
- **Bottom-left axis indicator**: three short red/green/blue lines (X/Y/Z) projected through the current camera orientation — a compass.

### 4.3 Mouse interactions

| Action | Effect |
|---|---|
| **Left-click** on an object | Selects the object, activates the gizmo, sets the orbit lock target |
| **Left-click** in empty space | Deselects |
| **Drag a gizmo handle** | Transforms the object with the active tool; holding **Ctrl** forces snap |
| **Alt + left-drag** | Orbits around the focal point |
| **Middle-drag** | Pan (camera lateral move) |
| **Mouse wheel** | Zoom |
| **Right-click held** + move | FPS camera: mouse look + ZQSD (WASD-like) movement; cursor captured and hidden |
| **Right-click held + Ctrl** | Lock-on: auto-targets the object at screen center, camera orbits around it (wheel = adjust distance) |
| **Wheel in FPS mode** | Adjusts move speed |
| **Right-click (no drag)** on object | Opens the **object context menu** |
| **Right-click (no drag)** in empty space | Opens the **Add Object** menu |
| **Double-click** | *(reserved for other panels — inactive in the viewport by default)* |

### 4.4 Object context menu

| Entry | Shortcut | Action |
|---|---|---|
| **Apply FX ▶** | — | Applies an effect: *PerlinFxNode*, *ShaderFxNode*, *WaveVertexShader* |
| **Duplicate** | Ctrl+D | Duplicates the object (creates a sibling node) |
| **Delete** | Del | Deletes the object |
| **Focus** | F | Centers the camera on the object |
| **Hide / Show** | H | Toggles visibility |
| **Lock / Unlock** | — | Locks/unlocks the object (prevents picker selection) |

### 4.5 Add Object menu (right-click on empty space)

**Add Object ▶** submenu listing every mesh available (enumerated by `ResourceEnumerator`). Click a mesh to create a `SceneObjectNode` at the right-click location (ray cast against the Y=0 plane, or 10 units in front of the camera as fallback).

### 4.6 Drag-and-drop into the Viewport

From other panels you can drop:

| Source | Payload | Result |
|---|---|---|
| **Shader Gallery → shader thumbnail** | `SHADER_NAME` | Applies the FX (same as *Apply FX* menu) to the hovered object |
| **Material Editor → material** | `MATERIAL_NAME` | Creates a `MaterialNode` bound to the hovered object |
| **Texture (anywhere)** | `TEXTURE_NAME` | Creates a `TextureNode` bound to the hovered object |
| **Particle (anywhere)** | `PARTICLE_NAME` | Creates a `ParticleNode` |
| **Compositor (anywhere)** | `COMPOSITOR_NAME` | Creates a `CompositorNode` |

### 4.7 Keyboard transform (gizmo keyboard mode)

When an object is selected:

| Key | Action |
|---|---|
| **G** | Enters keyboard translation mode (mouse modifies position without click) |
| **R** | Enters keyboard rotation mode |
| **S** | Enters keyboard scale mode |
| **X / Y / Z** (during keyboard mode) | Constrains the transform to the X / Y / Z axis |
| **Left-click** (during keyboard mode) | Confirms the transform |
| **Ctrl** (during keyboard mode) | Enables snap during the transform |
| **Escape** | Cancels the current transform |

### 4.8 Other Viewport shortcuts

| Shortcut | Action |
|---|---|
| **F** | Camera focus on the selected object |
| **Home** | Reset camera to default position |
| **Numpad 1** | Front view; **Ctrl+Numpad 1** = back view |
| **Numpad 3** | Right view; **Ctrl+Numpad 3** = left view |
| **Numpad 7** | Top view; **Ctrl+Numpad 7** = bottom view |
| **F11** | Toggles output window fullscreen (if open) |
| **+ / -** | BPM +1 / -1 (**Ctrl+** for ±5) — works from anywhere in the Studio |
| **Space** | Play/Pause animation |
| **Escape** (outside keyboard mode) | Exits Performance Mode or quits the Studio |

---

## 5. The Node Editor

The Node Editor is the visual graph where the project's animation is built. Nodes are drawn as boxes with input pins (left) and output pins (right), connected by Bézier curves. Window title: **Node Editor**.

### 5.1 Canvas navigation

| Action | Effect |
|---|---|
| **Mouse wheel** | Zoom in/out |
| **Right-drag** (in empty space) | Pan the canvas |
| **Double-click** on empty space | Opens **Quick Add** popup (see 5.4) |
| **Initial frame** | Auto-fit: after 3 frames, the canvas fits all nodes |

### 5.2 Minimap

Bottom-right of the Node Editor: a **minimap** shows the entire graph at reduced scale with a red frame indicating the current view. Click / drag on the minimap to center the main view on that area.

### 5.3 Selection and node movement

| Action | Effect |
|---|---|
| **Left-click** on a node | Selects this node (single selection) |
| **Ctrl + left-click** | Adds / removes the node from multi-selection |
| **Left-drag** in empty space | Selection rectangle — captures overlapping nodes |
| **Left-drag** on a node | Moves the node (or the whole multi-selection) |
| **Delete** | Removes all selected nodes (and their links) |

### 5.4 Quick Add (fast creation)

Triggered by **double-click on empty space** or **Ctrl+Space**. A floating popup opens with:

- Auto-focused search field (type to filter live)
- **Up/Down arrows** — change the highlighted entry
- **Enter** — creates the selected node at cursor position
- **Escape** — closes the popup

### 5.5 Context menus (right-click)

**Right-click on empty space** → **Create Node** menu: submenus by category (Scene, FX, Math, Audio, Video, Primitive, etc.), each listing the available types. Click to create at the right-click position.

**Right-click on a link** → **Delete Link**.

**Right-click on a node** → full menu:

| Entry | Enabled when | Action |
|---|---|---|
| **Enable / Disable** | always | Disables the node (runs no longer but stays in the graph) |
| **MIDI Learn Port ▶** | always | Submenu of input ports — click starts MIDI listen; `[CC#N]` suffix when already bound |
| **Detach Particle** | ParticleNode with entity link | Detaches the particle from its host object |
| **Add / Edit / Remove Comment** | always | Free-form note attached to the node (visible in the graph) |
| **Group (Ctrl+G)** | ≥ 2 selected | Groups them in a coloured named frame |
| **Remove from "X"** | node is group member | Removes from the group |
| **Collapse / Expand** | always | Shrinks / expands the node box |
| **Align ▶** | ≥ 2 selected | Top / Bottom / Left / Right |
| **Distribute ▶** | ≥ 2 selected | Horizontally (row) / Vertically (column) |
| **Apply FX ▶** | ≥ 2 SceneObjectNodes | Applies the FX (PerlinFxNode / WaveVertexShader) to all at once |
| **Save as Preset** | always | Opens a modal to save the selection as a reusable preset (name + description) |

**Right-click on a group** (coloured frame):

| Entry | Action |
|---|---|
| Hue swatch `##hue` | Preview of group colour |
| **Select all members** | Selects every node in the group |
| **Add selection to group** | Adds the current selection to the group |
| **Ungroup (keep nodes)** | Removes the group, keeps the nodes |
| **Delete group + all nodes** | Removes the group AND all its members |

### 5.6 Creating links

Click-drag from an **output pin** (right of a node) to an **input pin** (left of another node) to create a link. The link appears as a Bézier curve coloured by signal type. An input pin accepts only one connection — a new link replaces any existing one.

**Smart wire (Ctrl+L)** — with exactly 2 nodes selected, tries to auto-link compatible ports (same name, `entity→entity`, `out→in`, etc.). If no match is found, a console message is printed.

### 5.7 Copy / paste / duplicate

| Shortcut | Action |
|---|---|
| **Ctrl+C** | Copies selected nodes to an internal clipboard |
| **Ctrl+V** | Pastes at cursor position (links internal to the selection are preserved) |
| **Ctrl+D** | Duplicates selected nodes (equivalent to Copy + Paste offset) |

### 5.8 View bookmarks

Lets you memorise up to 9 views (position + zoom) on the canvas:

| Shortcut | Action |
|---|---|
| **Ctrl+1 … Ctrl+9** | Saves current view to slot 1..9 |
| **1 … 9** | Restores the saved view (focus + zoom) |

Useful for quickly jumping between zones of a large graph.

### 5.9 Comments and groups

- **Comment**: text note attached to a node, displayed above the box. Editable from the context menu.
- **Group**: tinted frame enclosing several nodes with a customisable name. When you drag the group (its header), every member follows.

### 5.10 Collapse (node folding)

A *collapsed* node shows only its title (no ports, no contents) — useful to reduce visual clutter. Links remain functional.

### 5.11 Save as Preset (from context menu)

A **"Save as Preset"** modal opens:

- **Name** field (required)
- **Save** button — writes the preset to `lua/presets/<name>.lua` and refreshes the Preset Browser
- **Cancel** button — aborts

The preset saves the selected sub-topology (nodes + internal links + positions + parameter values).

---

## 6. The Inspector

The Inspector is the contextual properties panel: it shows the parameters of the currently selected node in the Node Editor or Viewport. Title: **Inspector**.

### 6.1 When nothing is selected

Shows a bullet list of every registered node (BulletText) — a handy overview.

### 6.2 Header of the selected node

At the top of the panel:

- **Node name** — editable via the *"Name"* field (press **Enter** to confirm).
- **Delete** button — opens a modal **"Delete node 'X'?"** (Yes / Cancel).

### 6.3 FX Stack (for scene objects)

When the selected node is a `SceneObjectNode`, an **FX Stack** section lists the effects applied to it:

- For each FX: **##en** checkbox (enable/disable), FX name, **X** button (remove)
- **+** button at the end of the list → **Quick Apply FX** popup to add a PerlinFxNode / ShaderFxNode / WaveVertexShader

### 6.4 Transform offsets (SceneObjectNode)

A dedicated block shows position, rotation, and scale **offsets** (applied on top of the DAG-driven transform):

- **DAG Priority** checkbox — when checked, the DAG transform overrides user offsets
- **Pos: x, y, z** + **X##resetPos** (reset position offset)
- **Rot: x, y, z** + **X##resetRot** (reset rotation offset)
- **Scale: x, y, z** + **X##resetScl** (reset scale offset)
- **Reset All** — resets every offset in one click

### 6.5 Parameter widgets

For each parameter (defined via `ParamSpec` in the node), the Inspector auto-generates the right widget:

| Parameter type | Widget | Details |
|---|---|---|
| **float** | Horizontal slider | Bounds `minVal` / `maxVal` from `ParamSpec` |
| **bool** | Checkbox | — |
| **string** (free text) | Text input | — |
| **texture** | *"Select texture..."* button → popup list | *Search...* field + thumbnail grid |
| **item picker** (mesh, material, particle) | *"Select..."* button → popup list | Text search, Selectable items |
| **enum / choices** | Combo box | `param.choices` defines the options |
| **color** | ColorEdit3 with hue wheel | Also shows harmony pills (complementary, 2 triads) as preview |
| **vec3** | DragFloat3 | Three drag-able numeric fields |
| **port input (float)** | Slider -10 .. +10 | For values arriving on an input port rather than a parameter |
| **port input (enum)** | Combo box | Same, port side |

**Right-click on a parameter** → **##paramCtx_\<name\>** popup with advanced options (reset, copy, MIDI Learn depending on type).

### 6.6 Node-type specific parameters

Some nodes add special widgets:

- **ShaderFxNode** — multi-line Lua source editor (`InputTextMultiline` **##luasrc**) to modify the shader source live; **Apply** button is greyed out until the text changes.
- **ParticleNode** — particle controls (count, emitter, lifetime) as sliders.
- **TextureNode** — texture preview + reassignment picker.
- **LightNode** — type (Point/Directional/Spot), intensity, colour.
- **CameraNode** — FOV, near/far, perspective/ortho mode.

The full parameter list per node type is in [Appendix A](#appendix-a--complete-node-reference).

### 6.7 Port handling (advanced)

Ports can show their current value below the widget (read-only) when the port is connected. A connected port displays the received value (not directly editable — change the source instead).

### 6.8 Actions at the bottom of the panel

- **Name** (InputText with Enter → Return) — rename the node
- **Delete** — confirms then deletes via the Command Manager (undoable)

---

## 7. The Timeline

Time control panel for the project: playback, recording, BPM, automation lanes, and keyframe curves. Window title: **Timeline**.

### 7.1 Transport row (top)

| Button | Role |
|---|---|
| **\>** / **\|\|** | Play / Pause (glyph toggles). Shortcut **Space**. |
| **[ ]** | Stop — resets time to 0 and pauses |
| **REC** | Record: captures port modifications on *armed* lanes. Red when active. |
| **RPL / OVR** (visible while REC) | Toggles *Replace* (overwrite) / *Overdub* (superpose). Tooltip reminds the mode. |
| **LOOP** indicator | Green if automation loop region is active |
| **"N lanes, N kf"** indicator | Total lane + keyframe count (grey) |
| **BPM** (InputFloat) | Tempo between 20 and 1200 BPM — editable with mouse, with **+/-** keyboard (Ctrl+/- = ±5) |

After stopping a record, armed lanes are automatically *thinned* (redundant keyframes removed); a console message reports the removal count.

### 7.2 Beat ruler and markers (upper area)

A beat/bar ruler with numbered markers, the current position (vertical cursor), and the loop region (coloured rectangle when active).

### 7.3 Chord track (band above lanes)

A horizontal track shows **chords** (musical blocks) with name and colour.

| Action | Effect |
|---|---|
| **+##addchord** | Adds a chord at the current position |
| **Drag a chord** | Moves the chord |
| **Drag an edge (InvisibleButton)** | Resizes the chord |
| **Right-click on a chord** | Opens **Chord Context Menu**: rename (InputText + Enter), Store Snapshot, Recall Snapshot, Delete |

*Store Snapshot* captures the current port state inside the chord; *Recall Snapshot* restores that state when the chord is played.

### 7.4 Automation lanes (central area)

A scrollable region (`##AutoLanes`) contains all automation lanes.

At the top:
- **+Lane** button — adds a new empty automation lane

For each lane, left to right:
- **M##mute** — mutes the lane (curve no longer applied)
- **A##arm** button (tooltip *Arm for recording*) — enables REC capture on this lane
- **±##col** — collapse / expand
- **Lane name** (Selectable) — click selects the lane (for grouped keyframe editing)

**Right-click on a lane** → menu:
- **Assign Port ▶** — tree submenu (per node → per port) to bind the lane to a DAG port
- **Generate LFO...** — generates a sine / triangle / square curve
- **Delete Lane** — removes the lane

### 7.5 Keyframes

Keyframes are drawn as diamonds along the curve.

| Action | Effect |
|---|---|
| **Left-click** on a keyframe | Selects this keyframe (clears previous selection) |
| **Shift + click** | Adds/removes from multi-selection |
| **Drag** on a selected keyframe | Moves it (time + value) |
| **Drag in empty lane** | Selection rectangle (captures overlapped keyframes) |
| **Double-click in empty lane** | Creates a keyframe at the click position |
| **Right-click on keyframe** | Opens **##kfCtx**: Interpolation (Linear / Ease In / Ease Out / Ease In-Out / Step / Bezier / Hermite), Delete Keyframe |

Tangent handles (Bezier/Hermite mode) are draggable separately.

### 7.6 Audio spectrogram

At the bottom of the Timeline, after a separator: the **audio spectrum band**. It shows the live frequency-band response (when an audio source is active via `AudioAnalyzerNode`). Visual reference for placing keyframes in sync with the music.

### 7.7 Recording (REC)

When **REC** is active:
- Every value written on ports *with an armed lane* is captured live → keyframes.
- Keyboard events are captured to a `session.bbfx-session` file (input recorder).
- **RPL** mode: replaces existing keyframes in the traversed range.
- **OVR** mode: overlays, preserving previous keyframes elsewhere.

After stopping, redundant keyframes are auto-simplified.

---

## 8. The Scene Hierarchy

Shows the tree of scene objects (types `SceneObjectNode`, `LightNode`, `ParticleNode`, `CameraNode`, `SkyboxNode`, `FogNode`, `CompositorNode`). Title: **Scene Hierarchy**. Shortcut: **F8**.

### 8.1 Display

Each scene node is shown on a row with, left to right:

| Element | Meaning |
|---|---|
| **O** / **o** | Visible / hidden (click toggles; tooltip *Hide* / *Show*) |
| **#** / **.** | Locked / unlocked (tooltip *Unlock* / *Lock*) |
| **Icon + name** (Selectable) | Click selects the node (synced with Node Editor / Viewport / Inspector) |

Icons: `[M]` Mesh, `[L]` Light, `[P]` Particle, `[C]` Camera, `[S]` Skybox, `[F]` Fog, `[X]` Compositor, `[?]` unknown.

### 8.2 Reparenting via drag-and-drop

For `SceneObjectNode` only:

- **Drag** a node → payload `HIERARCHY_NODE` (tooltip *Move X*).
- **Drop on another SceneObjectNode** → reparents (becomes child of the target).
- **Drop on empty window area** → unparents (back to root level).

Children are indented 20 px under their parent.

### 8.3 Context menu (right-click on item)

| Entry | Action |
|---|---|
| **Focus** | Centers the camera on the object (same as **F** key) |
| **Hide** / **Show** | Toggles visibility |
| **Lock** / **Unlock** | Toggles lock |
| **Delete** | Deletes the node (via deferred delete — one tick later) |

When no scene object exists, the panel shows *"No scene objects"* in grey.

---

## 9. The Preset Browser

Multi-section panel to browse presets, assets, the effect rack and Quick Access shortcuts. Title: **Presets**. Shortcut: **F6**.

Four stacked sections, separated by horizontal lines.

### 9.1 Available Presets (tree)

Scans `lua/presets/*.lua` on start-up, reads the `category` field of each file and groups presets into **CollapsingHeader** by category (with count in parentheses, e.g. *Audio (6)*).

| Action | Effect |
|---|---|
| **Left-click** on a preset | Selects the preset |
| **Drag preset** to the Node Editor | Payload `PRESET_NAME` — instantiates at drop point |
| **Drag to a Quick Access slot** | Assigns the preset to the slot (see 9.4) |
| **Right-click on a preset** | **Preset Context Menu**: *Add to Wheel* / *Remove from Wheel* (based on state) |

Category is cached after first scan. The full preset list is in [Appendix C](#appendix-c--preset-catalog).

> **v3.5.1** — The previous *Assets* section and *Quick Access* bar have been **removed** from the Preset Browser. Asset navigation has moved to the dedicated **Asset Browser panel** (`Ctrl+Shift+A`, see chapter 9bis), and the effect rack has been **extracted** into its own panel with proper bypass and learn modes (`F9`, see section 10.3). The Preset Browser now focuses purely on preset listing.

---

## 9bis. The Asset Browser (v3.5.1)

Dedicated panel for browsing, searching and dragging every kind of asset registered in the Studio. Title: **Asset Browser**. Shortcut: **Ctrl+Shift+A**. Available from the **View** menu and from the *Open Asset Browser* entry of the Node Editor context menu.

### 9bis.1 Layout

The panel is split into two regions:

- **Sidebar** (left) — categories and tags
- **Main area** (right) — toolbar (search bar + Grid/List toggle) and grid/list of assets

### 9bis.2 Categories

The sidebar lists 9 categories with live counts:

| Category | Source | Approx. count |
|---|---|---|
| **All** | Aggregation of every category | ~ 200+ |
| **Meshes** | `MeshManager` (incl. procedural via iterator) — `.mesh` files | 32 |
| **Textures** | OGRE texture pool | varies |
| **Materials** | OGRE materials (incl. `BBFx/*` VJ materials) | 8 BBFx + 5 skybox + library |
| **Shaders** | `.frag` / `.vert` files in `resources/shaders/` | 58 |
| **Particles** | Particle templates (`BBFx/*`, `Examples/*`) | 23 |
| **Effects** | Post-process compositors / `getAvailableEffects()` | 29 |
| **Cameras** | (reserved) | — |
| **Presets** | `lua/presets/*.lua` | 93 + 6 aliases |
| **Templates** | `lua/templates/*.lua` | 14 |

Click a category to filter. **All** shows everything by default.

### 9bis.3 Search and tags

- **Search field** (top of the main area) — instant filter on **name + description + tags**, case-insensitive substring match. Refreshes within 16 ms even with 200+ assets.
- **Tags** in the sidebar (under categories) — each asset is tagged (`geometry`, `audio`, `beat`, `organic`, `glitch`, `retro`, `glow`, `abstract`, `composition`, `flagship`, `postprocess`, `camera`, `particle`, `shader`, etc.). Multi-select uses **AND** (intersection).
- The 3 filters (category + tags + search) **stack** — assets must match all simultaneously.

### 9bis.4 Grid view (default)

64×64 thumbnails arranged in a responsive grid. Default thumbnails are generic icons per type (mesh = cube, texture = image swatch, shader = code icon, etc.). The number of columns adapts to the panel width. Scroll is virtualized via `ImGuiClipper` for fluid 60 fps with hundreds of assets.

### 9bis.5 List view

Toggle the **Grid/List** button in the toolbar. Each row shows: name + type + description on a single line. Same data, easier to read for long lists.

### 9bis.6 Tooltip

Hovering an asset shows a tooltip with:

- Name + type
- Description
- For **meshes**: vertex / triangle count when available
- For **shaders**: list of uniforms parsed from the file
- For **presets**: number of nodes in the graph

### 9bis.7 Favorites

Right-click an asset → context menu **Add to Favorites** / **Remove from Favorites**. A **Favorites** section is pinned to the top of the grid, separated from the rest by a visual divider. Favorites are persisted in the user preferences (`settings.json`).

### 9bis.8 Drag and drop

Drag any asset out of the panel onto a compatible target. Eight typed payloads are emitted (aligned with `ViewportPanel` and `NodeEditorPanel`):

| Payload | Source asset type | Targets and effect |
|---|---|---|
| `MESH_NAME` | Mesh | **Viewport** → creates a `SceneObjectNode` at the drop position (raycast). **Node Editor** → creates a `SceneObjectNode` at the drop point with `mesh_file` pre-set. |
| `TEXTURE_NAME` | Texture | **SceneObjectNode** (Viewport or Node Editor) → creates a `TextureNode` linked to that scene object. |
| `MATERIAL_NAME` | Material | **SceneObjectNode** → creates a `MaterialNode` linked to that scene object. |
| `SHADER_NAME` | `.frag` / `.vert` | **SceneObjectNode** → creates a `ShaderFxNode` via `dbg.create_with_shader`. |
| `PARTICLE_NAME` | Particle template | **Viewport** / **Node Editor** → creates a `ParticleNode` with the `template` parameter pre-set (uses `dbg.create_with_param`). |
| `COMPOSITOR_NAME` | Post-process effect | **Node Editor** → creates a `PostProcessNode` with `compositor` pre-set. |
| `PRESET_NAME` | Preset | **Viewport** / **Node Editor** → instantiates the entire graph via `dbg.preset()`. |
| `TEMPLATE_NAME` | Template | **Node Editor** → loads the template via `dofile`. |

### 9bis.9 Double-click actions

Double-clicking an asset performs the same action as the typical drop target:

| Asset type | Action |
|---|---|
| Mesh | Creates a `SceneObjectNode` in the active scene |
| Preset | Instantiates the preset (same as drop on Viewport) |
| Particle | Creates a `ParticleNode` with the chosen template |
| Effect | Creates a `PostProcessNode` |
| Template | Loads the template |
| Texture / Material / Shader | Creates the corresponding node with the asset pre-filled |

### 9bis.10 Default layout

The Asset Browser is **enabled by default** and docked alongside the Timeline (tabbed in the bottom dock). The first launch sets the default layout; subsequent launches restore the user's customised layout (saved on every `Ctrl+S`).

---

## 10. Set Editor & Performance Mode

Two distinct but complementary tools for preparing and running a live set.

### 10.1 Set Editor

VJ set structuring panel (sequence of segments). Title: **Set Editor**.

Header: *"VJ Set List"* (grey) + two buttons:

| Button | Action |
|---|---|
| **+ Add Segment** | Adds a segment named *Segment N* |
| **Play Set** | Starts the set playback; toggles to *Stop* while playing. A green *"PLAYING: \<name\>"* label appears on the right when active |

Each segment is a collapsible **TreeNode** (green background when currently playing) with:

| Field | Type | Range |
|---|---|---|
| **Name** | InputText | — |
| **Source** | InputText | Lua path or chord name to play |
| **Duration (bars)** | SliderInt | 1 to 128 |
| **BPM** | SliderFloat | 40 to 200 |
| **Transition** | Combo | `cut` / `crossfade` / `fade_in` / `fade_out` |
| **Trans. beats** | SliderInt | 1 to 16 |
| **Notes** | InputTextMultiline | — |
| **Delete** button | — | Removes the segment |

**Reorder**: drag a segment up/down → swaps with its neighbour.

Bottom: with at least one segment, **<< Prev** and **Next >>** for manual navigation.

The set is saved/loaded as JSON (`saveSet` / `loadSet` methods, triggered by export extensions or the Lua API).

> **v3.5.1** — Set Editor playback is fully implemented: the *Play Set* button now drives `update(deltaTime)` with beat accumulation, auto-advance to the next segment on duration completion, transitions (cut / crossfade / fade_in / fade_out via alpha callback), per-segment BPM application, **Stop** button, and a progress bar of the current segment (bar X / Y). Set lists are also persisted across launches.

### 10.2 Performance Mode (F5)

**Performance Mode** is the fullscreen view dedicated to live performance. Toggled with **F5** or **View → Performance Mode**. Pressing **F5** again or **Escape** exits.

**Layout** (decoration-less window, full screen):

```
┌──────────────────────────────┬────────────────┐
│                              │ Trigger Grid   │
│     OGRE Viewport            │ (16 per page)  │
│     (80% of width)           │ ─────────────  │
│                              │ Faders (8)     │
│  Preset Wheel (top-left)     │ ─────────────  │
│                              │ Crossfader     │
│                              │                │
│       [ PANIC ]              │                │
├──────────────────────────────┴────────────────┤
│   VU meters    BPM overlay                    │
└───────────────────────────────────────────────┘
```

#### 10.2.1 Trigger Grid (top-right)

- 4×4 grid = **16 triggers per page**, multiple pages possible.
- **+##addPage** button to add a page.
- **Tab** cycles pages in order.
- Each trigger is a 56×48 px button, coloured by its `hue` parameter. Active state is shown visually.

**Keyboard shortcuts**:
- **1 to 9**: toggle triggers 1 to 9 of the current page
- **Q W E R T**: toggle triggers 10 to 14
- (triggers 15-16: mouse or MIDI only)

**Right-click a trigger** → context menu:

| Entry | Effect |
|---|---|
| **Enable Node ▶** | Submenu of DAG nodes — enables the selected node |
| **Disable Node ▶** | Same, disables |
| **Toggle Compositor ▶** | Toggles a compositor from a list |
| **Set Param ▶** | For each node / port, **Assign** (bind current value) or **Reset** |
| **Load Preset ▶** | Loads a preset on trigger fire |
| **Edit Macro ▶** | Macro editor: action list (`##macroAction` InputText), **X##del** to delete an action, **+ Add Action** to add one |
| **Clear Snapshot** / **Capture Current as Snapshot** | Port snapshot attached to the trigger |
| **Clear Scene** / **Capture Scene** | Same for a full scene |
| **MIDI Learn** / **Cancel MIDI Learn** / **Clear MIDI** | Binds or unbinds a MIDI Note to the trigger |
| **Hue** (SliderFloat 0-1) | Button colour |
| **Momentary** (Checkbox) | Momentary mode (release = off) vs toggle |
| **Clear** | Wipes the trigger configuration |

#### 10.2.2 Faders (middle-right)

**8 vertical faders** (`VSliderFloat`, 44×100 px). Each:

- Float value bounded by `minVal`/`maxVal`
- **M##midi\<i\>** button for MIDI Learn (binds a CC)
- **Right-click a fader**:
  - **Quick Assign ▶** — pre-filled list of *commonly automated* ports
  - Per-node → per-port submenu for manual assignment
  - **Clear** — unbinds the fader

**Keyboard shortcut**: **M** triggers MIDI Learn on the hovered fader.

#### 10.2.3 Crossfader

A horizontal 0-100 % fader with:
- **##cfActive** checkbox — enables crossfade
- **##crossfade** slider (0-100 %)
- **Auto** button — launches an automatic fade over N beats

#### 10.2.4 Preset Wheel (top-left of the viewport)

Interactive circular wheel showing presets marked *"Add to Wheel"*. Each preset occupies a sector. Click a sector = instant activation.

Available popup menu:
- **Capture Current** — captures the current state as a wheel snapshot
- **Presets ▶** — submenu to add a preset manually

#### 10.2.5 VU Meters and BPM overlay (bottom)

- Horizontal **VU meters** show the live audio level (several bands depending on `AudioAnalyzerNode` config).
- The **BPM overlay** shows the current tempo (large digits) + a beat indicator that flashes on every detected kick.

#### 10.2.6 PANIC button (bottom center of viewport)

Red **PANIC** button (120×36 px) — click = emergency reset on Performance Mode effects / state (*different* from the global `PANIC ALL` in the Stage menu, which also resets warp/blend/DMX/Spout).

#### 10.2.7 MIDI in Performance Mode

When a MIDI device is connected:
- Incoming CCs update assigned faders and push the value to the bound DAG port.
- Note On/Off fire assigned triggers (respecting *Momentary* config).
- Timeline REC captures received values with their beat.

#### 10.2.8 MacroRunner

Macros (lists of actions on a trigger) run *beat-gated*: each action waits for the current beat to reach the target before executing. Syntax `wait:N` = wait N beats before the next action.

### 10.3 Effect Rack (v3.5.1)

Standalone panel that lists every node currently registered in the graph and lets you toggle each one on/off in real time, with optional MIDI / Gamepad / Keyboard bindings per row. Title: **Effect Rack**. Shortcut: **F9**. Available from the **Connect** menu.

The Effect Rack used to live inside the Preset Browser; in v3.5.1 it is a fully autonomous panel with proper bypass and learn modes. Bindings remain active **even when the panel is closed**.

#### 10.3.1 Per-row controls

Each node appears on its own row with the following controls (left to right):

| Control | Behaviour |
|---|---|
| **LED dot** | Round dot showing the node state — **green** when active, **red** when bypassed. Dimmed/grey text when bypassed. Pattern matches the role indicators in MasterView. |
| **Checkbox** | Toggles the node's `enabled` flag. The action is **undoable** via `SetEnabledCommand` (same command as the Node Editor right-click *Disable* — both stay in sync). |
| **Node name + type** | Display name of the node (instance name + type label). |
| **M (MIDI Learn)** | Click to enter MIDI learn mode for this row, then move a fader / press a pad. Once captured the button shows a badge such as `[CC42]` or `[N60]`. CC > 0.5 → enable, CC ≤ 0.5 → disable. Note Down → enable, Note Up → disable. Uses `MidiLearnManager` with target type `rack_toggle`. |
| **G (Gamepad Learn)** | Click then press a button on any connected gamepad. Badge shows the source token, e.g. `[buttonA]`. Application uses front-edge detection (one toggle per press). |
| **K (Keyboard Learn)** | Click then press any keyboard key. Badge shows the bound key, e.g. `[F]`. Front-edge detection. The handler runs **before** the global shortcut dispatcher, so any key can be bound — a yellow tooltip warns when binding a globally-used key (F1-F8, Space, Escape, etc.). The handler respects `WantCaptureKeyboard` (no toggle while typing in a text field). |

#### 10.3.2 Persistence

- **Keyboard and gamepad bindings** are saved with the project in `extraJson["effectRack"]` (key codes as integers, gamepad source tokens as strings).
- **MIDI bindings** are managed by `MidiLearnManager` and saved in the project's MIDI bindings section.

#### 10.3.3 Live performance use

Bindings continue to work whether the Effect Rack panel is visible, hidden, or used in Performance Mode. The bindings update tick is unconditional: enable/disable a node by hitting the bound MIDI CC, gamepad button, or keyboard key from anywhere in the application.

---

## 11. Audio & MIDI

External interaction is handled by four dedicated panels, all accessible from the **Connect** menu. Audio itself is driven via **nodes** in the DAG and visualised in the Timeline (spectrogram) and Performance Mode (VU meters).

### 11.1 Audio — flow and control

BBFx has no standalone audio panel. Audio enters the system via nodes:

| Node | Role |
|---|---|
| **AudioCaptureNode** | Captures an audio input (microphone, interface) as raw stream |
| **AudioAnalyzerNode** | Live FFT analysis — outputs a spectrum (frequency bins) + bass / mid / high levels |
| **BeatDetectorNode** | Detects kicks / pulses → outputs a *beat* boolean + estimated BPM |
| **BeatTriggerNode** | Beat-based trigger usable as a trigger inside the graph |

To enable audio:

1. Open the **Node Editor** (**F7**)
2. **Right-click → Create Node → Audio → AudioCaptureNode**
3. Link its output to an **AudioAnalyzerNode** then to a **BeatDetectorNode**
4. The status bar turns to **Audio: On** (green)
5. The **Timeline** shows the spectrogram at the bottom
6. **Performance Mode** (F5) displays the VU meters and BPM overlay

The **Default BPM** parameter in the Settings dialog (60-240) is used only when no `BeatDetector` is active.

### 11.2 MIDI Activity (Connect → MIDI Activity)

**MIDI Activity** window — live monitor of incoming MIDI messages.

Header:
- **Messages: N \| Rate: N.0/s** — total count + instant rate (per second)
- **Clear** button — wipes the log and resets the count

Filters:
- **Ch Filter** (InputInt, 0-16) — 0 = no filter; otherwise shows only the chosen channel

Scrollable log (truncated to `kMaxEntries`, several thousand):

| Colour | Type |
|---|---|
| **Green** | NoteOn / NoteOff (displayed `Ch\<N\> NoteOn C4 vel=127`) |
| **Blue** | Control Change (`Ch\<N\> CC CC#74=64`) |
| **Yellow** | Program Change |
| **Grey** | PitchBend, Aftertouch, etc. |

The log auto-scrolls as new messages arrive.

### 11.3 MIDI Mapping (Connect → MIDI Mapping)

**MIDI Mapping** window — centralised binding management.

Two states:

**1. Learning** — magenta banner *"LEARNING: waiting for MIDI input → \<target\>"* + **Cancel** button. The next MIDI message captures and creates the binding.

**2. Idle** — three quick-learn buttons:
- **Learn Fader** — prepares listening for a fader (type `fader`, default index 0)
- **Learn Trigger** — same for a trigger
- **Learn Port** — same for a DAG port (target is narrowed later)

Below a separator: **"Bindings: N"** indicator and a sortable **bindings table**:

| Column | Contents |
|---|---|
| **Type** | `cc` (blue) or `note` (green) |
| **CC/Note** | Number (0-127) |
| **Ch** | MIDI channel (1-16) |
| **Target** | `Fader N`, `Trigger N`, or `<node>.<port>` |
| **Min** | Floor value (DragFloat 0-1, step 0.01) |
| **Max** | Ceiling value (DragFloat 0-1, step 0.01) |
| **X** | Remove-row button |

Bottom: **Clear All Bindings** button — removes all bindings at once (same as **Connect → Clear All Bindings**).

### 11.4 OSC Browser (Connect → OSC Browser)

**OSC Browser** window — tree explorer of received OSC addresses.

- **"Discovered addresses: N"** indicator + **Clear** button
- When empty: *"No OSC messages received yet"* in grey
- Otherwise: hierarchical tree built from `/a/b/c` style addresses
  - Internal nodes: collapsible **TreeNode**
  - Leaves: **Selectable**. Click = copies the full address to the clipboard. Hover tooltip.

Useful to discover addresses sent by a device (TouchOSC, Lemur, etc.) before binding them to ports via MIDI Learn equivalent or an `OscInputNode`.

### 11.5 MIDI Learn from the Node Editor

From the **Node Editor**, the **node context menu** exposes **MIDI Learn Port ▶**: a submenu listing every input port of the node with `[CC#N]` annotation when already bound. Click to enter Learn mode (display `>> portname <<`), the next received CC/Note creates the binding. Click again to cancel.

Same mechanism via **right-click on a fader** or **trigger** in Performance Mode (see chapter 10).

### 11.6 Load / save mappings

From the **Connect** menu:

- **Load Mapping Preset ▶** — submenu listing available MIDI preset files (*.json*)
- **Save Mapping As...** — saves the current mapping as a new file

The menu also offers **Clear All Bindings** to start from scratch.

---

## 12. Video (Theora)

Video is integrated via the **TheoraClipNode** node (**Ogg Theora** `.ogv` format). No dedicated video panel — everything goes through the node and the Inspector.

### 12.1 Inserting a video clip

1. Place an `.ogv` file in `resources/` (or a sub-folder mounted as OGRE resource).
2. In the Node Editor: **Right-click → Create Node → Video → TheoraClipNode**.
3. In the Inspector: **file** parameter (item picker) → pick your `.ogv`.

### 12.2 TheoraClip parameters (Inspector)

| Parameter | Widget | Role |
|---|---|---|
| **file** | Picker | Path to the `.ogv` file |
| **loop** | Checkbox | Auto-restart at end |
| **playback_speed** | Float slider | Playback speed (1.0 = normal; negative = reverse if *reversable*) |
| **volume** | 0-1 slider | Volume of the embedded audio |
| **start_frame** / **end_frame** | Sliders | Playback bounds |
| **play / pause / stop** | Boolean ports | Drivable from DAG or MIDI |
| **seek** (float port) | Input port | Jump to a 0-1 ratio |

### 12.3 Output ports

| Port | Type | Role |
|---|---|---|
| **texture** | OGRE texture | Current video frame — connect to a `TextureNode` / `MaterialNode` / 3D object |
| **progress** | float 0-1 | Normalised playback progress |
| **beat** | bool | (when enabled) pulse on key frames |

### 12.4 Video texture crossfade

The internal `TextureCrossfader` class (used under the hood) lets you mix two `TheoraClipNode` instances via a **BlendNode** or a Set segment with *crossfade* transition.

### 12.5 Reversable clips

If the `.ogv` went through the seek-map generation pipeline (see external documentation), it becomes *reversable*: negative `playback_speed` plays backwards without slippage.

---

## 13. Shader Gallery & Material Editor

Two separate panels for **procedural shaders** (animated GLSL effects) and **OGRE materials** (surface properties).

### 13.1 Shader Gallery

**Shader Gallery** window — catalogue of the 8 procedural shaders in BBFx, with animated preview.

| Shader | Fragment file |
|---|---|
| **Plasma** | `plasma.frag` |
| **Voronoi** | `voronoi.frag` |
| **Mandelbrot** | `mandelbrot.frag` |
| **Truchet** | `truchet.frag` |
| **Flow Field** | `flowfield.frag` |
| **Tunnel** | `tunnel.frag` |
| **Reaction Diffusion** | `reaction_diffusion.frag` |
| **Sphere Trace** | `sphere_trace.frag` |

Grid display of 64×64 thumbnails (columns adapt to panel width). Each thumbnail shows a live animation.

| Action | Effect |
|---|---|
| **Left-click** | Selects the shader (blue border) |
| **Double-click** | Applies the shader to the selected scene object (via `mApplyCb` callback) |
| **Drag-and-drop** | Payload `SHADER_NAME` (frag file name) — droppable on the Viewport or a Node Editor object |
| **Hover** | Tooltip: full name + fragment file |

Panel header reminds: *"Double-click to apply shader to selected object"*.

### 13.2 Material Editor

**Material Editor** window — editor for an existing OGRE material.

**Empty state** (no material loaded): shows *"No material selected"* and *"Double-click a material in the browser to open it here."*

Once a material is loaded (name shown in cyan *"Material: \<name\>"*), the panel contains:

**1. Preview** — 128×128 px sphere rendered live with the current material (when `ShaderPreviewRenderer` is available).

**2. Colors** (section) — 4 × ColorEdit3 + 2 sliders:

| Control | Range |
|---|---|
| **Diffuse** (ColorEdit3) | diffuse colour |
| **Specular** (ColorEdit3) | specular colour |
| **Ambient** (ColorEdit3) | ambient colour |
| **Emissive** (ColorEdit3) | self-illumination |
| **Shininess** (SliderFloat) | 0 to 128 |
| **Alpha** (SliderFloat) | 0 to 1 |

Every change is **applied immediately** to the in-memory OGRE material (no *Apply* button).

**3. Textures** (section) — lists the material's texture unit states:

- *"No texture slots"* when empty
- Otherwise per slot: 48×48 thumbnail (via thumbnail cache) + label `Slot N: \<name\>`

**4. Material creation**

At the bottom: **##newmat** InputText (200 px wide) + **New Material** button. Typing a name and clicking creates an empty OGRE material and loads it in the panel.

---

## 14. Compositor Stack

**Compositor Stack** window — post-processing chain (OGRE Compositor) applied to the global render.

### 14.1 Display

Header: **"Compositor Chain (N)"** (grey) — compositor count.

When empty: **"No compositors in scene"** + drop zone for adding.

For each compositor, in application order (top = first applied → bottom = last applied):

| Element | Role |
|---|---|
| **::** (20 px button) | Move handle — click-drag to reorder (payload `COMP_REORDER`) |
| **O##eye / -##eye** | Visibility: **O** = enabled, **-** = bypass (grey) |
| **Compositor name** (text, grey if disabled) | — |
| **X##del** (right side) | Removes this compositor from the chain |
| **Inline sliders** | Up to 3 float parameters, edited directly |

**Shift + click on the eye** → **SOLO mode**: disables every other compositor, keeping only this one. A second Shift+click restores the previous state.

### 14.2 Adding a compositor

Two options:

1. **Drag-and-drop** from the **Preset Browser** (Compositors tab) or the **Shader Gallery** onto the *"Drop compositor here to add..."* zone at the bottom of the panel. The `COMPOSITOR_NAME` payload creates a `CompositorNode` and appends it to the chain.
2. From the **Node Editor**: Create Node → Compositor → pick a type.

### 14.3 Reorder

Drag the **::** handle of an item up or down. The order updates immediately and the compositor viewport is re-applied.

### 14.4 DAG integration

Each entry in the Compositor Stack is actually a **CompositorNode** in the DAG with two main parameters:

- **compositor** (string) — OGRE compositor name
- **enabled** (bool) — enabled/disabled

You can drive these parameters from the Timeline (keyframes), MIDI, or Lua — like any other node.

---

## 15. Console & Debugger

The Studio embeds a **Lua interpreter** accessible via the Console and a full **debug API** (`dbg.*`) runnable both from the console and from `.lua` scripts.

### 15.1 The Console

**Console** window. Shortcut: **F2**.

On open, the panel shows:

```
BBFx Studio Console v3.1
Type Lua expressions. Tab for autocompletion. help() for commands.
```

**Toolbar**:
- **Copy All** (SmallButton) — copies the whole output to the Windows clipboard
- **Clear** — wipes the output area

**Output area**: auto-scrolls on new lines.

- Lines containing *"error"* or *"Error"* are shown in **red**.
- Text is wrapped to the panel width.
- History bounded to `kMaxOutput` lines (several thousand).

**Input field** (bottom, always focused):

| Key | Effect |
|---|---|
| **Enter** | Executes the command, appends to history, clears the field |
| **Tab** | Autocompletion: proposes node names matching the typed prefix |
| **Escape** | Clears the current line |

Each executed command is prefixed with `bbfx> ` in the log. Non-nil results are prefixed with `--> `.

### 15.2 Built-in console commands

Added globally to the Lua runtime (callable without prefix):

| Command | Description |
|---|---|
| `help()` | Shows help in the console |
| `graph()` | Lists every node (with port values) and every link |
| `ports("nodeName")` | Details the inputs/outputs of a node |
| `set("node", "port", val)` | Sets the value of an input port |
| *any Lua expression* | Evaluated and its result is shown (`--> \<value\>`) |

### 15.3 The `dbg.*` API

Global module available from the console, Lua scripts (CLI arguments), and snippets. This is the reference API for scripting / automating the Studio.

Type `dbg.help()` in the console to show the complete list. Summary by category (full list in [Appendix B](#appendix-b--all-keyboard-shortcuts), dedicated section):

**Node and graph management:**

| Command | Role |
|---|---|
| `dbg.create(type, name)` | Creates a node |
| `dbg.create_with_param(type, name, param, val)` | Creates with an initial parameter |
| `dbg.create_with_shader(name, frag)` | Creates a ShaderFxNode with shader |
| `dbg.delete(name)` | Removes a node |
| `dbg.ui_delete(name)` | Removes via CommandManager (undoable) |
| `dbg.undo()` / `dbg.redo()` | Programmatic undo/redo |
| `dbg.set_enabled(name, bool)` / `dbg.is_enabled(name)` | Enable/disable |
| `dbg.preset(name)` | Instantiates a preset |
| `dbg.link(from, fport, to, tport)` / `dbg.unlink(...)` | Links |
| `dbg.set(node, port, val)` / `dbg.get(node, port)` | Port read/write |
| `dbg.list()` / `dbg.links()` / `dbg.types()` / `dbg.presets()` | Enumerations |
| `dbg.inspect(name)` | Node details |
| `dbg.clear()` | Empties the DAG |
| `dbg.screenshot("path.png")` | Viewport capture |
| `dbg.fps()` | Current FPS |
| `dbg.test()` | Internal test suite (34 assertions) |

**Editing and view:**

| Command | Role |
|---|---|
| `dbg.select(name)` / `dbg.select_nodes(...)` | Selection |
| `dbg.group(name)` / `dbg.ungroup(name)` / `dbg.list_groups()` | Groups |
| `dbg.comment(name, text)` / `dbg.get_comment(name)` | Comments |
| `dbg.collapse(name, bool)` / `dbg.is_collapsed(name)` | Collapse |
| `dbg.align(dir)` / `dbg.distribute(dir)` | Alignment / distribution |
| `dbg.camera_move(x,y,z)` / `dbg.camera_orbit(yaw,pitch)` | Camera |
| `dbg.transform(name, px,py,pz)` / `dbg.reparent(child, parent)` | Transforms |
| `dbg.material_edit(matName, prop, ...)` / `dbg.material_create(name)` | Materials |
| `dbg.shader_apply(frag, target)` | Shader on object |
| `dbg.mode("design")` / `dbg.mode("perf")` | Studio mode |
| `dbg.trace(name)` | Traces a node |

**MIDI (v3.3 / v3.4):**

`dbg.midi_devices`, `dbg.midi_open`, `dbg.midi_close`, `dbg.midi_inject`, `dbg.midi_monitor`, `dbg.midi_send`, `dbg.midi_poll`, `dbg.midi_clock_start/stop/status`, `dbg.midi_learn_fader/trigger/port`, `dbg.midi_learn_cancel`, `dbg.midi_bindings`, `dbg.midi_clear_bindings`.

**Performance Mode:**

`dbg.trigger_page`, `dbg.trigger_fire`, `dbg.trigger_set_macro`, `dbg.fader_assign`, `dbg.fader_get`, `dbg.crossfade_set`, `dbg.crossfade_auto`, `dbg.crossfade_capture_a`, `dbg.wheel_add`, `dbg.wheel_remove`, `dbg.wheel_fire`, `dbg.record_start`, `dbg.record_stop`.

**Outputs and projection (v3.4 Lots A-K):**

`dbg.output_open/close/fullscreen/resolution` (slot 0 compat), `dbg.output_add/remove/list` (multi), `dbg.output_warp(id, 8 points)`, `dbg.output_warp_reset`, `dbg.output_warp_panic`, `dbg.output_blend(id, L/R/T/B/gamma)`, `dbg.output_blend_reset`, `dbg.output_gridwarp(id, row, col, x, y)`, `dbg.output_gridwarp_reset`, `dbg.wizard_start/click/cancel/state`.

**Zones (Surface Editor):**

`dbg.zone_add(name, x, y, w, h)`, `dbg.zone_remove(id)`, `dbg.zone_assign(zoneId, outputId)`, `dbg.zone_list()`.

**Network (Network Sync):**

`dbg.sync_start(role)` (`"master"` / `"slave"` / `"standalone"`), `dbg.sync_stop`, `dbg.sync_peers`, `dbg.sync_role`, `dbg.sync_chord(name)`, `dbg.sync_beat(bpm, beat)`, `dbg.sync_panic`.

**Peripheral outputs:**

`dbg.spout_enable(id, name)` / `dbg.spout_disable(id)`, `dbg.ndi_status()`, `dbg.artnet_send(ip, uni, ...channels)`, `dbg.artnet_quick_assign(name)`.

**Scene Switcher and Master View:**

`dbg.scene_capture(name)`, `dbg.scene_apply(name)`, `dbg.scene_list()`, `dbg.master_view()`.

**PANIC:**

`dbg.panic_all()` — same as the **Stage → PANIC ALL** button (resets warp/blend/DMX/Spout/network).

**UI tests:**

`dbg.run_ui_tests()` — launches the ImGui Test Engine automated suite.

### 15.4 Undo History

**Undo History** window — shows the full undoable / redoable command history, clickable.

**Display**:
- Header: *"Undo stack: N \| Redo stack: N"*
- Upper zone: **Redo** entries (grey, oldest to newest)
- Coloured marker: **>>> Current <<<**
- Lower zone: **Undo** entries (most recent on top, highlighted blue)

**Interactions**:
- **Click a Redo entry**: redoes up to that position (chained redos)
- **Click an Undo entry**: undoes to that position (chained undos)

Each entry shows the command's `description()` — typically: *"Create node 'mynode'"*, *"Delete link …"*, *"Set param …"*, *"Reparent …"*, etc.

---

## 16. Project management

### 16.1 File format

A BBFx project uses the **`.bbfx-project`** extension. It is a JSON file serialising:

- The full graph (nodes + links + positions + parameters)
- Groups, comments, view bookmarks
- Animation data (lanes, keyframes, chords)
- Output configuration (slots, warp, blend, zones) for v3.4
- MIDI bindings
- Presets added to the Preset Wheel
- Preset Browser Quick Access slots
- Panel layout (via `imgui.ini` next to it)

### 16.2 New / Open / Save

From the **File** menu or shortcuts:

| Action | Shortcut | Behaviour |
|---|---|---|
| **New** | Ctrl+N | Clears non-singleton nodes, resets project path, window title = *"BBFx Studio v3.4"* |
| **Open...** | Ctrl+O | Native dialog with `*.bbfx-project` filter |
| **Save** | Ctrl+S | Writes to current path; if empty, writes to `project.bbfx-project` (current directory) |
| **Save As...** | — | Forces file picker, updates current path after write |
| **Exit** | Alt+F4 | Clean exit — removes `.bbfx_lock` |

The window title mirrors the loaded project path. The `*` in the status bar indicates unsaved changes.

### 16.3 Recent Projects

**File → Recent Projects ▶** submenu lists recently opened projects (up to ~10 entries, persisted in settings). Click to reload directly.

### 16.4 Autosave and recovery

The Studio periodically writes an **autosave file** (frequency configured in **File → Settings... → Auto-save interval**, 30 to 600 s). This file has the current project's name with an *autosave* suffix.

On start-up, if the `.bbfx_lock` file was not cleaned (previous abnormal termination) AND an autosave newer than the last explicit save exists, the **"Recover Autosave"** dialog opens:

- **Recover Autosave** → loads the autosave
- **Ignore** → declines, leaves the autosave in place (it will be overwritten at next autosave)

### 16.5 Video export (File → Export..., Ctrl+E)

Opens the modal **"Export Session"** dialog. Two states:

**Configuration**:

| Field | Type | Range | Role |
|---|---|---|---|
| **Output Directory** | InputText | — | Destination folder (created if missing) |
| **Session File (.bbfx-session)** | InputText | — | Upstream session file recorded via Timeline **REC** |
| **Width** | InputInt step 16 / 320 | 64 to 7680 | Render width |
| **Height** | InputInt step 16 / 180 | 64 to 4320 | Render height |
| **FPS** | InputInt step 1 / 10 | 1 to 120 | Frames per second |
| **Duration (s)** | InputInt step 1 / 5 | 1 to 3600 | Total duration |

Buttons:
- **Start Export** (200 px) — begins capture
- **Cancel** (100 px) — closes the dialog

**Export in progress**:

- Shows the current output directory
- Progress bar **"Frame N / Total"**
- Red **Cancel Export** button — stops the capture

The export produces a **PNG sequence** named `frame_000001.png`, `frame_000002.png`, etc. in the output directory. Converting to mp4/mov is done with an external tool (ffmpeg).

If a `.bbfx-session` file is provided, recorded input events (keyframes, MIDI, keys) are replayed during export to reproduce the performance exactly.

### 16.6 Recorded sessions (`.bbfx-session`)

Produced by the Timeline **REC** mode (chapter 7). JSON format containing:
- Keyboard events (press / release with timing)
- Port value changes with occurrence beat
- Received MIDI events

Played back via the **Session File** parameter of the export, or through `InputPlayer` (Lua API).

### 16.7 VJ Sets (JSON format)

Saved by the **Set Editor** (chapter 10) via `saveSet(path)` / `loadSet(path)` methods. Simple JSON format (version 1):

```json
{
  "version": 1,
  "segments": [
    { "name": "Intro", "source": "chord_intro", "duration_bars": 32,
      "bpm": 120.0, "transition": "crossfade", "transition_beats": 4,
      "notes": "..." },
    ...
  ]
}
```

### 16.8 Presets and templates

- **Presets** — stored in `lua/presets/*.lua`. Created via the Node Editor (*Save as Preset*) or edited manually. Listed in the Preset Browser. Full list in [Appendix C](#appendix-c--preset-catalog).
- **Templates** — starter projects in `lua/templates/`. Loaded on first launch or via **File → New from Template...** (depending on version). List in [Appendix D](#appendix-d--template-catalog).

### 16.9 Persistent settings

The **Settings** dialog (Ctrl+,) writes to a user file (`SettingsManager`). Persisted values:

- Auto-save interval (sec)
- Font size (10-24)
- Viewport scale (0.25-4.0)
- Default BPM (60-240)

Panel layout (docking) is persisted separately in `imgui.ini` next to the executable.

### 16.10 Lock file (`.bbfx_lock`)

Created on start-up, removed on clean exit. Its presence at start-up signals a previous crash → triggers autosave detection (see 16.4). You can delete it manually if it blocks launch after a system hang (brutal shutdown, kill -9, etc.).

---

## 17. Workflow tutorials

Five step-by-step tutorials covering the most common use cases. Each starts from an **empty project** (File → New).

### 17.1 Tutorial 1 — Rotate a 3D object on an animated curve

**Goal**: place a cube in the scene, animate its rotation over 4 beats with a smooth curve.

1. **Open the Viewport** (menu **View → Viewport** if hidden).
2. **Right-click in the Viewport void** → **Add Object ▶ → cube.mesh**. A cube appears at the origin.
3. The cube is auto-selected; check the **Scene Hierarchy** (**F8**) for entry `[M] cube_1`.
4. Open the **Node Editor** (**F7**). Node `cube_1` (SceneObjectNode) is visible.
5. Open the **Timeline** (**F4**). Set **BPM** to 120.
6. In the Timeline, click **+Lane** to create an automation lane.
7. **Right-click** the lane → **Assign Port ▶ → cube_1 → rotation_y**. The lane is now bound to the cube's Y rotation.
8. **Double-click** on the lane at beat 0 to create the first keyframe (value 0).
9. **Double-click** at beat 4, creating a second keyframe; **drag** its value to `6.28` (2π, full turn).
10. **Right-click** the second keyframe → **Interpolation ▶ → Ease In-Out**.
11. Press **Space** to start playback. The cube rotates continuously over 4 beats with an ease curve.
12. **File → Save** (Ctrl+S), name the project `cube_tour.bbfx-project`.

### 17.2 Tutorial 2 — Audio-reactive scene (sphere pulsing with the bass)

**Goal**: make a sphere's scale follow the incoming bass energy.

1. Plug in a system or microphone audio source. In the console (**F2**), type `dbg.fps()` to check the Studio is running.
2. **Right-click in the Viewport void → Add Object → sphere.mesh**.
3. Open the **Node Editor** (**F7**). Create the needed nodes:
   - **Right-click** → **Audio → AudioCaptureNode**
   - **Right-click** → **Audio → AudioAnalyzerNode**
4. **Link** the `AudioCaptureNode` **audio** output → `AudioAnalyzerNode` **audio** input.
5. Check the status bar: it should show **Audio: On** (green). If not, check the `AudioCaptureNode` parameters in the Inspector (device index).
6. Create a **Math → MapperNode**: it remaps one range to another.
7. **Link** the `AudioAnalyzerNode` **bass** (or `band_0`) output → `MapperNode` **in** input.
8. In the `MapperNode` **Inspector**: set `in_min=0`, `in_max=1`, `out_min=0.5`, `out_max=3.0`. The output will range from 0.5× to 3× depending on bass energy.
9. **Link** the `MapperNode` **out** output → `sphere_1` **scale_x** input (and/or **scale_y**, **scale_z**).
10. Play loud music: the sphere pulses with the bass. Watch the Timeline **spectrogram** to verify live activity.

### 17.3 Tutorial 3 — Apply a procedural shader, modulate via MIDI

**Goal**: cover a plane with a *Plasma* shader, and control its `speed` parameter with a MIDI knob.

1. Create a plane: **Right-click in the Viewport → Add Object → plane.mesh**.
2. Open the **Shader Gallery** (**View → Shader Gallery**).
3. **Drag** the *Plasma* thumbnail onto the plane in the Viewport. A `ShaderFxNode` is created and linked to the plane. The plane now shows the animated Plasma effect.
4. Open the **Inspector** — the `ShaderFxNode` appears in the plane's FX Stack. Unfold its parameters: you will see `speed` (slider 0-5) and `intensity`.
5. Plug in a MIDI controller (USB). Open **Connect → MIDI Activity** to verify messages are received.
6. In the **Node Editor**, **right-click the ShaderFxNode → MIDI Learn Port ▶ → speed**. The menu shows `>> speed <<`.
7. Turn a knob on the MIDI controller. The binding is created instantly (indicator disappears, replaced by `[CC#XX]`).
8. Fine-tune the range in **Connect → MIDI Mapping**: adjust **Min** and **Max** of the binding (e.g. 0 → 2.5).
9. **Connect → Save Mapping As...** to save the MIDI preset.

### 17.4 Tutorial 4 — Video clip as texture on an object

**Goal**: loop an `.ogv` video clip on a sphere.

1. Place `resources/videos/clip.ogv` (Ogg Theora format) in the resources folder.
2. **Right-click in the Viewport → Add Object → sphere.mesh**.
3. In the **Node Editor**: **Right-click → Video → TheoraClipNode**.
4. In the `TheoraClipNode` **Inspector**:
   - **file**: pick `clip.ogv`
   - **loop**: checked
   - **playback_speed**: 1.0
5. Create a **MaterialNode**: **Right-click → Scene → MaterialNode**.
6. **Link** the `TheoraClipNode` **texture** output → `MaterialNode` **diffuse_texture** input.
7. **Link** the `MaterialNode` **material** output → `sphere_1` **material** input.
8. The sphere now displays the looping video across its surface.
9. To drive playback: link a `BeatTriggerNode` → `TheoraClipNode` **play** input to start the video on a kick.

### 17.5 Tutorial 5 — Prepare a live performance (triggers + faders + set)

**Goal**: configure 3 keyboard triggers (*intro*, *drop*, *outro*), 2 MIDI faders driving shader parameters, and switch into Performance Mode.

**Preparation (Design mode):**

1. Open an already-populated project (e.g. load a VJ template).
2. Add 3 presets to the **Preset Wheel**:
   - In the **Preset Browser**, **right-click** on `scene_intro` → **Add to Wheel**
   - Same for `scene_drop` and `scene_outro`.
3. Build a **Set List** in the **Set Editor**:
   - Click **+ Add Segment** three times, rename to *Intro*, *Drop*, *Outro*
   - For each, set *Source* = matching preset name, adjust *Duration* and *Transition* (crossfade 4 beats recommended).
4. In the console, test a trigger:
   ```lua
   dbg.trigger_page(0)
   dbg.trigger_set_macro(0, 0, {"enable:scene_drop", "wait:4", "disable:scene_intro"})
   ```
   This fills trigger 1 of page 0 with a beat-gated macro.
5. Assign MIDI as needed: **Connect → MIDI Mapping → Learn Fader**, move a physical fader.

**During performance:**

1. Press **F5** to enter **Performance Mode**. The screen turns fullscreen: viewport on the left, triggers + faders + crossfader on the right.
2. **Tab** cycles trigger pages if you have several.
3. **Keys 1-9** and **Q-W-E-R-T** fire triggers; **M** (over a hovered fader) starts MIDI Learn.
4. The **Preset Wheel** (top-left corner) lets you play a preset directly by click.
5. **Escape** or **F5** again to exit Performance Mode.

**Emergency:** if a parameter goes wrong (broken warp, frozen DMX, saturated network), **Ctrl+Shift+P** or **Stage → PANIC ALL** resets everything instantly.

---

## 18. Plugins & Community (v3.5)

### 18.1 Plugin Manager (Ctrl+Shift+X)

The Plugin Manager is the central hub for all installed plugins.

**Installed tab:**
- Lists all discovered plugins with **state badges**: Enabled (green), Disabled (grey), Failed (red), Validated (blue)
- **Search bar** with sort options (name, author, state, date)
- **Context menu** (right-click a plugin): Enable, Disable, Uninstall, Show in folder, View errors
- **Bulk actions**: "Enable All" / "Disable All" buttons
- **Status bar** badge shows the count of active plugins (click opens the panel)

**Installing plugins:**
- **Drag & drop** a `.zip` file onto the Studio window
- **Community Browser** → click "Install" on any plugin
- **CLI**: `bbfx-studio.exe --install path/to/plugin.zip`
- **Deep link**: navigate to `bbfx://install/author.plugin-name` in a browser

A **Permission Prompt** (Chrome-style dialog) appears before installation, listing the permissions the plugin requests (network, filesystem, UI, MIDI, etc.). You must explicitly accept.

### 18.2 Community Browser

A VS Code Marketplace-style panel for discovering and installing community plugins.

**Layout (3 columns):**
1. **Sidebar** — Filter by: category, tags, author, license, rating, sort order
2. **Grid** — Plugin cards (256x256) with animated thumbnail on hover, name, author, rating stars, Install button
3. **Detail panel** — Full plugin information with tabs: README (rendered markdown), Screenshots, Changelog, Reviews

**Features:**
- **Featured section** at the top highlights curated plugins
- **Search** with real-time filtering
- **Install** button downloads, extracts, validates, and enables the plugin in one click
- **Rating** from GitHub Reactions API (live overlay on cards)
- **Author profiles** — click an author name to see all their published plugins

### 18.3 Plugin Authoring

**Export mode** (right-click a preset or output template → "Export as Plugin"):
- Auto-detects resources (shaders, textures, materials, particles)
- Auto-detects required permissions
- Generates manifest.json and plugin structure
- Creates a ready-to-publish ZIP

**New Plugin Wizard** (Plugins menu → "Create Plugin"):
1. Choose template (node generator, node FX, preset, shader, panel, output template)
2. Fill metadata (name, author, description, category, tags, license)
3. Select permissions
4. Studio generates the plugin skeleton and auto-enables it for development

**Hot reload**: changes to plugin Lua files are detected every 500ms and auto-reloaded (no restart needed during development).

### 18.4 Publishing to Community

1. **Plugins → Publish** (or `dbg.plugin_publish("my-plugin")`)
2. Studio authenticates with GitHub via **device flow** (displays a code + URL to enter)
3. Forks the community repository, creates a branch, commits plugin files, opens a PR
4. A GitHub Action CI validates the plugin automatically
5. Once merged, the plugin appears in the Community Browser for all users

### 18.5 Command Palette (Ctrl+Shift+P)

Type-ahead search for all available commands (like VS Code). Includes:
- All menu actions
- Plugin-registered commands
- Debugger commands
- Node creation shortcuts

### 18.6 Deep Links

BBFx supports `bbfx://` URL scheme (registered in Windows):
- `bbfx://install/author.plugin-name` — download and install
- `bbfx://enable/author.plugin-name` — enable an installed plugin
- `bbfx://disable/author.plugin-name` — disable a plugin
- `bbfx://run/author.plugin-name` — enable and run a plugin

If BBFx Studio is already running, deep links are forwarded via IPC.

### 18.7 Plugin Errors (Ctrl+Shift+E)

Shows a ring-buffer log of plugin errors:
- Sandbox violations (attempted access to restricted APIs)
- Load failures (missing resources, syntax errors)
- Runtime errors (Lua exceptions in plugin code)

Each error entry has actions: "Show Plugin", "Retry Load", "Disable Plugin".

---

## 19. Gamepad (v3.5)

### 19.1 Gamepad Panel

Real-time visualization of the connected gamepad. Accessible from **View** or **Plugins** menu.

**Display elements:**
- **Sticks** — 2D pads showing left/right stick position
- **Triggers** — vertical bars for L2/R2
- **Buttons** — lit indicators for all face buttons, bumpers, D-pad
- **Gyroscope** — 3D rotating cube showing real-time orientation (PS5/Switch)
- **Touchpad** — 2D pad showing up to 2 finger positions (PS5)
- **LED** — color picker to set the controller LED color (PS5)
- **Battery** — level bar with charging state indicator

**Test buttons:**
- "Rumble" — test low/high frequency haptic feedback
- "Trigger Rumble" — test adaptive trigger haptic (PS5)
- "LED Test" — cycle through LED colors
- "Calibrate" — start gyroscope calibration (hold controller still)

### 19.2 GamepadNode

A DAG node with 33 output ports covering all gamepad data:
- Sticks (4), triggers (2), buttons (16), gyroscope (3), accelerometer (3), touchpad (4), battery (1)

Wire outputs to any parameter: gyro → camera rotation, triggers → effect intensity, touchpad → XY position, etc.

### 19.3 Gamepad Profiles

Three shipped mapping profiles:
- **PS5 VJ Mode** — sticks control camera, triggers control FX intensity, touchpad controls pan/zoom
- **Xbox DJ Mode** — bumpers for beat skip, triggers for crossfader, sticks for parameter sweep
- **SwitchPro Performance** — optimized for live performance with gyro camera control

Custom profiles can be saved/loaded via `bbfx.gamepadMapping.save/load`.

### 19.4 Learn Mode

Press "Learn" in the Gamepad Panel, then move any gamepad control, then click a parameter in the Inspector or a fader in Performance Mode — the binding is created automatically.

---

# Appendix A — Complete node reference

This appendix lists every node type available in the Node Editor (**Right-click → Create Node → \<category\> → \<type\>**), grouped by **category** (menu tabs). For each node: role, input (I) and output (O) ports, typical parameters.

Port names below are those exposed to the user; some may vary with node configuration.

## Core

| Node | Role | Ports / Key parameters |
|---|---|---|
| **RootTimeNode** | *Singleton* — global clock (beat, bpm, current time) | O: `beat`, `bpm`, `time` |

## Logic

| Node | Role | Ports / Key parameters |
|---|---|---|
| **LuaAnimationNode** | Lua-scripted node (logic edited in Inspector) | Dynamic I/O; **source** parameter (Lua code) |
| **AccumulatorNode** | Accumulates a value over time | I: `in`, `reset` — O: `out` — param: `rate` |
| **SubgraphNode** | Wraps a sub-graph (reusable as a single node) | Dynamic I/O driven by the sub-graph |

## FX

Effects applied to scene objects (pixel shaders, vertex shaders, Perlin, etc.).

| Node | Role | Ports / Key parameters |
|---|---|---|
| **PerlinFxNode** | Perlin deformation (animated vertex noise) | I: `intensity`, `speed`, `scale`, `entity` — O: `entity` |
| **ShaderFxNode** | Custom shader (GLSL + uniforms) | I: `entity`, `speed`, `intensity`, `color`, etc. — O: `entity` — param: **frag_file**, **vert_file**, Lua source |
| **TextureBlitterNode** | Blits a texture onto the object (compositing) | I: `texture`, `entity` — O: `entity` |
| **WaveVertexShader** | Parametric sinusoidal deformation | I: `amplitude`, `frequency`, `phase`, `entity` — O: `entity` |
| **ColorShiftNode** | HSV colour shift on the material | I: `entity`, `hue`, `saturation`, `brightness` — O: `entity` |

## Audio

Capture, analysis and beat detection.

| Node | Role | Ports / Key parameters |
|---|---|---|
| **AudioCaptureNode** | Captures a system or hardware audio input | O: `audio` — param: **device_index**, **sample_rate** |
| **AudioAnalyzerNode** | FFT analysis and band extraction | I: `audio` — O: `bass`, `mid`, `high`, `band_0`..`band_N`, `rms`, `centroid` |
| **BeatDetectorNode** | Kick / BPM detection | I: `audio`, `bass` — O: `beat` (bool), `bpm`, `phase` |
| **BandSplitNode** | Multi-band splitting of an audio stream | I: `audio` — O: `low`, `mid`, `high` |

## Video

| Node | Role | Ports / Key parameters |
|---|---|---|
| **TheoraClipNode** | Ogg Theora clip playback | I: `play`, `pause`, `stop`, `seek`, `playback_speed` — O: `texture`, `progress`, `beat` — param: **file**, **loop**, **volume** |

## Signal

Continuous or event-based signal generation and processing.

| Node | Role | Ports / Key parameters |
|---|---|---|
| **LFONode** | Low-frequency oscillator (sine / tri / square / saw) | O: `out` — param: **waveform**, **frequency**, **amplitude**, **offset** |
| **RampNode** | Linear or exponential ramp | I: `trigger`, `reset` — O: `out` — param: **duration**, **curve** |
| **DelayNode** | Signal time delay | I: `in`, `delay` — O: `out` |
| **EnvelopeFollowerNode** | Envelope follower (attack / release) | I: `in` — O: `out` — param: **attack_ms**, **release_ms** |
| **BeatTriggerNode** | Trigger on every beat / subdivision | I: `beat` — O: `trigger` — param: **subdivision** |
| **TriggerNode** | Conditional trigger (threshold) | I: `in`, `threshold` — O: `trigger` |
| **SplitterNode** | Duplicates a signal into N outputs | I: `in` — O: `out_0`, `out_1`, ..., `out_N` |

## Scene

Objects visible in the 3D scene.

| Node | Role | Ports / Key parameters |
|---|---|---|
| **SceneObjectNode** | 3D object (OGRE mesh) | I: `position_x/y/z`, `rotation_x/y/z`, `scale_x/y/z`, `visible` — O: `entity`, `position`, `rotation` — param: **mesh_file**, **parent_node**, **material** |
| **LightNode** | Scene light (point / directional / spot) | I: `intensity`, `color`, `range` — param: **type** |
| **ParticleNode** | OGRE particle system | I: `entity` (optional, to attach to an object), `emission_rate` — param: **system_name** |
| **CameraNode** | DAG-driven camera | I: `position_x/y/z`, `look_at_x/y/z`, `fov` — param: **near**, **far**, **mode** (perspective / ortho) |
| **TextureNode** | Texture container | O: `texture` — param: **texture_name** |
| **MaterialNode** | Dynamically created / edited OGRE material | I: `diffuse`, `specular`, `shininess`, `diffuse_texture` — O: `material` |

## Environment

| Node | Role | Ports / Key parameters |
|---|---|---|
| **SkyboxNode** | Scene skybox | param: **material_name**, **distance** |
| **FogNode** | Exponential / linear fog | I: `density`, `color` — param: **mode**, **start**, **end** |

## PostProcess

| Node | Role | Ports / Key parameters |
|---|---|---|
| **CompositorNode** | Entry in the Compositor Stack (see ch. 14) | I: `enabled` + 0-3 compositor parameters — param: **compositor** (OGRE name), **enabled** |

## Math

| Node | Role | Ports / Key parameters |
|---|---|---|
| **MathNode** | Generic math operation (add/sub/mul/div/pow/sin/cos/...) | I: `a`, `b` — O: `out` — param: **operation** |
| **MixerNode** | Weighted N→1 mixer | I: `in_0`..`in_N`, `weight_0`..`weight_N` — O: `out` |
| **MapperNode** | Remaps `[in_min, in_max]` → `[out_min, out_max]` | I: `in` — O: `out` — param: **in_min**, **in_max**, **out_min**, **out_max**, **clamp** |

## Input

| Node | Role | Ports / Key parameters |
|---|---|---|
| **MidiInputNode** | Receives MIDI messages from a device | O: `cc_value`, `note_on`, `pitch_bend` — param: **device_index**, **channel**, **cc_number** |
| **OscInputNode** | Receives OSC messages on an address | O: `value_0`, `value_1`, `trigger` — param: **address**, **port** |

## Output

| Node | Role | Ports / Key parameters |
|---|---|---|
| **OscOutputNode** | Sends OSC messages | I: `value`, `trigger` — param: **address**, **target_ip**, **port** |
| **MidiOutputNode** | Sends MIDI messages (CC, Note, Clock) | I: `value`, `trigger` — param: **device**, **channel**, **cc_number** |
| **NdiOutputNode** | NDI stream (network) of the rendered output | param: **sender_name** |
| **TextureShareOutputNode** | Generic Spout / Syphon share (compat alias) | param: **slot_id**, **share_name** |
| **SpoutOutputNode** | Spout output (Windows) | param: **slot_id**, **share_name** |
| **ArtnetOutputNode** | DMX output via Art-Net | I: `channel_1`..`channel_N` — param: **target_ip**, **universe** |

## Stage (v3.4)

| Node | Role | Ports / Key parameters |
|---|---|---|
| **WarpNode** | Quad or grid warp, animatable | I: `tl_x`, `tl_y`, ..., `br_x`, `br_y` (warp points) — param: **output_id** |
| **BlendNode** | Edge blending for multi-projector setups | I: `left`, `right`, `top`, `bottom`, `gamma` — param: **output_id** |

## Community (v3.5)

| Node | Role | Ports / Key parameters |
|---|---|---|
| **GamepadNode** | Gamepad input as DAG source (33 outputs) | O: `left_x`, `left_y`, `right_x`, `right_y`, `left_trigger`, `right_trigger`, `button_a`..`button_y`, `gyro_x`/`y`/`z`, `accel_x`/`y`/`z`, `touch1_x`/`y`, `touch2_x`/`y`, `battery` — param: **gamepad_index** |
| **ArtnetInputNode** | Receives Art-Net DMX data | O: `channel_1`..`channel_8` — param: **universe**, **start_channel** |
| *Plugin-contributed nodes* | Custom nodes from installed plugins | Appear in the **Community** category of the node creation menu |

## Port conventions

- **Input ports** (left) accept a single connection. A new link replaces any previous one.
- **Output ports** (right) can feed several inputs simultaneously.
- **`entity` type**: reference to a 3D object (typically an FX chain: `SceneObjectNode → PerlinFxNode → ShaderFxNode → ...`).
- **`texture` / `material` types**: shared OGRE resource.
- **`bool` / `trigger` types**: instant event (e.g. a detected kick).
- Numeric types auto-convert (float ↔ double ↔ int).

---

# Appendix B — All keyboard shortcuts

Exhaustive list of keyboard shortcuts, grouped by context. A live copy is available through **Help → Keyboard Shortcuts**.

## Global (works from anywhere)

| Shortcut | Action |
|---|---|
| **Ctrl+N** | New project |
| **Ctrl+O** | Open project |
| **Ctrl+S** | Save project |
| **Ctrl+E** | Export video (Export Session dialog) |
| **Ctrl+,** | Settings |
| **Ctrl+Z** | Undo |
| **Ctrl+Y** | Redo |
| **Ctrl+D** | Duplicate selection |
| **Alt+F4** | Exit |
| **Space** | Timeline Play / Pause |
| **+ / -** | BPM ±1 (**Ctrl + / -** = ±5) |
| **Escape** | Exit Performance Mode / close popup / cancel keyboard gizmo |
| **F1** | *About* dialog |
| **F11** | Toggle output window fullscreen |

## Panel toggles (F keys)

| Shortcut | Toggles |
|---|---|
| **F2** | Console |
| **F3** | Inspector |
| **F4** | Timeline |
| **F5** | Performance Mode |
| **F6** | Preset Browser |
| **F7** | Node Editor |
| **F8** | Scene Hierarchy |

## Stage (v3.4)

| Shortcut | Action |
|---|---|
| **Ctrl+Shift+O** | Output Manager |
| **Ctrl+Shift+S** | Surface Editor |
| **Ctrl+Shift+N** | Network Sync |
| **Ctrl+Shift+M** | Master View |
| **Ctrl+Shift+P** | **PANIC ALL** — reset warp, blend, network, DMX, Spout |

## Plugins (v3.5)

| Shortcut | Action |
|---|---|
| **Ctrl+Shift+X** | Plugin Manager |
| **Ctrl+Shift+E** | Plugin Errors |
| **Ctrl+Shift+P** | Command Palette (context-dependent: in Node Editor = PANIC, elsewhere = Command Palette) |

## 3D Viewport

**Tools (Design Mode):**

| Shortcut | Tool |
|---|---|
| **A** | Translate |
| **E** | Rotate |
| **R** | Scale |

**Keyboard transform (gizmo keyboard mode):**

| Shortcut | Action |
|---|---|
| **G** | Starts a keyboard translation |
| **R** | Starts a keyboard rotation (when Ctrl is not held) |
| **S** | Starts a keyboard scale (when Ctrl is not held) |
| **X / Y / Z** | Constrains the axis during keyboard transform |
| **Left-click** | Confirms the keyboard transform |
| **Ctrl** (held) | Snap during the transform |
| **Escape** | Cancels the transform |

**Camera:**

| Shortcut | Action |
|---|---|
| **F** | Focus on the selected object |
| **Home** | Reset camera |
| **Numpad 1** / **Ctrl+Numpad 1** | Front / back view |
| **Numpad 3** / **Ctrl+Numpad 3** | Right / left view |
| **Numpad 7** / **Ctrl+Numpad 7** | Top / bottom view |

**On selected object:**

| Shortcut | Action |
|---|---|
| **Ctrl+D** | Duplicate |
| **Delete** | Remove |
| **H** | Toggle visibility |

## Node Editor

| Shortcut | Action |
|---|---|
| **Delete** | Removes selected nodes |
| **Ctrl+D** | Duplicate |
| **Ctrl+C** / **Ctrl+V** | Copy / paste |
| **Ctrl+G** | Group the selection (≥ 2 nodes) |
| **Ctrl+L** | Smart wire between 2 selected nodes |
| **Ctrl+Space** | Opens the **Quick Add** popup |
| **Double-click in empty space** | Opens **Quick Add** |
| **Ctrl+1 … Ctrl+9** | Saves current view to bookmark 1-9 |
| **1 … 9** | Restores view bookmark |
| **Right-click in empty space** | **Create Node** menu |
| **Right-click on node / link / group** | Corresponding context menu |

Inside **Quick Add**: up/down arrows to navigate, **Enter** to confirm, **Escape** to close.

## Timeline

| Shortcut | Action |
|---|---|
| **Space** | Play / Pause |
| **+** / **-** | BPM ±1 (Ctrl: ±5) |
| **Double-click in lane** | Creates a keyframe |
| **Shift+click on keyframe** | Adds/removes from multi-selection |
| **Drag in lane** | Selection rectangle |

## Performance Mode (F5)

| Shortcut | Action |
|---|---|
| **Tab** | Cycles trigger pages |
| **1 … 9** | Fires triggers 1 to 9 of the current page |
| **Q W E R T** | Fires triggers 10 to 14 |
| **M** (over hovered fader) | MIDI Learn for this fader |
| **Escape** / **F5** | Exit Performance Mode |

## MIDI Learn (global)

| Shortcut | Action |
|---|---|
| **Right-click on a fader** in Performance Mode | Quick Assign / Clear |
| **Right-click on a trigger** in Performance Mode | Context menu (incl. *MIDI Learn*) |
| **Right-click on a port** in the Node Editor | Menu with *MIDI Learn Port ▶* |

## Console

| Shortcut | Action |
|---|---|
| **Tab** | Autocompletion of node names |
| **Enter** | Runs the command |
| **Escape** | Clears the current line |

## Scene Hierarchy

| Shortcut | Action |
|---|---|
| **Click** on item | Selects the node (synced with Node Editor / Viewport / Inspector) |
| **Drag-and-drop** onto a SceneObjectNode | Reparents |
| **Drag-and-drop** in empty area | Unparents |
| **Right-click on item** | Menu: Focus / Hide / Show / Lock / Unlock / Delete |

## File drag-and-drop on the main window

| Extension | Effect |
|---|---|
| **.bbfx-project** | Loads the project |
| **.lua** | Runs the script in the Lua interpreter |

---

# Appendix C — Preset catalog

The **41 presets** shipped with BBFx live in `lua/presets/*.lua`. You can instantiate them:

- **From the Preset Browser** (F6): double-click or drag-and-drop to the Node Editor
- **From the console**: `dbg.preset("<name>")`
- **From a Lua script**: `dbg.preset("<name>")`
- **From the Preset Wheel** or the **Quick Access slots** (chapter 9)

Format: v2 with `ParamSpec`. Each preset instantiates a sub-topology (nodes + links + positions + parameters).

## Geometry (8)

Geometric deformations applied to meshes.

| Name | Description |
|---|---|
| **perlin_pulse** | Geosphere with Perlin displacement pulsing on the beat |
| **perlin_breath** | Slow organic breathing Perlin deformation |
| **wave_morph** | Sinusoidal wave deformation on geosphere |
| **vertex_noise** | High frequency vertex noise |
| **elastic_bounce** | Elastic squash and stretch on kick |
| **mesh_morph_cycle** | Cycle between deformations |
| **fractal_growth** | Fractal growth pattern |
| **geosphere_explode** | Centrifugal explosion on beat |

## Color (7)

Chromatic transformations.

| Name | Description |
|---|---|
| **color_shift** | Continuous HSV hue rotation |
| **rainbow_cycle** | Full spectrum sweep |
| **gradient_pulse** | Animated gradient along an axis |
| **monochrome_fade** | Progressive desaturation to B&W |
| **material_cycle** | Cycle materials on beat |
| **texture_sweep** | Progressive texture transition |
| **flash_strobe** | White flash on each beat onset — classic VJ effect |

## Particle (8)

Particle systems.

| Name | Description |
|---|---|
| **fireflies** | Luminous firefly swarm |
| **rain_drops** | Gentle rain |
| **snowfall** | Gentle snowfall with wind |
| **smoke_rise** | Rising atmospheric smoke |
| **spark_burst** | Spark explosion on beat |
| **star_field** | Star field traversal |
| **jet_exhaust** | Jet engine flame |
| **aureola** | Luminous halo |

## Camera (5)

Scripted camera moves.

| Name | Description |
|---|---|
| **orbit_slow** | Slow contemplative orbital camera around the subject |
| **fly_through** | Camera flying through the scene |
| **dolly_zoom** | Hitchcock / Vertigo dolly zoom |
| **shake_beat** | Camera shake on audio onset |
| **auto_track** | Auto-follows the active object |

## PostProcess (8)

Post-processing effects (compositor chain).

| Name | Description |
|---|---|
| **bloom_dream** | Overexposed bloom for a dreamy ethereal look |
| **glitch_fx** | Digital corruption: RGB offset + scanlines + noise blocks |
| **heat_distort** | Thermal distortion |
| **mirror_kaleidoscope** | Radial symmetry |
| **motion_trail** | Strong motion blur trails |
| **bw_high_contrast** | High contrast B&W |
| **old_film** | Vintage film grain look |
| **depth_of_field** | Cinematic DOF blur |

## Composition (5)

Full ready-made scenes.

| Name | Description |
|---|---|
| **audio_reactive_sphere** | *Iconic BonneBalle*: bass drives displacement, mids shift colour, highs boost bloom |
| **tunnel_infinite** | Infinite tunnel with pulsation |
| **particle_symphony** | 4 particle systems driven by frequency bands |
| **texture_vjing** | Texture cycling on beat |
| **starwars_tribute** | Tribute to the 2006 StarWars demo |

## Creating your own preset

From the Node Editor:
1. Select the nodes to save (Ctrl+click for multi-selection)
2. Right-click → **Save as Preset**
3. Preset name → **Save**

The file is written to `lua/presets/<name>.lua` and appears in the Preset Browser after refresh.

To edit manually, open the Lua file in an external editor. Format:

```lua
return {
    name = "my_preset", version = 2, category = "Custom",
    description = "My description",
    nodes = { ... },
    links = { ... },
}
```

---

# Appendix D — Template catalog

The **14 templates** shipped with BBFx are complete starter projects stored in `lua/templates/*.lua`. Each one pre-configures a scene, a BPM, triggers and faders suited to a musical style or a use case.

They are accessible from the **Splash screen** at first launch, or through the console:

```lua
dbg.preset("<template_name>")  -- or manual load from a script
```

## General-purpose templates

| File | Display name | BPM | Description |
|---|---|---|---|
| `empty.lua` | **Empty Project** | 120 | Start from scratch |
| `bonneballe_basic.lua` | **BonneBalle Basic** | 120 | Geosphere + Perlin + orbit camera (historical scene) |
| `full_performance.lua` | **Full Performance** | 128 | Complete VJ set pre-configured with triggers, faders, Preset Wheel |

## By musical style

| File | Name | BPM | Description |
|---|---|---|---|
| `ambient.lua` | **Ambient** | 70 | Slow organic visuals for ambient music |
| `hiphop.lua` | **Hip-Hop** | 90 | Cool flowing visuals for hip-hop |
| `house.lua` | **House** | 124 | Warm pulsing visuals for house music |
| `beat_machine.lua` | **Beat Machine** | 128 | 4 presets on chord triggers |
| `techno.lua` | **Techno** | 135 | Hard geometric visuals for techno |
| `dubstep.lua` | **Dubstep** | 140 | Heavy bass drop visuals for dubstep |
| `dnb.lua` | **Drum & Bass** | 172 | Fast nervous visuals for D&B |

## By use case

| File | Name | BPM | Description |
|---|---|---|---|
| `audio_reactive.lua` | **Audio Reactive** | 0 | Microphone input drives visuals (free BPM) |
| `shader_lab.lua` | **Shader Lab** | 120 | Explore GPU shaders |
| `particle_show.lua` | **Particle Show** | 140 | 4 particle systems on triggers |
| `video_mix.lua` | **Video Mix** | 120 | 2 video sources with crossfade |

## Creating a template

A template is a simple Lua file returning a table with:

```lua
return {
    name = "Display name",
    bpm = 120,
    description = "...",
    setup = function()
        -- Code executed on load
        dbg.create("SceneObjectNode", "mesh_1")
        dbg.preset("audio_reactive_sphere")
        -- ...
    end,
}
```

Drop the file in `lua/templates/`, it will be detected on the next launch.

---

# Appendix E — Glossary

Terms used in the interface and this manual.

## DAG concepts

**Node** — A functional unit of the graph (SceneObjectNode, ShaderFxNode, etc.). Has input/output ports and parameters.

**Port** — A node's connection point. **Input** (left, one connection) or **output** (right, many connections).

**Link** — Connection between an output port and an input port. Drawn as a coloured Bézier curve.

**DAG** (*Directed Acyclic Graph*) — Directed, acyclic graph. The BBFx graph forbids cycles (feedback loops).

**Singleton** — Unique node (`RootTimeNode`) that cannot be duplicated or deleted. *File → New* preserves it.

**Preset** — Predefined sub-graph that can be instantiated (e.g. `perlin_pulse`). Stored in `lua/presets/*.lua`. See Appendix C.

**Template** — Complete starter project (scene + triggers + faders) tuned for a style. Stored in `lua/templates/*.lua`. See Appendix D.

**ParamSpec** — Parameter metadata (type, bounds, label, widget). Used by the Inspector to auto-generate widgets.

## Animation and time

**Beat** — Musical time unit. The Timeline indexes keyframes in beats (not seconds).

**BPM** (*Beats Per Minute*) — Project tempo. Adjustable in the Timeline (20-1200), via **+/-** keyboard, or through default settings.

**Keyframe** — Animation point on a lane (beat + value). Connected to its neighbour by an interpolation curve (linear, ease, bezier, etc.).

**Lane** — Automation curve for a specific port (`node.port`). Can be *muted*, *armed* (for recording), *collapsed*.

**Chord** — Time block in the chord track, able to hold a port *snapshot* (instant recall).

**Snapshot** — Capture of every port's value at a given time, recallable at will.

## Live performance

**Trigger** — Trigger button in Performance Mode (4×4 grid, 16 per page). Mappable to keyboard key (1-9, Q-W-E-R-T), MIDI note, or beat-gated macro.

**Fader** — Vertical slider driving a continuous value (assignable to a DAG port or a MIDI CC).

**Crossfader** — Horizontal 0-100 % fader mixing two sources (A/B), with *Auto* for an automatic fade.

**Preset Wheel** — Circular wheel in Performance Mode showing presets marked *Add to Wheel*.

**Macro** — Action list on a trigger, run sequentially *beat-gated* (with `wait:N` between actions).

**PANIC** — Emergency reset of in-flight effects. *PANIC* local (Performance Mode button) ≠ *PANIC ALL* (Ctrl+Shift+P: resets warp, blend, DMX, Spout, network).

## Rendering and outputs (v3.4)

**Output slot** — A projector or secondary display output window. Managed in *Output Manager* (Ctrl+Shift+O).

**Warp** — Geometric deformation of the output (4-corner quad or N×M grid) to correct projection on non-flat or oblique surfaces.

**Blend** (*edge blending*) — Progressive edge attenuation to blend two adjacent projectors seamlessly.

**Zone** — Sub-rectangle of the global render, assignable to an output slot (multi-zone mapping). Edited in *Surface Editor* (Ctrl+Shift+S).

**Gizmo** — 3D manipulation handle in the Viewport (translation arrows, rotation rings, scale cubes).

**Compositor** — Post-processing shader chain applied to the whole viewport (bloom, motion blur, DOF, etc.). Stacked in the *Compositor Stack*.

**Render target** (RT) — OGRE texture used as render destination, sized to the main window (*cover* mode for display).

## External

**MIDI Learn** — Listening mode for creating a MIDI binding. The next received message (CC or Note) is bound to the target.

**Binding** — Persisted MIDI assignment (type, CC/Note number, channel, DAG target or fader/trigger, min/max).

**OSC** (*Open Sound Control*) — Network control protocol (addresses like `/my/path/42`).

**Art-Net** — DMX-over-IP protocol (stage lighting).

**Spout** / **NDI** — Texture sharing across applications: Spout on Windows (via DirectX/GL), NDI over network (video encoding).

## Interface (ImGui)

**Dockspace** — Dock grid accepting panels. Drag-and-drop to rearrange.

**Modal popup** — Blocking dialog (Splash, About, Settings, Recover Autosave, Save as Preset, Edit Comment, etc.).

**Selectable** — Clickable list item (presets, nodes in hierarchy, assets).

**DragDropPayload** — Dragged content: texture name (`TEXTURE_NAME`), material name (`MATERIAL_NAME`), etc.

**Tooltip** — Hover info. Some include the matching keyboard shortcut.

---

# Appendix F — Troubleshooting / FAQ

Common issues and resolution hints observable from the interface (no recompilation needed).

## Start-up

### The Studio does not launch (a dialog closes immediately)

Check in the `build/windows-debug/Debug/` folder:
- **`lua/` present** — if missing, rerun a build (`cmake --build ... --target bbfx-studio`). The `lua/` folder is copied from `bbfx-revival/lua/` during build.
- **`resources.cfg` present** and `resources/` folder — same, copied by CMake.
- **Stale `.bbfx_lock` file**: if a hard crash left this lock behind, remove it manually.

### The "Recover Autosave" dialog appears at every launch

The previous session did not end cleanly. Click **Recover Autosave** if you want to resume, or **Ignore** to start fresh. If it keeps appearing, check that you exit via **File → Exit** or **Alt+F4** (not a process kill).

## Viewport and rendering

### The Viewport shows *"(OGRE RenderTexture not ready)"*

The render target is not yet initialised. Typically happens 1-2 frames after launch or a resize. Should disappear immediately. If persistent:
- Resize the main window
- Restart the Studio
- Check the console for OGRE error messages

### The render is black

- Check the **camera** position (menu **View → Use Editor Camera** if lost)
- Press **Home** to reset the camera
- If an active `CameraNode` is in use, check its `near` / `far` / `fov` in the Inspector
- If you just exited Performance Mode and the viewport is black: move the window to force a resize

### Objects gradually disappear / 3D render degrades

Typically a GL state conflict between outputs and the main viewport. Try:
- Close every output window (**Stage → Output Manager → Remove All**)
- Restart the Studio
- This scenario is fixed since I-FIX-3; report it if still reproducible.

## Audio

### Status bar shows *"Audio: Off"*

- No `AudioAnalyzerNode` in the graph. Create one and link an `AudioCaptureNode` to it.
- Check `AudioCaptureNode` in the Inspector: is **device_index** pointing to your interface? In the console: `dbg.list()` then `dbg.inspect("AudioCaptureNode_1")`.
- Verify the system grants access to the device (mic permissions, ASIO/WASAPI driver loaded).

### The beat does not flash in Performance Mode

- Check a `BeatDetectorNode` is linked to the `AudioAnalyzerNode`
- Adjust detection threshold in the Inspector (`threshold`, `sensitivity`)
- The default BPM (**Settings → Default BPM**) is only used when no `BeatDetector` is running — it does not fire a "virtual" beat

## MIDI

### Status bar shows *"MIDI: Off"*

- No MIDI device detected. Plug the device in before launching the Studio.
- In the console: `dbg.midi_devices()` lists devices; `dbg.midi_open(0)` opens the first one.

### I move a MIDI knob, nothing happens

- Check in **Connect → MIDI Activity** that messages arrive (they should appear in the log).
- If yes but no effect: no active binding. Open **Connect → MIDI Mapping**, inspect the bindings table, or run **MIDI Learn** on the desired target.
- Check the channel: if the CC arrives on a filtered channel, no effect.
- Bindings have **Min/Max** — if both are 0, the value does not move.

### MIDI bindings are lost at the next launch

- Save via **Connect → Save Mapping As...** to a preset file
- Reload via **Connect → Load Mapping Preset ▶**
- Bindings are also saved inside the `.bbfx-project` when it is explicitly saved

## Node Editor

### *"Smart wire: no compatible ports found"* in the console

Ctrl+L tried to auto-link two selected nodes, but no compatible port pair was found. Create the link manually (drag from one pin to the other).

### *"Pasted 0 nodes"* after Ctrl+V

The internal clipboard is empty (no previous Ctrl+C) or the last copied selection was empty.

### The selection rectangle does not appear

The rectangle is triggered by click-drag in an **empty** area (no node or link under the cursor). Dragging over a node moves it instead of selecting.

## Outputs (v3.4)

### No render on the open output window

- Check that the output is **active** in **Output Manager** (checkbox)
- From the console: `dbg.output_list()` to list slots
- Try a warp reset: `dbg.output_warp_reset(0)`
- Press **F11** to toggle fullscreen / windowed

### Output image upside down or distorted

- *Fixed since I-FIX-4* for vertical flip. If still present, **Ctrl+Shift+P** (PANIC ALL) to reset all warps.
- In **Output Manager → Warp tab**, click **Reset** on the relevant output.

### Crash when adding a 2nd or 3rd output

- Check GPU compatibility (requires OpenGL 3.3 core + shared FBO)
- On AMD: some drivers have issues with cross-DC `wglMakeCurrent` — make sure drivers are up to date.

## Performance and FPS

### Very low FPS (< 30)

- **Settings → Viewport scale**: lower to 0.5 or 0.25 (cheaper internal render)
- High node count: temporarily bypass non-essential nodes via **Preset Browser → Effect Rack**
- Heavy shaders: in `ShaderFxNode` Inspector, reduce `intensity` or `iterations`
- Long compositor chain: empty the Compositor Stack to test

### The Studio freezes periodically for a few seconds

- Likely autosave in progress. Increase interval in **Settings → Auto-save interval** (up to 600 s).

## Projects and files

### The Save button writes to `project.bbfx-project` instead of my path

This is the behaviour when no current path is set. Use **File → Save As...** to set a path; **Ctrl+S** will then save to it.

### My saved presets do not appear in the Preset Browser

- Scan happens on start-up in `lua/presets/` only
- Check the file is in `build/windows-debug/Debug/lua/presets/` (not `bbfx-revival/lua/presets/`). **Rebuild** if you edited the source to sync.
- Name must **not** start with `_` (prefix reserved for test presets)

### Video export produces frames but no .mp4 file

Expected: the export writes a **PNG sequence**. Use `ffmpeg` to assemble, e.g.:
```
ffmpeg -framerate 30 -i frame_%06d.png -c:v libx264 -pix_fmt yuv420p out.mp4
```

## Misc

### A panel disappeared, I cannot find it

Every panel can be re-opened from **View**, **Connect** or **Stage**. Use the F2-F8 shortcuts for the main ones.

### The theme / some colours look wrong

The theme is not user-configurable. If some elements are miscoloured, it is likely a bug — a corrupt `imgui.ini` may be the cause (deleting `imgui.ini` next to the executable restores the default layout).

### The manual mentions a feature I cannot find

- Check you are in version **3.5 "Community"** (Splash at launch or **Help → About**)
- Some features are only available in specific modes (e.g. **Performance Mode** → triggers / faders)
- Report through the project's channels

## Plugins (v3.5)

### A plugin fails to load with "sandbox violation"

The plugin attempted to use a restricted API (io, debug, os.execute, require("ffi"), etc.). Check the **Plugin Errors** panel (Ctrl+Shift+E) for details. The plugin is automatically disabled — contact the plugin author.

### Installed plugin nodes do not appear in the menu

- Ensure the plugin state is **Enabled** (green badge in Plugin Manager)
- Plugin nodes appear under the **Community** category in the node creation menu
- Try **Plugins → Plugin Manager → right-click → Reload**

### Community Browser shows "Failed to fetch index"

- Check your internet connection
- The Community Browser fetches from GitHub — verify access to `api.github.com`
- A local cache is used when offline (from `~/Documents/BBFx/.community-cache/`)

---

**Quick diagnostics** (to type in the console, **F2**):

| Command | Information |
|---|---|
| `dbg.fps()` | Current FPS |
| `dbg.list()` | Node list |
| `dbg.links()` | Link list |
| `dbg.inspect("node_name")` | Node details |
| `graph()` | Full graph view |
| `dbg.output_list()` | Active outputs |
| `dbg.sync_role()` | Network Sync state |
| `dbg.midi_devices()` | MIDI devices |
| `dbg.midi_bindings()` | MIDI binding count |
| `dbg.plugin_list()` | Installed plugins |
| `dbg.plugin_info("id")` | Plugin details |
| `dbg.plugin_scan()` | Re-scan plugin directories |
| `dbg.gamepad_list()` | Connected gamepads |
| `dbg.community_search("query")` | Search community index |
| `dbg.test()` | Internal test suite (673 assertions) |
