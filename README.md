# BBFx Revival — v3.4

**Real-time 3D animation and effects engine** — a modern C++20 revival of the 2006 BBFx (BonneBalle Effects) engine.

BBFx provides a Lua-scriptable animation DAG (directed acyclic graph) that drives OGRE 3D rendering in real time. Animation nodes are defined and wired in Lua or via the visual node editor; BBFx propagates values through the graph each frame at render speed.

**v3.0 "BBFx Studio"** adds a full GUI application with Dear ImGui: interactive node editor, inspector, timeline, performance mode (F5), and video export — no code required for artists and VJs.

**v3.1 "BBFx Studio++"** completes and stabilizes the Studio: all stubs wired, BPM-to-DAG sync (beat/beatFrac ports), scene/project separation, Lua source serialization in `.bbfx-project`, CLI arguments (`--default`, `--reset`, `--fullscreen`), native Windows file dialogs, console REPL, full keyboard shortcuts, undo/redo (Command pattern), node duplication, bookmarks, and flow animation on links.

**v3.2 "BBFx Studio Content"** makes all presets functional and the Studio usable as a creative tool: 41 presets across 6 categories, 8 procedural fragment shaders, 13 BBFx compositors, ParamSpec system with auto-generated Inspector widgets, enable/disable nodes with visual [OFF] feedback, preset browser organized by category, and 13 new node types (SceneObject, Light, Particle, Camera, Compositor, Skybox, Fog, Math, Mapper, Mixer, Splitter, Trigger, BeatTrigger).

**v3.2.1 "Interactive Viewport"** adds direct manipulation in the 3D viewport: orbit/pan/zoom camera controller (Alt+LMB/MMB/scroll), ray-query object picking with bidirectional selection sync, translation gizmo with axis constraints and undo, procedural infinite grid, viewport toolbar (Select/Translate modes via Q/W), safe deletion with full OGRE cleanup, mesh-to-FX auto-linking, and "Use Editor Camera" toggle.

**v3.2.2 "Multi-Object Scene"** transforms the Studio from a single-object editor into a multi-object composition engine: Scene Hierarchy panel (F8) with visibility/lock toggles, Blender-style intelligent naming (ogrehead→Ogre with auto-increment), right-click context menus for object creation and FX application, drag-drop mesh/FX from browser to viewport, object duplication (Ctrl+D), parent-child hierarchy with relative transforms, cascade FX (multiple FX on one object), unified entity linking for all node types, and dynamic target resolution.

**v3.2.3 "Timeline Automation"** transforms the decorative timeline into a full automation sequencer: AutomationData structures with multi-mode interpolation (Step, Linear, Smooth, EaseIn, EaseOut, Bezier), AutomationEngine per-frame evaluation and DAG injection, automation lanes UI with virtualized rendering, keyframe editing (create/drag/delete, popup, multi-selection, quantize), cue markers with keyboard navigation, loop region, trigger events (chord_jump, preset, enable/disable), real-time recording from faders and Inspector (overdub/replace modes), bezier tangent editing, copy-paste keyframes, LFO generation (sine/square/triangle/sawtooth), chord snapshots with crossfade transitions, and native multi-target DAG (port multiLink, FX multi-clone from graph, target_entity migration). 58 iterations across 9 lots (A-I).

**v3.2.4 "Asset Pipeline & Visual Application"** replaces text-based asset manipulation with visual direct interaction: unified Asset Browser with 6 asset types (Meshes, Textures, Particles, Compositors, Shaders, Materials), 64x64 texture thumbnails via TextureThumbnailCache, universal drag-drop with payloads, TextureNode and MaterialNode as DAG nodes with entity-link and per-sub-entity save/restore, CompositorStackPanel with drag-reorder/inline params/solo/bypass, visual texture picker with grid/search/live preview, ParticleNode entity-link, configurable triggers (chord/enable/disable/preset/reset/compositor) with pages/color/momentary, fader learn mode with intelligent labels and ParamSpec range, auto-detect drop on viewport via raycast, iterative anti-stacking positioning. 75 iterations across 11 lots (A-K).

**v3.2.5 "Performance Pro & Final Polish"** completes the Studio Perfect branch with professional-grade tools: multi-selection (box select, Shift+click, CompoundCommand delete/copy-paste/align/distribute), Shader Gallery (animated 64x64 thumbnails, double-click apply), Material Editor (sphere preview, color/texture editing), Crossfader A/B (DagSnapshot interpolation, auto-crossfade sync BPM, Bounce/Hold), macro triggers (MacroRunner beat-gated, set_param/wait actions), FX Stack in Inspector (Applied Effects with enable/disable/unlink/reorder), quick-add popup (type-ahead, Ctrl+Space), drag-link auto-create, smart wire (Ctrl+L), node comments/groups/collapsed, interactive minimap, ToastSystem, UndoHistoryPanel, preset wheel, auto-assign faders. Infrastructure: Dear ImGui v1.92.7-docking, imgui_test_engine integrated, 95 automated tests (0 FAIL, 0 SKIP), 25 ImGui UI tests, multi-frame Lua coroutine test runner, 20+ dbg automation commands. 89 iterations across 8 lots (B-H).

**v3.3 "BBFx Connect"** transforms the isolated Studio into a connected live performance hub: MIDI controller integration (rtmidi — MidiDeviceManager with auto-detect, hot-plug, multi-device with proper deviceId via CallbackData, MidiInputNode with 8 CC outputs + 4 note triggers + MIDI clock sync + relative CC encoder mode + aftertouch, MidiOutputNode with note/CC send + LED feedback), MIDI Learn system (MidiLearnManager singleton with conflict detection, learn from faders/triggers/Inspector/NodeEditor, MidiMappingPanel with real-time binding display), OSC control (UdpServer thread-safe, OscInputNode with glob pattern matching + command dispatch /bbfx/set|node|preset|bpm, OscOutputNode rate-limited with delta detection, OscBrowserPanel hierarchical tree), Dual Output window (second SDL3 window for projector, resolution presets 720p/1080p/4K, fullscreen F11, multi-monitor selection, OutputPanel with preview), NDI output skeleton (#ifdef BBFX_HAS_NDI), MappingProfile class (MIDI+OSC bindings, save/load .bbfx-mapping, 3 controller presets), Performance Mode overhaul (trigger activate/deactivate with proper node cleanup, rest snapshot + PANIC restore, ChordSystem connected to DagSnapshot, trigger menus Load Preset/Toggle Compositor/Set Param, auto-descriptive labels, Quick Assign faders, crossfader lerp fix + repositioned in right column), autosave recovery dialog (crash detection via lock file, popup Recover/Ignore/Delete), serialization hardened (macroActions, chord snapshots, MIDI mappings, autosave complete, link deduplication), Menu Connect with Load/Save Mapping Preset, status bar MIDI/Output indicators, Help shortcuts. FX node naming fix (ColorShiftNode/PerlinFxNode/WaveVertexShader/TextureBlitterNode use instance names), FX cleanup fix (PerlinFxNode/WaveVertexShader destroy OGRE clones on delete), ColorShiftNode factory fix (uses existing SceneObjectNode entity). 224 iterations (I-714→I-937), 14 lots (A-N) + fixes, 101+ automated tests.

**v3.4 "BBFx Stage"** transforms the Studio into a professional multi-projector mapping and synchronization platform: OutputManager multi-slot architecture (up to 8 independent outputs, uber-shader GL3.3, Win32 native windows bypassing AMD driver RenderTexture bug, blit-based pipeline), QuadWarp per-output (4-corner perspective with bilinear interpolation, GLSL distortion shader, keyboard precision mode, drag handles), EdgeBlend per-output (soft-edge overlap zones, linear/gamma curves, RGB per-channel control, independent left/right/top/bottom), GridWarp (subdivided mesh distortion with NxM control points, mesh-based GLSL shader), WarpWizard (step-by-step calibration: output selection → warp type → preview → apply, with live feedback), SurfaceMap multi-zone manager (add/remove/rename/select zones, zone-to-output assignment, WarpProfile+BlendProfile+GridWarpProfile per zone), SyncManager (UDP clock protocol, master/slave auto-detect, beat/bar/phase sync, network latency compensation, BPM broadcast, transport lock), NetworkPanel (master/slave status, connected peers, latency display, manual BPM override, Start/Stop sync), Spout output integration (TextureShareSender cross-platform abstraction — Spout2 on Windows, DmaBuf stub on Linux, Null fallback, per-output enable/config), NDI output (full NdiOutputNode implementation with libndi, resolution/fps config), Art-Net DMX output (ArtnetOutputNode with universe/channel addressing, 8 DAG-driven channels), MIDI Clock output (MidiClockNode — 24ppq tick generation, Start/Stop/Continue, tempo-synced to BeatDetector), MasterViewPanel (mosaic of all active outputs with live thumbnails, per-output status overlay, click-to-select, fullscreen preview), SceneSwitcher (ZoneSnapshot capture/apply warp+blend per zone, chord integration for scene presets, crossfade transitions, PANIC restore), and full integration across OutputPanel/MasterView/SceneSwitcher. Architecture: blit-based rendering pipeline (uber-shader GL3.3 fullscreen quad, Win32 native windows via CreateWindowEx/wglCreateContext, contournement bug AMD driver glBlitFramebuffer sur RenderTexture). Dear ImGui v1.92.7-docking. 352 iterations (I-938→I-1280 + I-FIX-1→I-FIX-11), 17 lots (A-Q) + 62 fix iterations, 34+ automated tests.

---

## Features

### Core
- **Animation DAG** — Boost.Graph directed acyclic graph, BFS propagation, pre/post operation queues
- **Lua scripting** — Lua 5.4+ via sol2 (type-safe, no code generation)
- **OGRE 14.5** — GL3Plus default, D3D11 via `--d3d11` (Windows), Vulkan (Linux), full resource pipeline
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

### BBFx Studio Timeline Automation (v3.2.3)
- **Fix pause/resume** — `RootTimeNode::resume()` resets `mLastTime` without touching `mTotalTime`, `seekTo()` for repositioning, dt clamp 0.1s
- **AutomationData** — Keyframe, AutomationLane, CueMarker, TriggerEvent, LoopRegion structures; multi-mode interpolation (Step, Linear, Smooth, EaseIn, EaseOut, Bezier); `evaluate()` with binary search
- **AutomationEngine** — per-frame evaluation of all active lanes, port value injection between `time->update()` and `renderOneFrame()`
- **Automation lanes UI** — virtualized rendering under chord blocks, keyframe diamonds (8x8px), interpolation curves, lane header (mute/collapse/arm), assignation lane-to-port via dropdown
- **Keyframe editing** — double-click create, drag move, Delete remove, popup editor (beat/value/mode), right-click interpolation mode, Ctrl+Q quantize, rubber band multi-selection, grouped operations
- **Cue markers** — M to add at playhead, Ctrl+Left/Right navigation, rename, delete; yellow dashed vertical lines
- **Loop region** — Shift+drag on time bar, L toggle, `seekTo()` wrap at endBeat
- **Trigger events** — programmable actions at precise beats (chord_jump, preset, enable, disable); fire on beat crossing
- **Recording** — arm lanes (R button), real-time capture from faders and Inspector, `thinRedundantKeyframes()` post-record cleanup, overdub/replace modes
- **Bezier tangents** — editable handles (drag circles), `SetTangentCommand` with undo, cubic bezier evaluation via De Casteljau
- **Copy-paste** — Ctrl+C/V keyframes with relative beat offset, `PasteKeyframesCommand`
- **LFO generation** — sine/square/triangle/sawtooth waveforms, configurable frequency/amplitude/offset/cycles, `GenerateLFOCommand`
- **Zoom vertical** — Ctrl+scroll on lanes area, 24px-200px range
- **Chord snapshots** — Store/Recall parameter snapshots on chord blocks, crossfade transitions on playhead entry
- **Automation serialization** — `AutomationData::toJson()/fromJson()`, `ProjectSerializer` section "automation", retrocompatible with v3.2.2 files
- **AutomationCommands** — 17 undoable commands (AddKeyframe, MoveKeyframe, DeleteKeyframe, SetInterpolationMode, SetTangent, AddLane, DeleteLane, AssignLanePort, AddCueMarker, DeleteCueMarker, RenameCueMarker, AddTriggerEvent, DeleteTriggerEvent, SetLoopRegion, QuantizeKeyframes, PasteKeyframes, GenerateLFO)
- **Native multi-target DAG** — `AnimationPort::multiLink` flag, `Animator::getSourceNodes()` helper, FX nodes `resolveTargets()` multi-clone from graph, `target_entity` ParamSpec removed, serialization migration

### BBFx Studio Asset Pipeline (v3.2.4)
- **Asset Browser unified** — 7 sections (Meshes, Textures, Particles, Compositors, Shaders, Materials, Presets), unified search bar, texture thumbnail grid (64x64 via TextureThumbnailCache), preview tooltips, 7 drag-drop payloads (MESH/TEXTURE/PARTICLE/COMPOSITOR/SHADER/MATERIAL/PRESET)
- **Visual pickers** — TEXTURE popup grid with thumbnails and live preview (hover=temporary change, leave=restore), MATERIAL/COMPOSITOR/PARTICLE searchable popups in Inspector
- **TextureNode** — new DAG node for texture application via entity-link, creates material clones (TexNode_ prefix), per-sub-entity save/restore, cascade detection, setEnabled detach/re-attach
- **MaterialNode** — same entity-link pattern for OGRE material application with per-sub-entity save/restore
- **CompositorStackPanel** — dockable panel scanning DAG for CompositorNodes, drag-reorder, inline float params, solo/bypass (Shift+click), drop from browser, syncStackOrder() independent of render
- **Compositor Performance Mode** — `setClearEveryFrame(false)` fix enables OGRE compositor chain on F5 viewport, mCompositorsPending for deferred application after resize, proper cleanup on F5 exit
- **ParticleNode entity port** — multiLink input port, resolveTarget(), onLinkChanged(), detach via context menu
- **Triggers Pro** — TriggerSlot replaces triggerChords[16]: 7 polymorphic actions (chord/enable/disable/compositor/chord_jump/preset/reset), right-click assignment UI with categories, momentary/toggle mode, per-trigger hue color, multiple pages (Tab navigation)
- **Faders Pro** — learn mode (click param in Inspector → auto-assign), intelligent labels (nodeName.portName), numeric value display, min/max range from ParamSpec, persistent in save/load
- **Visual feedback** — node activity indicators (green/gray/orange), beat flash border in Performance Mode
- **Auto-detect drop** — viewport drop uses raycast at release point (no pre-selection needed), node editor drop uses cached canvas transform (mCachedNodeRects + screenToCanvasCached)
- **Anti-stacking** — iterative while-loop positioning (50x80px tolerance), horizontal alignment with target SceneObjectNode, checks both existing nodes and pending positions
- **Renderer selection** — GL3Plus default, `--d3d11` runtime argument, `BBFX_USE_D3D11` CMake option, dynamic plugin loading, only GL3Plus loaded by default (no D3D11/GL legacy overhead)
- **Debugger extensions** — `dbg.create_with_shader()`, `dbg.create_with_param()`, `dbg.set_param()`, `dbg.compositor_status()`, `dbg.trace()`, `dbg.mode()`, PendingOp with preParam injection
- **ShaderFxNode tex0** — auto-binding for post-process shaders using `sampler2D tex0`
- **ShaderFxNode onLinkChanged** — uniform entity-link pattern across all 5 FX node types
- **Resource filter** — `isBBFxShader()` filters ~25 OGRE internal shader prefixes from browser
- **API mutable** — `getParams()` non-const on ParamSpec, `getInputs()`/`getOutputs()` non-const on AnimationNode (0 const_cast)
- **Bridge save/load** — StudioApp copies faders (8 + minVal/maxVal), triggerPages (N x 16 slots), compositorStack between panels and ProjectState
- **Camera default** — orbit distance 150 units, pitch 15 degrees for comfortable viewing

### BBFx Connect (v3.3)
- **MidiDeviceManager** — rtmidi wrapper, auto-detect + auto-open all devices at startup, hot-plug detection, proper deviceId via CallbackData, thread-safe (callback→mutex queue→poll)
- **MidiInputNode** — DAG node with 8 configurable CC outputs, 4 note triggers, pitch bend, aftertouch, MIDI clock sync (24ppq→BPM), transport start/stop/continue, relative CC encoder mode
- **MidiOutputNode** — DAG node sending note-on/off, CC, and LED feedback (led_note/led_velocity ports for Launchpad/APC)
- **MidiLearnManager** — singleton, learn mode (fader/trigger/port), conflict detection (auto-replace), toJson/fromJson serialization
- **MidiMappingPanel** — real-time binding table from MidiLearnManager, Learn buttons, edit min/max, delete
- **MidiActivityPanel** — live MIDI monitor, color-coded by type (green=notes, blue=CC, yellow=PC), channel filter
- **OscInputNode** — UDP listener with glob pattern matching, 8 value outputs, trigger bang, command dispatch (/bbfx/set, /bbfx/node/enable|disable, /bbfx/preset/load, /bbfx/bpm)
- **OscOutputNode** — UDP sender, rate-limited (max_rate configurable), delta detection
- **OscBrowserPanel** — hierarchical tree of auto-discovered OSC addresses, click-to-copy
- **OutputPanel** — second SDL3 window for projector, resolution presets (720p/1080p/4K), fullscreen (F11), multi-monitor selection via SDL_GetDisplays, preview 16:9
- **NdiOutputNode** — skeleton with #ifdef BBFX_HAS_NDI, ParamSpec (source_name, width, height, fps), graceful no-op without SDK
- **MappingProfile** — MIDI+OSC binding profiles, save/load .bbfx-mapping JSON, 3 controller presets (APC Mini, nanoKONTROL2, Launchpad)
- **Menu Connect** — MIDI Activity, MIDI Mapping, OSC Browser, Load/Save Mapping Preset, Clear All
- **Status bar** — MIDI (devices/bindings) and Output (On/Off) indicators
- **Performance Mode overhaul** — trigger activate/deactivate with proper node cleanup (synchronous deletion), rest snapshot + PANIC restore (not all-to-zero), ChordSystem→DagSnapshot (capture/apply/remove), trigger menus (Load Preset, Toggle Compositor, Set Param), auto-descriptive labels + tooltips, Quick Assign faders, crossfader lerp fix + repositioned in right column
- **Autosave recovery** — crash detection via lock file, popup dialog (Recover/Ignore/Delete Autosave)
- **Serialization hardened** — macroActions, chord snapshots, MIDI mappings, autosave complete, link deduplication
- **FX node fixes** — instance naming (not type-hardcoded), proper OGRE cleanup (destroy clones, not just hide), ColorShiftNode factory uses existing SceneObjectNode entity

### BBFx Stage (v3.4)
- **OutputManager** — multi-slot architecture (up to 8 independent outputs), uber-shader GL3.3 fullscreen quad, blit-based pipeline, Win32 native windows (CreateWindowEx/wglCreateContext) bypassing AMD driver RenderTexture bug
- **QuadWarp** — 4-corner perspective distortion per output, bilinear interpolation, GLSL distortion shader, keyboard precision mode (arrow keys), draggable corner handles, reset-to-default
- **EdgeBlend** — soft-edge overlap blending per output, linear/gamma curves, RGB per-channel control, independent left/right/top/bottom zones, real-time preview
- **GridWarp** — subdivided mesh distortion with NxM control points, mesh-based GLSL shader, drag-to-deform, grid density configuration
- **WarpWizard** — step-by-step calibration workflow (output selection → warp type → preview → apply), live feedback, undo/redo integration
- **SurfaceMap** — multi-zone manager (add/remove/rename/select zones), zone-to-output assignment, WarpProfile + BlendProfile + GridWarpProfile per zone, SurfaceEditorPanel
- **SyncManager** — UDP clock protocol (master/slave auto-detect), beat/bar/phase synchronization, network latency compensation, BPM broadcast, transport lock
- **NetworkPanel** — master/slave status, connected peers list, latency display, manual BPM override, Start/Stop sync controls
- **TextureShareSender** — cross-platform texture sharing abstraction (Spout2 on Windows via spout2dx, DmaBuf stub on Linux, Null fallback), per-output enable/config, factory pattern
- **NdiOutputNode** — full NDI output implementation with libndi, resolution/fps configuration, ParamSpec (source_name, width, height, fps)
- **ArtnetOutputNode** — Art-Net DMX output, universe/channel addressing, 8 DAG-driven channels for lighting control
- **MidiClockNode** — MIDI Clock output (24ppq tick generation), Start/Stop/Continue, tempo-synced to BeatDetector
- **MasterViewPanel** — mosaic of all active outputs with live thumbnails, per-output status overlay (resolution, warp, blend), click-to-select, fullscreen preview
- **SceneSwitcher** — ZoneSnapshot capture/apply (warp + blend per zone), chord integration for scene presets, crossfade transitions between scenes, PANIC restore
- **Blit-based architecture** — uber-shader GL3.3 fullscreen quad replaces glBlitFramebuffer, Win32 native windows via CreateWindowEx/wglCreateContext, contournement bug AMD driver on RenderTexture

---

## Architecture

```
Lua scripts (lua/*.lua, lua/demos/*.lua)
    |
sol2 bindings (src/bindings/bbfx_bindings.cpp)
    |
C++ core
  ├── Engine          -- SDL3 window + OGRE render loop
  ├── Animator        -- Boost.Graph DAG, BFS propagation, pre/post op queues, multiLink ports
  ├── PrimitiveNodes  -- RootTimeNode (resume/seekTo), LuaAnimationNode, AnimationStateNode
  ├── Automation      -- AutomationData, AutomationEngine (per-frame DAG injection)
  ├── FX              -- Perlin (CPU+GPU), TextureBlitter, WaveVertex, Shader, ColorShift
  ├── Input           -- KeyboardManager, MouseManager, JoystickManager (SDL3)
  ├── Audio           -- AudioCapture, AudioAnalyzer, BeatDetector
  ├── Video           -- OggReader, TheoraReader, TheoraBlitter, TheoraClip, Crossfader
  ├── MIDI            -- MidiDeviceManager, MidiLearnManager, MappingProfile, MidiMessage (v3.3)
  ├── OSC             -- OscMessage (v3.3)
  ├── Network         -- TcpServer (remote REPL), UdpServer (OSC, v3.3), SyncManager (v3.4)
  ├── Output          -- OutputManager, WarpProfile, BlendProfile, GridWarpProfile, WarpWizard (v3.4)
  ├── Surface         -- SurfaceMap, ZoneSnapshot (v3.4)
  ├── Share           -- TextureShareSender (Spout/DmaBuf/Null) (v3.4)
  ├── Record          -- InputRecorder, InputPlayer, VideoExporter
  └── Studio          -- StudioApp, StudioEngine, NodeTypeRegistry, Debugger
       ├── Nodes      -- SceneObject, Light, Particle, Camera, Compositor, Skybox, Fog, Math, Texture, Material, MidiInput, MidiOutput, OscInput, OscOutput, NdiOutput, ArtnetOutput, MidiClock, ...
       ├── Panels     -- Viewport, NodeEditor, Inspector, Timeline, Presets, Console, Perf, CompositorStack, MidiActivity, MidiMapping, OscBrowser, Output, MasterView, Network, SurfaceEditor (v3.4)
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
| `src/core/PrimitiveNodes` | RootTimeNode (clock + resume/seekTo), LuaAnimationNode, AnimationStateNode, AccumulatorNode |
| `src/core/AutomationData` | Keyframe, AutomationLane, CueMarker, TriggerEvent, LoopRegion; multi-mode interpolation evaluate() |
| `src/core/AutomationEngine` | Per-frame automation evaluation, DAG port injection |
| `src/fx/PerlinVertexShader` | 3D Perlin noise CPU vertex deformation |
| `src/fx/ShaderFxNode` | GLSL shader loader with auto-parsed float uniforms as ports |
| `src/fx/TextureBlitter` | Manual RGBA texture, pixel-level write |
| `src/fx/WaveVertexShader` | Sinusoidal vertex displacement |
| `src/input/` | KeyboardManager, MouseManager, JoystickManager, InputManager |
| `src/audio/` | AudioCapture, AudioAnalyzer, BeatDetector |
| `src/video/` | TheoraClip, ReversableClip, TheoraBlitter, TextureCrossfader |
| `src/midi/` | MidiDeviceManager, MidiLearnManager, MappingProfile, MidiMessage |
| `src/osc/` | OscMessage parser |
| `src/network/TcpServer` | TCP REPL server, WinSock2/POSIX |
| `src/network/UdpServer` | UDP listener for OSC, thread-safe queue |
| `src/network/SyncManager` | UDP clock sync master/slave, beat/bar/phase, latency compensation |
| `src/output/OutputManager` | Multi-slot output architecture (up to 8), uber-shader GL3.3, blit pipeline |
| `src/output/WarpProfile` | QuadWarp 4-corner perspective + GridWarp NxM mesh distortion |
| `src/output/BlendProfile` | EdgeBlend soft-edge overlap, linear/gamma, RGB per-channel |
| `src/output/WarpWizard` | Step-by-step calibration workflow with live preview |
| `src/surface/SurfaceMap` | Multi-zone manager, zone-to-output assignment, WarpProfile+BlendProfile per zone |
| `src/surface/ZoneSnapshot` | Capture/apply warp+blend per zone, scene presets |
| `src/share/TextureShareSender` | Cross-platform texture sharing (Spout2/DmaBuf/Null), factory pattern |
| `src/record/` | InputRecorder, InputPlayer, VideoExporter |
| `src/studio/` | StudioApp, StudioEngine, NodeTypeRegistry, Debugger, SettingsManager, ResourceEnumerator |
| `src/studio/nodes/` | SceneObjectNode, LightNode, ParticleNode, CameraNode, CompositorNode, SkyboxNode, FogNode, MathNode, MapperNode, MixerNode, SplitterNode, TriggerNode, BeatTriggerNode, TextureNode, MaterialNode, MidiInputNode, MidiOutputNode, OscInputNode, OscOutputNode, NdiOutputNode, ArtnetOutputNode, MidiClockNode |
| `src/studio/panels/` | ViewportPanel, NodeEditorPanel, InspectorPanel, TimelinePanel, PresetBrowserPanel, ConsolePanel, PerformanceModePanel, SetEditorPanel, SceneHierarchyPanel, CompositorStackPanel, MidiActivityPanel, MidiMappingPanel, OscBrowserPanel, OutputPanel, MasterViewPanel, NetworkPanel, SurfaceEditorPanel |
| `src/studio/TextureThumbnailCache` | Lazy-load OGRE textures as ImGui GL texture IDs (64x64), single-thread only |
| `src/studio/viewport/` | ViewportCameraController, ViewportPicker, ViewportGizmo, ViewportGrid, ViewportToolbar |
| `src/studio/commands/` | CommandManager, NodeCommands, LinkCommands, EditCommands, ChordCommands, TransformCommands, SceneCommands, AutomationCommands |
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
| **MIDI Live** | `./bbfx-studio lua/demos/demo_midi_live.lua` | MIDI controller live performance |
| **OSC Control** | `./bbfx-studio lua/demos/demo_osc_control.lua` | OSC remote control from tablet/phone |
| **Dual Output** | `./bbfx-studio lua/demos/demo_dual_output.lua` | Second window for projector output |
| **Multi Output** | `./bbfx-studio lua/demos/demo_multi_output.lua` | Multi-projector setup with warp/blend calibration |
| **Network Sync** | `./bbfx-studio lua/demos/demo_network_sync.lua` | Master/slave beat synchronization over network |
| **Projection Mapping** | `./bbfx-studio lua/demos/demo_projection_mapping.lua` | Surface mapping with QuadWarp, EdgeBlend, GridWarp |

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
| Dear ImGui | 1.92.7 | Studio GUI (panels, inspector, menus) — docking branch |
| imgui-node-editor | — | Visual node graph editor |
| libtheora | 1.2.0 | Theora video decoding |
| libogg | 1.3.6 | Ogg container parsing |
| rtmidi | latest | MIDI device I/O |
| Spout2 | latest | Texture sharing on Windows (via spout2dx) |
| libndi | latest | NDI video output |

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

- [`docs/architecture.md`](docs/architecture.md) — Full architecture reference (v2.0–v3.4), all modules, Lua API, design decisions
- [`lua/demos/USAGE.md`](lua/demos/USAGE.md) — Demo and Studio usage reference

---

## History

BBFx was written in 2006 by Sébastien JULLIEN and Thomas LEFORT as a real-time 3D animation engine for demoscene productions: OGRE 1.2, OIS, SWIG, Lua 5.1, SCons on Linux. The v2.x revival (2025–2026) rewrites it from scratch in modern C++20 — same animation DAG architecture, entirely updated stack — and extends it with audio reactivity, GPU shaders, Theora video, live scripting, and a production recording/export pipeline. v3.0 introduces the visual Studio (ImGui + OGRE 14), v3.1 stabilizes it with undo/redo and project serialization, v3.2 delivers 41 presets and 13 new node types, v3.2.1 adds interactive viewport manipulation (picking, gizmos, grid), v3.2.2 completes the multi-object workflow with scene hierarchy, drag-drop, and cascade FX, v3.2.3 transforms the timeline into a full automation sequencer with keyframes, cue markers, loop region, real-time recording, bezier curves, LFO generation, chord snapshots, and native multi-target DAG, v3.2.4 delivers the asset pipeline with unified browser, visual pickers, TextureNode/MaterialNode DAG entity-link, compositor stack with Performance Mode rendering, triggers/faders pro with 7 action types and learn mode, auto-detect drop, and runtime renderer selection, v3.2.5 completes the Studio Perfect branch with multi-selection, Shader Gallery, Material Editor, Crossfader A/B, macro triggers, FX Stack, quick-add, smart wire, collapsed nodes, groups, minimap, 95 tests, v3.3 "Connect" transforms the Studio into a live performance hub with MIDI controller integration (rtmidi, MidiLearnManager, conflict detection, LED feedback), OSC control (UdpServer, command dispatch), dual output window (projector, F11 fullscreen, multi-monitor), NDI skeleton, MappingProfile save/load, Performance Mode overhaul (trigger activate/deactivate, rest snapshot PANIC, ChordSystem→DagSnapshot), autosave recovery dialog, and 224 iterations, and v3.4 "Stage" transforms the Studio into a professional multi-projector mapping platform with OutputManager multi-slot (up to 8 outputs, uber-shader GL3.3, blit-based pipeline, Win32 native windows), QuadWarp/EdgeBlend/GridWarp per-output calibration, WarpWizard step-by-step, SurfaceMap multi-zone manager, SyncManager UDP clock (master/slave beat/bar sync), TextureShareSender cross-platform (Spout2/DmaBuf/Null), full NDI/Art-Net/MIDI Clock output nodes, MasterViewPanel mosaic, SceneSwitcher with ZoneSnapshot, and 352 iterations across 17 lots.

---

## License

See [LICENSE](LICENSE).
