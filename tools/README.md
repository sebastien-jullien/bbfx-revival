# BBFx tools/

Outils de développement et de production pour BBFx-Revival.

## `asset_pipeline.py`

Pipeline complet de production des packs d'assets externes (Lots E, J, N de
v3.5.2). Discover → Download → Process → Review → Hash → Upload (CDN) → Manifest.

Voir [`docs/asset_pipeline_guide.md`](../../docs/asset_pipeline_guide.md) pour
le guide d'utilisation détaillé.

```sh
pip install -r tools/requirements.txt
python tools/asset_pipeline.py --help
python tools/asset_pipeline.py run-all --lot E --skip upload,manifest
```

État de travail : `tools/.pipeline/` (gitignoré, recréé à chaque run).
Seeds curés    : `tools/seeds/lot_<E|J|N>.json`.

## `dbg_send.py`

Helper Python pour envoyer une commande au Studio Debugger via socket UDP local.
Utilisable depuis n'importe quel script externe pour piloter BBFx en marche.

## `theora_reverse.cpp` + `compile_reverse.bat`

Outil natif de génération d'un Theora reverse-clip à partir d'un .ogv source
(utilisé par `bbfx::ReversableClip`). DLLs ogg/theora/theoraenc/theoradec
fournies pour build standalone Windows.

```bat
compile_reverse.bat
theora_reverse.exe input.ogv output_reverse.ogv
```

## requirements.txt

Dépendances Python pour `asset_pipeline.py`. Pas requis pour `dbg_send.py`.
