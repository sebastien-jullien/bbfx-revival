# BBFx Architecture

## Section 17 — Animator Avancé (v2.5)

### 17.1 — Nœuds temporels

Fichier : `lua/temporal_nodes.lua`

Quatre nœuds temporels implémentés comme `LuaAnimationNode` wrappers :

| Nœud | Ports d'entrée | Port de sortie | Comportement |
|------|---------------|----------------|-------------|
| **LFONode** | dt, frequency, amplitude, offset, waveform, phase | out | Oscillateur — sin/tri/square/saw |
| **RampNode** | dt, target, rate | out | Convergence vers target à vitesse rate/s, sans dépassement |
| **DelayNode** | dt, in, delay_time | out | Buffer circulaire (600 entrées max, 10s à 60fps) |
| **EnvelopeFollowerNode** | dt, in, attack, release | out | Enveloppe attack/release, entrée 0..1 trigger |

**Lifecycle :**
```lua
local lfo = LFONode:new({frequency=1.0, amplitude=1.0, waveform=LFONode.SIN})
-- lfo._node = nœud bbfx brut
-- wrappers: lfo:setFrequency(f), etc.
-- le nœud est auto-enregistré dans bbfx.Animator.instance()
```

**Constantes de forme d'onde :** `LFONode.SIN=0`, `LFONode.TRI=1`, `LFONode.SQUARE=2`, `LFONode.SAW=3`

---

### 17.2 — Animation spline Lua

Fichier : `lua/animation.lua`

Port du pattern 2006 `compo.lua` : animations spline Catmull-Rom pure Lua (Ogre::SimpleSpline non disponible dans ogre-lua).

**Lifecycle :**
```lua
local anim = Animation:new()
anim:addFrames(
    {length=1, translate=Ogre.Vector3(0,0,0)},
    {length=1, translate=Ogre.Vector3(0,0,100)},
    {length=1, translate=Ogre.Vector3(0,0,0)}
)
anim:create(Animation.IM_SPLINE, Animation.RIM_LINEAR, true)  -- loop=true
anim:bind(sceneNode)  -- connecte à un Ogre SceneNode
anim:play()           -- démarre la lecture
anim:stop()           -- arrête
anim:setTime(0.5)     -- seek
t = anim:getTime()
```

**Constantes :** `Animation.IM_LINEAR=0`, `Animation.IM_SPLINE=1`, `Animation.RIM_LINEAR=0`

**Implémentation spline :** Catmull-Rom, 4 points de contrôle par segment, tangentes auto-calculées aux bords (duplication du premier/dernier keyframe).

---

### 17.3 — SubgraphNode

Fichier : `lua/subgraph.lua`

Encapsule un sous-graphe d'animation avec interface externe nommée.

```lua
local sg = SubgraphNode:new("MyGraph")
sg:addNode(lfo,  "lfo")
sg:addNode(ramp, "ramp")
sg:link("lfo.out", "ramp.target")         -- câblage interne
sg:exposeInput("frequency", "lfo",  "frequency")   -- port externe
sg:exposeOutput("value",    "ramp", "out")

sg:setInput("frequency", 2.0)
local v = sg:getOutput("value")
```

---

### 17.4 — Système de Presets

Fichier : `lua/subgraph.lua` (table `Preset`), `lua/presets/`

```lua
Preset:define("MyPreset", function(args)
    local sg = SubgraphNode:new("MyPreset")
    -- ... construction ...
    return sg
end)

local instance = Preset:instantiate("MyPreset", {frequency=1.5})
Preset:save(instance, "presets/my_preset.txt")
local params = Preset:load("presets/my_preset.txt")
```

**Preset built-in :** `"PerlinPulse"` — LFONode (sin) → RampNode → ports exposés : frequency, amplitude, rate, value.

---

### 17.5 — Style déclaratif

Fichier : `lua/declarative.lua`

```lua
local handles = build({
    nodes = {
        {name="lfo",    type="LFO",      frequency=0.5, amplitude=30.0},
        {name="ramp",   type="Ramp",     rate=2.0},
        {name="perlin", type="existing", ref=perlinFxNode},
        {name="pp",     type="preset",   preset="PerlinPulse", frequency=1.0},
    },
    links = {
        {from="lfo.out",  to="ramp.target"},
        {from="ramp.out", to="perlin.displacement"},
    }
})
-- handles.nodes.lfo, handles.nodes.ramp, etc.
```

**Types de nœud :** `"LFO"`, `"Ramp"`, `"Delay"`, `"Envelope"`, `"existing"` (ref=node), `"preset"` (preset="Name").

---

### 17.6 — Port compo.lua

Fichier : `lua/compo.lua`

Port de la production 2006 (`prog/workspace/bbfx/bbfx/src/lua/0.1/compo.lua`).

**Adaptations v2.5 :**
- `Ogre.Animation.IM_SPLINE` → `Animation.IM_SPLINE`
- `Dictionary:proxy()` → stub (warning + no-op)
- `bbfx.LuaControllerValue` / `createValueAnimation` → stubbed (note_30/note_31 = nil)
- `Object:fromPsys()` → wrappé dans `safe_psys()` avec pcall (templates may be absent)
- `anim:bind(obj.animNode)` → `safe_bind()` helper (skips nil objects)
- `chord:defState()` → `safe_def()` helper (skips nil notes)

```lua
local result = Compo.starwars()
-- result.sync  : Sync object
-- result.chord : Chord object
```

---

## Section 18 — Shell & Scripting (v2.6)

### 18.1 — REPL Console

Fichier : `lua/console.lua`

**Lifecycle :** `StdinReader` (C++) → `LuaConsoleNode` (DAG) → `ErrorHandler.eval()` → print result

```lua
require "console"
-- Automatic: LuaConsoleNode added to DAG, prompt "bbfx> " displayed
-- Type any Lua expression at the prompt during rendering
-- Result is printed, errors are logged via Logger.error()
```

**StdinReader C++** (`src/input/StdinReader.h/.cpp`) : lit stdin sans bloquer via `_kbhit()`/`_getch()` (Windows). Buffer ligne interne, backspace géré. Binding sol2 : `bbfx.StdinReader()`, `:poll()` → nil ou string.

### 18.2 — Commandes introspection

Fonctions globales Lua, utilisables depuis le REPL et le shell TCP :

| Commande | Description |
|----------|-------------|
| `graph()` | Liste les nœuds du DAG avec leur nom |
| `ports("name")` | Liste les ports d'un nœud (entrée/sortie + valeurs) |
| `set("name", "port", val)` | Modifie la valeur d'un port |
| `reload("script.lua")` | Recharge un script via ErrorHandler.dofile |
| `watch("file.lua")` | Ajoute un fichier au hot reload |
| `unwatch("file.lua")` | Retire un fichier du hot reload |
| `watchlist()` | Affiche les fichiers surveillés |
| `help()` | Affiche l'aide |
| `quit()` | Arrête le moteur |

### 18.3 — Shell TCP distant

**Architecture :**
```
Client (Python/netcat) → TCP (port 33195) → TcpServer C++ (thread) → queue → ShellServer Lua (main thread) → ErrorHandler.eval → reply
```

**TcpServer C++** (`src/network/TcpServer.h/.cpp`) : WinSock2, `std::thread` listener, `std::mutex` + `std::queue` pour la communication thread-safe. Le thread réseau ne touche jamais `lua_State`.

**ShellServer Lua** (`lua/shell/server.lua`) :
```lua
require "shell/server"
local shell = ShellServer:new()  -- default port 33195, max 2 clients
-- Protocole : une ligne TCP = une expression Lua
-- Réponse : "--> result" ou "error: message"
```

**Client :** `python lua/shell/client.py [host] [port]`

### 18.4 — Hot Reload

Fichier : `lua/hotreload.lua`

```lua
require "hotreload"
watch("lua/myeffect.lua")   -- surveille le fichier
-- Modification détectée → [RELOAD] lua/myeffect.lua → ErrorHandler.dofile()
-- Erreur → Logger.error(), moteur continue
unwatch("lua/myeffect.lua") -- arrête la surveillance
```

Vérifie les timestamps toutes les ~1 seconde via `bbfx.fileModTime()` (binding C++ `std::filesystem::last_write_time`).

### 18.5 — Logger & ErrorHandler

**Logger** (`lua/logger.lua`) :
```lua
require "logger"
Logger.init("bbfx.log")  -- active l'écriture fichier (optionnel)
Logger.info("message")    -- [2026-03-26 14:30:00] [INFO] message
Logger.warn("message")    -- [2026-03-26 14:30:00] [WARN] message
Logger.error("message")   -- [2026-03-26 14:30:00] [ERROR] message
```

**ErrorHandler** (`lua/errorhandler.lua`) :
```lua
require "errorhandler"
local ok, result = ErrorHandler.eval("return 1+1")  -- true, 2
local ok, err = ErrorHandler.dofile("missing.lua")   -- false, "load error: ..."
-- Sur erreur : debug.traceback inclus, Logger.error() appelé automatiquement
```

---

## Section 19 — Audio Réactif (v2.7)

### 19.1 — Architecture audio

```
Micro → SDL3_audio (callback thread) → Ring buffer (std::mutex)
  ↓
AudioCaptureNode (AnimationNode, poll chaque frame)
  ↓
AudioAnalyzerNode (FFT Hann → spectrum → rms/peak/band_0..7)
  ↓
┌─ BeatDetectorNode (onset detection → beat trigger, BPM auto)
└─ BandSplitNode (Lua: band_0..2 → low, band_3..5 → mid, band_6..7 → high)
  ↓
DAG → effets réactifs (Perlin, particules, caméra, compositors)
```

**Thread safety :** Le callback SDL3_audio écrit dans un ring buffer via `std::mutex`. Le thread principal poll le buffer chaque frame. Aucune opération Lua dans le callback.

### 19.2 — AudioCapture

```cpp
// src/audio/AudioCapture.h
auto capture = new AudioCapture(44100, 2048);  // sampleRate, bufferSize
capture->start();   // ouvre le micro SDL3, retourne false si absent
capture->stop();
capture->poll(buffer);  // copie les N derniers samples
```

**Graceful fallback :** Si aucun micro n'est disponible, `start()` retourne false et le moteur continue sans audio.

### 19.3 — AudioAnalyzerNode

```cpp
// src/audio/AudioAnalyzer.h
auto analyzer = new AudioAnalyzerNode("analyzer", captureNode);
// Ports de sortie : rms (0..1), peak (0..1), band_0..band_7 (0..1)
```

**FFT :** Radix-2 Cooley-Tukey (header-only `kiss_fft.h`), fenêtre de Hann, 8 bandes fréquentielles.

### 19.4 — BeatDetectorNode

```cpp
// src/audio/BeatDetector.h
auto beat = new BeatDetectorNode("beat", analyzer);
// Port entrée : sensitivity (0..1), Port sortie : beat (trigger 0/1), bpm (float)
```

**Algorithme :** énergie courante > moyenne mobile × (1 + sensitivity × 2). Anti-bounce 200ms (max 300 BPM). BPM = 60 / moyenne des 16 derniers intervalles entre beats.

### 19.5 — Audio Lua wrapper

```lua
require "audio"
local audio = Audio:start()  -- crée toute la chaîne
audio:getRMS()               -- 0.0..1.0
audio:getPeak()              -- 0.0..1.0
audio:getBPM()               -- float (ex: 128.0)
audio:getBand("low")         -- basses 0..1
audio:getBand("mid")         -- médiums 0..1
audio:getBand("high")        -- aigus 0..1
-- Câblage DAG :
animator:addPort(audio.analyzer, "rms", perlinNode, "displacement")
```

### 19.6 — HUD Overlay

```lua
require "hud"
local h = HUD:new()
h:show()      -- affiche BPM + low/mid/high en overlay
h:toggle()    -- ou via REPL: hud()
h:update(audio)  -- appelé chaque frame pour mettre à jour les valeurs
```

### 19.7 — Sync auto-mode

```lua
local sync = Sync:new(song)
sync:setAutoMode(audio)  -- BPM auto-détecté pilote le séquenceur
sync:setAutoMode(nil)    -- retour au BPM fixe
```

---

## Section 20 — GPU & Shaders (v2.8)

### 20.1 — Pipeline shader OGRE 14.5

```
.glsl file → HighLevelGpuProgramManager::createProgram()
  ↓
Material (Pass → setVertexProgram / setFragmentProgram)
  ↓
Entity::setMaterial() → GPU rendering
  ↓
ShaderFxNode::update() → GpuProgramParameters::setNamedConstant() each frame
```

**Auto-params OGRE :** `worldViewProj`, `world`, `lightDiffuse`, `ambientLight`, `materialDiffuse` — mappés automatiquement par ShaderFxNode.

### 20.2 — ShaderFxNode

```cpp
// src/fx/ShaderFxNode.h
auto fx = new ShaderFxNode("perlin_gpu", "shaders/perlin_deform.glsl", "", scene, entity);
// Parses uniform float xxx; → creates input port "xxx"
// update() pushes port values → GPU uniforms each frame
// "time" uniform is auto-accumulated from dt
```

### 20.3 — Shader Lua wrapper

```lua
require "shader"
local fx = Shader:load("shaders/perlin_deform.glsl", {
    mesh = head.movable,
    displacement = 20.0,
    frequency = 3.0,
    speed = 1.5
})
fx:setUniform("frequency", 5.0)
-- Wire audio to GPU:
animator:addPort(audio.analyzer, "rms", fx._node, "displacement")
```

### 20.4 — PerlinGPU GLSL

Fichier : `resources/shaders/perlin_deform.glsl`

Vertex shader GLSL 330 avec bruit Perlin 3D simplex (Stefan Gustavson). Déplace chaque vertex le long de sa normale : `pos += normal * snoise(pos * frequency + time * speed) * displacement`.

Uniforms custom : `time` (auto-accumulé), `displacement`, `frequency`, `speed`.

### 20.5 — Profiler overlay

```lua
require "profiler"
perf()  -- toggle on/off, shows: Frame: 16.2ms (60 fps)
```
