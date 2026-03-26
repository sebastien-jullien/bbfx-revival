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
