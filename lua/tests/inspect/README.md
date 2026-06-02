# Inspect tests — BBFx v3.5.2 Sprint S8

Scripts Lua spécifiques pour `/inspect studio <script>` qui valident
**visuellement** les implémentations Sprint S8 (closure des trous post-audit
fact-checking 2026-05-10).

Chaque script :
1. Initialise une lumière ambiante minimale
2. Attend 5 frames (init OGRE / preset / template loaders prêts)
3. Charge le préset / template / setup graph in-line
4. Laisse le graphe vivant — `/inspect` peut alors screenshot le rendu

## Lancement type

```sh
# Depuis e:/prog/BBFx-Revival/ — /inspect lance bbfx-studio.exe depuis Debug/
/inspect studio lua/tests/inspect/<script>.lua
/inspect studio lua/tests/inspect/<script>.lua --animation
```

Le flag `--animation` prend 2 screenshots espacés (utile pour les scripts
qui ont une video, un feedback, un audio-react).

## Index des scripts

### Présets corrigés Sprint S8 Lot AE (material flow effectif)

| Script | Préset | Vérifie | Critère visuel |
|---|---|---|---|
| `inspect_s8_fanions.lua` | `fanions_dans_la_plaine` | Lot AE Fanions — chaîne 10 paires gray/color + JoystickRouter + bridge | Quad plein écran avec texture metal/heritage, **pas BaseWhite** |
| `inspect_s8_multibank.lua` | `multibank_chamber` | Lot AE multibank — chaîne MultiTextureBank + TextureBlend + bridge | Quad blend BumpyMetal+Water01 avec mask aureola, **pas BaseWhite** |
| `inspect_s8_vj_complete.lua` | `vj_complete_show` | Lot AE — 16+ nodes incl. 2 overlays empilés (z 0.02 + 0.01) | 2 overlays distincts (video bg + texture fg), **pas BaseWhite** |
| `inspect_s8_feedback_organic.lua` | `feedback_organic` | Lot AE — NoiseTexture + TextureFeedback + bridge | Perlin noise + trail (decay 0.92) visible, **--animation** révèle l'echo |
| `inspect_s8_spectrogram.lua` | `spectrogram_displacement` | Lot AE — AudioCapture + Spectrogram + bridge → mesh | Mesh (ogrehead) avec colormap viridis appliquée |
| `inspect_s8_video_scrub.lua` | `video_scrub_loop` | Lot AE — TheoraClip + VideoSlicer + bridge | Frame de bombe.ogg sur l'overlay plein écran |

### Baseline Sprint S5 (référence — déjà fonctionnel avant S8)

| Script | Préset | Vérifie |
|---|---|---|
| `inspect_s8_theora_geosphere.lua` | `theora_on_geosphere` | S5 baseline — TheoraClip → bridge → SceneObject Geosphere (vidéo sur mesh 3D) |

### Tests isolés des nouvelles APIs Sprint S8 Lot AC/AD

| Script | Vérifie | Critère visuel |
|---|---|---|
| `inspect_s8_mbr_multitarget.lua` | Lot AC — MaterialBridge ciblant FullscreenOverlay (target type !== SceneObjectNode) | Quad plein écran avec material `BBFx/Chrome` (chromé brillant) |
| `inspect_s8_consumer_port.lua` | Lot AC — port `material_source` natif FullscreenOverlay (Pattern 3 direct, SANS MaterialBridge intermédiaire) | Quad avec material du blend, **MaterialBridge absent du graphe** |
| `inspect_s8_cascade.lua` | Lot AD — cross-class cascade Pattern 4 (MaterialNode + MaterialBridge sur même mesh) | Geosphere avec **matB Chrome** (MaterialBridge connecté en 2nd → mApplySeq plus haut → gagne) |

### Templates créés Sprint S8 Lot AF (3 templates manquants au CDC)

| Script | Template | Vérifie |
|---|---|---|
| `inspect_s8_template_dual_video.lua` | `vj_dual_video_crossfade.lua` (CDC OBJ-352-114) | 2 TheoraClips + VideoCrossfade + bridge + overlay — quad video |
| `inspect_s8_template_texture_set.lua` | `vj_texture_set_classic.lua` (CDC OBJ-352-115) | Reproduction structurelle Fanions 2006 (BPM 144) |
| `inspect_s8_template_hybrid_3d.lua` | `vj_hybrid_3d_overlay.lua` (CDC OBJ-352-116) | Geosphere 3D + Perlin + overlay video translucide alpha 0.4 |

## Critères globaux de validation

Pour chaque script :

1. **Build préalable OK** : `cmake --build build/windows-debug --config Debug` exit 0
2. **PAS de crash** : `bbfx-studio.exe` reste vivant après 5+ secondes
3. **PAS d'écran BaseWhite** uniforme (sauf script de baseline qui doit en montrer un)
4. **Pas d'erreur Lua** dans la console (`[ERROR]`, `[lua]` panic, etc.)
5. **Mesh ou quad ou overlay visible** selon le scope du test
6. **Avec `--animation`** : changement détectable entre frame A et B pour les scripts marqués

## Tests négatifs implicites (par contraste)

Si l'un des fix Sprint S8 régresse, ces tests doivent **échouer visuellement** :
- `inspect_s8_fanions.lua` rendrait BaseWhite si le routing blend→bridge→overlay casse (Lot AE rollback)
- `inspect_s8_mbr_multitarget.lua` rendrait BaseWhite si MaterialBridge revient à la version SceneObjectNode-only (Lot AC rollback)
- `inspect_s8_consumer_port.lua` rendrait BaseWhite si le port `material_source` est retiré (Lot AC rollback)
- `inspect_s8_cascade.lua` rendrait matA au lieu de matB si MaterialNode n'a plus mApplySeq (Lot AD rollback)

## Méthodologie suggérée

1. **Smoke test global** : lancer les 12 scripts en mode `studio` simple pour vérifier qu'aucun ne crash
2. **Validation des fixes critiques** : lancer en priorité
   - `inspect_s8_fanions.lua` (trou critique #1 réparé)
   - `inspect_s8_mbr_multitarget.lua` (trou critique #2 réparé)
   - `inspect_s8_cascade.lua` (Pattern 4 alignement)
3. **Animations** : lancer `--animation` sur
   - `inspect_s8_feedback_organic.lua` (trail visible)
   - `inspect_s8_video_scrub.lua` (frame video change)
   - `inspect_s8_template_hybrid_3d.lua` (Perlin + video)

## Note technique

- Les scripts pompent l'init via un `LuaAnimationNode` connecté au `RootTimeNode.dt`
  (même pattern que `dbg_autotest.lua`).
- Aucun `os.exit()` — les scripts laissent le studio vivant pour permettre les screenshots.
- Le `taskkill` final est fait par `/inspect` après les captures.
- Les chemins `lua/templates/<name>.lua` sont relatifs au CWD = `Debug/`
  (où le bbfx-studio.exe est lancé par `/inspect`).

## Caveat — Heritage Pack cache (Sprint S7 Lot Y)

`inspect_s8_fanions.lua` (et `inspect_s8_template_texture_set.lua` côté
heritage si applicable) chargent le préset Fanions qui essaie d'utiliser le
manifest Heritage Pack (`lua/assets/heritage_pack.lua`, 712 entries). Le
fichier manifest est toujours présent dans le repo, mais les textures
binaires sont téléchargées via `tools/asset_pipeline.py` dans
`~/.bbfx/cache/`. Si le pipeline n'a pas été lancé localement :

- Le préset essaie quand même (`entry_count() >= 10` ⇒ heritage mode)
- OGRE log "Cannot locate resource ambientcg_*.jpg" (warning, **pas crash**)
- Le visuel Fanions montre l'overlay avec textures vides/blanches au lieu
  du blend texturé
- Le test runtime **FAN-004 reste PASS** car il vérifie le routing material,
  pas le contenu pixel

Pour un test visuel **complet** sur Fanions, il faut au préalable lancer :
```sh
cd e:/prog/BBFx-Revival/bbfx-revival
python tools/asset_pipeline.py run-all --lot E
```

Les autres scripts (`inspect_s8_multibank`, `_vj_complete`, `_feedback_organic`,
`_video_scrub`, `_mbr_multitarget`, `_consumer_port`, `_cascade`,
`_template_dual_video`, `_template_hybrid_3d`) utilisent les textures
bundled v3.5.1 (BumpyMetal.jpg etc.) et fonctionnent visuellement
**sans dépendance au cache Heritage**.

## Smoke test runtime confirmé (2026-05-11)

`./bbfx-studio.exe lua/tests/inspect/inspect_s8_fanions.lua` lancé en
arrière-plan reste vivant >6s, charge correctement le préset (`11 nodes
spawned, primary=fanions_dans_la_plaine_fullscreen_overlay`), aucune
exception Lua, le routing DAG Sprint S8 Lot AE est opérationnel.
