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
