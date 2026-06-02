# BBFx Studio — Usage guide (v3.5.2)

## Build & run

```sh
cd bbfx-revival
./extern/cmake/bin/cmake.exe -B build/windows-debug --preset windows-debug   # one-time
./extern/cmake/bin/cmake.exe --build build/windows-debug --target bbfx-studio --config Debug

cd build/windows-debug/Debug
./bbfx-studio.exe lua/demos/demo_studio.lua
```

The Lua source of truth is `bbfx-revival/lua/`. The build copies it to the
output directory; rebuild after editing Lua files to sync.

---

## VJ workflow (v3.5.2)

### Sprint S1 — reproduire le set Fanions historique

```lua
dbg.preset("fanions_dans_la_plaine")
```

Crée 10 nodes : `cam`, `light`, `cycle1`, `cycle2`, `blend`, `fullscreen_overlay`,
`gamepad`, `router_sweep`, `router_scroll_u`, `router_scroll_v`. Le bouton A
du gamepad déclenche le sweep (next preset) ; le bouton B maintenu permet aux
axes du stick gauche de piloter `scroll_u_a/b` et `scroll_v_a/b` du blend
fidèlement à 2006.

Note : 10 paires gray/color utilisent les textures Heritage v3.5.1 fallback
(BumpyMetal, Water01 et al.) en attendant le sourcing complet du Heritage
Pack CC0/CC-BY (~30-200 textures, tâche externe).

### Sprint S2 — VJing avec banque vidéo

```lua
-- Charger le template "vj_dual_video_crossfade"
dofile("lua/templates/vj_dual_video_crossfade.lua")
```

2 TheoraClipNodes + VideoCrossfadeNode + FullscreenOverlayNode + GamepadNode
+ JoystickRouterNode. Bouton 0 = next clip A, bouton 1 = next clip B,
axe 0 = beta crossfade.

### Sprint S3 — effets avancés (feedback, scrub, noise)

```lua
dbg.preset("feedback_organic")           -- texture feedback + noise procedural
dbg.preset("video_scrub_loop")           -- scrub video via gamepad axis
```

### Sprint S4 — show complet

```lua
dbg.preset("vj_complete_show")           -- 14 nodes : 2 video + crossfade + texture set + feedback + spectrogram displaced + audio-react
```

---

## Asset Manifest (v3.5.2)

BBFx v3.5.2 introduit un système de manifest d'assets indexé par **SHA-256**,
permettant de partager un projet `.bbfx` < 1 KB sans embarquer les médias.
Le cache local utilise une layout style git-objects :
`~/.bbfx/cache/<hash[0..1]>/<hash>`.

### API `bbfx.assets` Lua (8 méthodes)

```lua
-- Calculer le hash SHA-256 d'un fichier
local h = bbfx.assets.compute_sha256("resources/video/bombe.ogg")
print(h)   -- 64 caractères hex

-- Résoudre un asset par nom (manifest projet → cache → fallback filename)
local path = bbfx.assets.resolve("alien_cells")

-- Cache : info + vérifications
bbfx.assets.cache_root()                  -- "C:\Users\xxx\.bbfx\cache"
bbfx.assets.cache_path("abc123def...")    -- "<root>/ab/abc123def..."
bbfx.assets.is_cached("abc123def...")     -- true/false

-- Téléchargement HTTPS vérifié post-download (synchronous)
local path, err = bbfx.assets.download(
  "https://cdn.example.com/path/to/file.jpg",
  "0123456789abcdef..." -- 64-char SHA-256 attendu
)
if path == "" then print("Download failed: " .. err) end

-- Charger un manifest .lua dans l'instance globale
local n = bbfx.assets.load_pack("lua/assets/heritage_pack.lua")
print(n .. " entries loaded — total: " .. bbfx.assets.entry_count())
```

Sécurité :
- HTTPS strict (HTTP plain refusé)
- Limite 500 MB par asset
- Hash SHA-256 vérifié post-download (mismatch = abandon)

### Pipeline de production des packs (Phase 1 post-release)

Pour produire les 30 textures Heritage Pack (Lot E), 10 vidéos VJ Loops Pack
(Lot J) ou 200 textures Heritage Pack étendu (Lot N), un pipeline Python
automatisé est livré dans [`tools/asset_pipeline.py`](tools/asset_pipeline.py).
Voir le guide complet [`docs/asset_pipeline_guide.md`](../docs/asset_pipeline_guide.md).

```sh
pip install -r tools/requirements.txt
cp .env.example .env             # remplir R2_* + PIXABAY_API_KEY (Lot J)

# Lot E (textures CC0 — ambientCG + Polyhaven, sans clé API)
python tools/asset_pipeline.py run-all --lot E --skip upload,manifest

# Lot J (vidéos — PIXABAY_API_KEY dans .env + ffmpeg requis)
python tools/asset_pipeline.py run-all --lot J --skip upload,manifest --sources pixabay

# Upload vers CDN R2/B2/S3 + génération du manifest .lua final
# (bucket / endpoint / public-base / credentials lus depuis .env)
python tools/asset_pipeline.py upload   --lot E
python tools/asset_pipeline.py manifest --lot E   # writes lua/assets/heritage_pack.lua
```

Au démarrage, le Studio charge automatiquement `lua/assets/heritage_pack.lua` et
`lua/assets/video_library.lua` dans `bbfx.assets` — vides tant que le pipeline
n'a pas été exécuté, peuplés après.

CLI standalone (resolution batch) :
```sh
./bbfx.exe --resolve-assets project.bbfx
```

---

## Learn Panel (v3.5.2)

Ouvert via menu **View > Learn Panel** ou raccourci **Ctrl+Shift+L**.

Mappe chaque port DAG du graphe à une source d'input (MIDI CC, MIDI Note,
Gamepad button, Gamepad axis, Keyboard key) en un clic :

1. Cliquer sur **Learn** sur la ligne du port concerné
2. Bouger un slider / appuyer sur un bouton / presser une touche
3. Le binding est créé avec scale=1.0, offset=0.0, invert=false

Édition inline : DragFloat **Scale**, DragFloat **Offset**, Checkbox **Invert**.
Mode **Auto-map all** : capture séquentielle sur tous les ports filtrés —
mappe un contrôleur entier en 30 secondes.

Persistance : `state.extraJson["learn_bindings"]` (rétro-compatible MidiLearnManager).

---

## REPL Lua (v3.5.2)

Le **ConsolePanel** (menu View > Console) est un REPL Lua complet :

- **Single-line** : tape une expression, Enter pour exécuter
- **Multi-line** : checkbox `Multi-line`, écrire un bloc `do ... end`,
  Shift+Enter ou bouton Execute
- **Tab** : autocomplétion sur `dbg.<>`, `bbfx.<>`, noms de nodes
- **Up/Down** : navigation historique (max 1000 entrées)
- **Historique persistant** : `~/.bbfx/repl_history.lua`
  - Filtre regex `password|secret|token|api_key` non persistées

Exemples :
```lua
dbg.list()                                -- lister les nodes
dbg.set("blend", "scroll_u_a", 0.5)       -- piloter un port DAG
do
  for i = 1, 10 do
    dbg.set("cycle1", "next", 1.0)
    dbg.set("cycle1", "next", 0.0)        -- pulse
  end
end
```

---

## Outputs (Spout, NDI, ArtNet, ArtNet Video Mapping)

- **NDI / Spout / TextureShare** — déjà disponibles depuis v3.5
- **ArtnetOutputNode** — déjà depuis v3.5 (DMX channel-based)
- **ArtnetVideoMapperNode** (v3.5.2 nouveau) — pixel-mapping compatible
  MadMapper / Resolume Wire :

```lua
dbg.create("ArtnetVideoMapperNode", "leds")
dbg.set_param("leds", "target_ip", "192.168.1.42")
dbg.set_param("leds", "pixel_count_x", "16")
dbg.set_param("leds", "pixel_count_y", "16")
dbg.set_param("leds", "pixel_layout", "serpentine_horizontal")
dbg.set_param("leds", "pixel_format", "RGBW")
```

Multi-universe automatique (max 510 octets / universe), gamma 2.2 par défaut.

---

## Killer features

### SpectrogramTextureNode

Texture 2D du spectre audio temps réel en waterfall scroll horizontal.
Utilisable comme displacement map / mask / texture pour TextureBlend /
MaterialAnim / FullscreenOverlay.

```lua
dbg.preset("spectrogram_displacement")    -- demo : Geosphere + ShaderFxNode + spectrogram en displacement_map
```

4 colormaps inline (grayscale, viridis, plasma, magma), 3 freq scales
(linear, log, mel), 3 intensity scales (linear, log, dB).

### TextureFeedbackNode

Feedback non-linéaire (frame N-1 → frame N avec decay/displacement/zoom/rotate)
via shader GPU `feedback_node.frag` + 3 blend modes (Additive/Screen/Max).

```lua
dbg.preset("feedback_organic")            -- demo : feedback + noise procedural
```

### Joystick Router

Permet de mapper le pattern 2006 fidèle "bouton tenu = axes activent
des paramètres" vs "bouton pressé = trigger one-shot" :

```lua
dbg.create("JoystickRouterNode", "scroll")
dbg.set_param("scroll", "button_index", "1")     -- bouton B
dbg.set_param("scroll", "axis_index", "0")       -- leftStickX
dbg.set_param("scroll", "mode", "hold_gate")     -- gate par button
```

---

## Material bridges & video-on-mesh (Sprint S5)

Sprint S5 ouvre le routage **DAG-pure** entre les nodes producteurs de
material (`material_out` mirror) et la geometrie 3D (`SceneObjectNode`).

### MaterialBridgeNode — universal mat → mesh router

```lua
-- Cas 1 : routage statique d'un material existant
dbg.create("SceneObjectNode", "geo")
dbg.set_param("geo", "mesh", "Geosphere8000.mesh")
dbg.material_bridge("mb", "BBFx/Chrome", "unlit")
dbg.link("geo", "entity", "mb", "entity")

-- Cas 2 : auto-wrap d'un texture name
-- Si material_in pointe vers une texture (pas un material), le bridge cree
-- automatiquement `MatBridge_<node>_<tex>_<lighting>` avec single TUS.
dbg.material_bridge("mb_tex", "BumpyMetal.jpg", "unlit")

-- Cas 3 : pull dynamique du mirror d'un upstream node
-- material_source port suit la connexion DAG et lit le mirror upstream
-- (material_out / texture_out / current_texture / texture).
dbg.create("TextureBlendNode", "blend")
dbg.material_bridge("mb_dyn")
dbg.link("blend", "material_ready", "mb_dyn", "material_source")
dbg.link("geo",   "entity",         "mb_dyn", "entity")
```

Lighting modes :
- `unlit` (defaut) — texture pure, ideal video/RTT
- `lit` — diffuse + ambient enabled, modulation par lights
- `emissive` — selfIllumination = (1,1,1), brille en sombre

**Cascade** : si plusieurs MaterialBridge/TextureNode ciblent la meme entity,
le dernier connecte gagne (mApplySeq counter). Disable d'un node fait
reapparaitre le node sous-jacent.

### GrayscaleNode — runtime BT.709 desat

```lua
dbg.grayscale("g1", "BumpyMetal.jpg")          -- input texture name
dbg.set("g1", "mix", 1.0)                      -- 0=color, 1=gray (animable)
-- Output dispo via mirror texture_out (resolu auto par MaterialBridge)
dbg.material_bridge("mb_g")
dbg.link("g1",  "texture_ready", "mb_g", "material_source")
dbg.link("geo", "entity",        "mb_g", "entity")

-- Animation color↔gray live : link le mix port a un beat detector
dbg.link("beat", "trigger", "g1", "mix")
```

### TheoraClipNode → mesh 3D (preset theora_on_geosphere)

```lua
dbg.preset("theora_on_geosphere")
-- Cree : cam + light + geo (Geosphere8000) + clip (bombe.ogg) + mb (bridge)
-- La video s'applique sur la sphere via DAG pure.
```

Pattern equivalent en code Lua direct :
```lua
dbg.create("TheoraClipNode", "clip")              -- bombe.ogg par defaut
dbg.create("SceneObjectNode", "geo")
dbg.set_param("geo", "mesh", "Geosphere8000.mesh")
dbg.material_bridge("mb")
dbg.link("geo",  "entity",         "mb", "entity")
dbg.link("clip", "material_ready", "mb", "material_source")
```

`TheoraClipNode` expose desormais (Lot V) :
- ParamSpec mirror `material_out` (STRING) — `mClip->getMaterialName()` actualise chaque frame
- Port DAG output `material_ready` (FLOAT 0/1) — pulse 1.0 quand material valide ET clip playing

Pattern aligne sur VideoCrossfadeNode/VideoSlicerNode/VideoLibraryNode qui
exposent deja un `material_out`. Tous routables vers un mesh via MaterialBridgeNode.

---

## NodeEditor visual conventions (Sprint S6 Lot W)

Pour rendre le DAG plus lisible en cours d'edit, le NodeEditor + Inspector exposent
des codes couleurs et des indicateurs d'activite. Conventions etablies en Sprint S6 :

### Couleurs port (NodeEditor)

| Pattern de nom | Couleur | Signification |
|----------------|---------|----------------|
| `entity` ou `*_source` | 🟢 vert | Entity-link port — pointe vers un autre node, pas une valeur |
| `material_*` (`material_in`, `material_out`) | 🟠 orange | Material producteur ou consommateur |
| `texture_*`, `current_texture`, `slot_N_texture` | 🟣 magenta | Texture/RTT producteur ou consommateur |
| `*_ready`, `playing`, `trigger` | 🟡 jaune | Pulse/trigger (0..1, edge montant) |
| autre | ⚪ blanc | Float générique (slider/animable) |

### Bordure node "active"

Les nodes dont un output `*_ready` ou `playing` est actif (> 0.5) reçoivent une
**bordure verte légèrement pulsée** (sin(time*2) sur l'alpha 0.7..1.0). Cela rend
visible en un coup d'œil quel TheoraClipNode est en lecture, quel GrayscaleNode
est en train de re-render, quel TextureFeedbackNode boucle.

### Tooltips Inspector (hover)

Sur hover d'un port DAG ou d'un ParamSpec field dans l'Inspector, une bulle texte
explique son rôle. Tooltips populés sur les 16 nouveaux nodes (3 Sprint S5 + 13
release v3.5.2) au minimum sur les ports/params principaux.

### Mirrors read-only

Les ParamSpec sortantes (`material_out`, `texture_out`, `current_texture`,
`target_entity`, `slot_*_texture`) sont rendues comme **texte gris désactivé**
dans l'Inspector — pas un champ éditable. Elles refletent l'état runtime,
modifiables seulement par le node lui-même.

### Auto-layout & cascade visualization (Sprint S6 Lot X)

Quand un preset multi-node est chargé (`dbg.preset(...)`), le NodeEditor
**auto-layoute** les nodes nouvellement créés selon une heuristique :

- **Colonnes par catégorie** :
  - Col 0 (Input) : CameraNode, LightNode, GamepadNode, MidiInputNode, OscInputNode, AudioCaptureNode, BeatDetectorNode, JoystickRouterNode...
  - Col 1 (FX) : TextureBlend, NoiseTexture, Spectrogram, Grayscale, Feedback, ColorShift, ShaderFx, MaterialAnim, MultiTextureBank, TextureCycle...
  - Col 2 (Scene) : SceneObjectNode, BillboardLayer, FullscreenOverlay, MaterialBridge, Material, Texture, Theora, Video*...
  - Col 3 (Output) : Spout, NDI, Artnet, TextureShare...
- **Lignes par column** : empilement vertical séquentiel (160 px par row, 280 px par col)
- **Topological sort** : un node est placé en colonne `max(category, max_upstream_col + 1)` — assure le flux gauche-droite
- **Cycles DAG gérés** : TextureFeedbackNode auto-link ne provoque pas d'infinite loop
- **Préservation layout user absolue** : si un node est déjà à une position non-zero (drag user, restauration projet), l'auto-layout **NE TOUCHE JAMAIS** sa position

### Cascade visualization (multi-bridges sur même entity)

Quand plusieurs nodes Texture/Material/MaterialBridge ciblent la même
SceneObjectNode (ex. 2 MaterialBridges concurrents) :

- Le **link winner** (mApplySeq le plus élevé) est dessiné en **vert saturé épais (thickness 4)**.
- Les **links losers** (supplantés) sont dessinés en **gris transparent fin (thickness 2, alpha 0.4)**.
- Un **badge `[N]`** s'affiche au midpoint du link, indiquant l'ordre `mApplySeq`. Texte vert pour le winner, gris pour les losers.
- Effet : à coup d'œil tu vois quel bridge "gagne" et l'historique de connexion.

---

## Tests

```sh
cd build/windows-debug/Debug
./bbfx-studio.exe lua/dbg_autotest.lua
# === Results: 318 PASS, 0 FAIL ===
```

318 tests automatisés post-Sprint S6 (131 v3.5.2 + 187 baseline v3.5.1). Voir section finale pour le total post-Sprint S7.


---

## Heritage Pack runtime (v3.5.2 Sprint S7 Lot Y)

Le pipeline `tools/asset_pipeline.py` produit un manifest `lua/assets/heritage_pack.lua` listant les textures CC0/CC-BY (ambientCG, polyhaven, ...) téléchargées dans `~/.bbfx/cache/<hash[0..1]>/<hash>`. À partir de v3.5.2 RELEASED FINAL :

- `Engine.cpp` enregistre `~/.bbfx/cache/` comme `Ogre::ResourceLocation` recursive nommé "AssetCache" au démarrage. Toutes les textures du cache sont indexées par OGRE.
- `bbfx::AssetManifest::resolve(name)` retourne désormais le filename OGRE-friendly (ex. `ambientcg_bark004.jpg`) — pas le path absolu cache.
- `bbfx::AssetManifest::resolveAndLoad(name)` resolve + force-load via `TextureManager::load(filename, "AssetCache")` si non encore en mémoire. Use-case : après avoir résolu le nom, appliquer immédiatement à un TUS sans attendre un material rebuild.

Exemple Lua :
```lua
-- Test runtime e2e
local fn = bbfx.assets.resolve("ambientcg_bark004")  -- "ambientcg_bark004.jpg"
-- → directement consommable par MaterialBridgeNode auto-wrap
dbg.material_bridge("mb1", fn)  -- crée material MatBridge_mb1_<fn>_*
```

## LearnPanel (v3.5.2 Sprint S7 Lot Y)

Panel ImGui dédié au mapping MIDI/Joystick Learn — écrit en Sprint S6 Lot P, instancié dans StudioApp à partir de Sprint S7.

- **Toggle visibility** : menu `View → Learn Panel` ou raccourci `Ctrl+Shift+L`.
- **Update always-on** : `mLearnPanel->update()` est appelé chaque frame (sync MIDI Learn cumulatif), même panel masqué.
- **Render conditionnel** : la fenêtre ne s'affiche que si `mShowLearnPanel == true`. Bidirectionnel : la croix de fermeture met aussi `mShowLearnPanel = false`.

## AssetBrowser Heritage section (v3.5.2 Sprint S7 Lot Z)

L'AssetBrowserPanel expose une nouvelle section "Heritage Pack" sous les tags :

- Header `Heritage Pack (<total>)` avec compteur d'entries du manifest.
- Combo filtre `Category: [all|organic|cosmic|geometric|mask|gray_pair|video]`.
- Sous-sections TreeNode par category avec count.
- Hover tooltip : license, author, taille (KB), thumbnail (si chargé).
- Drag source `HERITAGE_TEXTURE` payload = nom logique.
- Drop target NodeEditor canvas : auto-resolve via `AssetManifest::resolveAndLoad` + crée TextureNode + auto-link à la SceneObjectNode sous le curseur (réutilise le pipeline TEXTURE_NAME drop existant).

## Auto-layout & cascade visualization (v3.5.2 Sprint S6 Lot X / Sprint S7 Lot Z)

- **Cascade label `[N]`** : les links MaterialBridge/TextureNode → SceneObjectNode affichent un badge avec le compteur `applySeq` au midpoint.
- **Winner highlight** : le bridge avec le plus haut `applySeq` est tracé en thick 4 (vs 2) avec alpha 0.4 sur les supplantes.
- **Auto-layout au load preset** : BFS topological + categoryColumn heuristique. Préserve les positions user (skip nodes à position non-zero). Cycle-safe via in-progress visited set (Sprint S7 Lot Z ALY-005 mesure 0.02 ms sur cycle explicite cy_a ↔ cy_b).

## Border pulse green (v3.5.2 Sprint S6 Lot W étendu Sprint S7 Lot AA)

NodeEditorPanel itère les outputs `*_ready` ou `playing` : valeur > 0.5 → border pulse vert sur le node. Sprint S7 Lot AA ajoute le port `texture_ready` sur :
- **NoiseTextureNode** : 1.0 si re-rendered, 0.0 sinon
- **SpectrogramTextureNode** : 1.0 chaque update (column scrolle)
- **TextureCycleNode** : 1.0 si transition active (0.001 < progress < 0.999)

## NodeEditor port tooltips (v3.5.2 Sprint S6 Lot W étendu Sprint S7 Lot Z)

Chaque port d'un node release v3.5.2 expose un tooltip dans l'Inspector au hover. Les 14 nodes release ont leurs ports user-facing populés (TLT-001 PASS 14/14).

## v3.5.2 RELEASED FINAL — récap

```bash
cd build/windows-debug/Debug
./bbfx-studio.exe lua/dbg_autotest.lua
# === Results: 446 PASS, 0 FAIL ===
```

**446 PASS / 0 FAIL** — total final post-Lot AW. (Les compteurs intermédiaires cités plus haut dans ce document — « 318 PASS » post-Sprint S6, « 326 PASS » au moment de la release initiale v3.5.2 — sont des jalons historiques S6/S7 ; le total courant est 446.)

**Lot AW (post-release, 2026-05-30)** : audit fonctionnel exhaustif du Studio — 59/59 contrôles morts/stubs câblés et +68 tests anti-régression, portant la suite de 378 à **446 PASS / 0 FAIL**. Retraits assumés : `FullscreenOverlayNode.camera_locked` (ne rendait pas) et `VideoLibraryNode` volume (pas d'audio).

Sprint S7 a fermé les 12 trous identifiés par méta-audit : 3 critiques (Heritage runtime, LearnPanel instanciation, Fanions migration), 5 moyens (tooltips coverage, ALY cycle réel, AssetBrowser sidebar, ENUM sync, ReversableClip instanceTag), 4 mineurs/décisions (TheoraClipNode factory, texture_ready ports, callback unregister API, GrayscaleNode GPU shader retrait).

---

*USAGE étendu Sprint S7 le 2026-05-09 — Sébastien JULLIEN.*
*BBFx v3.5.2 "VJ Reference Edition" — RELEASED FINAL.*
