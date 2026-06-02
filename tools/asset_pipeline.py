#!/usr/bin/env python3
"""
asset_pipeline.py — BBFx asset pipeline (Phase 1: Lots E + J).

Pipeline:
  1. discover  — query CC0 sources (ambientCG, Polyhaven, Pexels) -> candidates JSON
  2. download  — fetch candidates locally (sources/raw/)
  3. process   — resize, desaturate (gray pair), encode Theora (videos)
  4. review    — local HTTP review server (manual approval per asset)
  5. hash      — compute SHA-256 + write final manifest entries
  6. upload    — push to S3-compatible CDN (R2/B2/AWS) + write public URL into manifest
  7. manifest  — emit lua/assets/heritage_pack.lua and video_library.lua

Each stage is idempotent and can be re-run safely.

Usage:
  python asset_pipeline.py discover --lot E --limit 50
  python asset_pipeline.py download --lot E
  python asset_pipeline.py process  --lot E
  python asset_pipeline.py review   --lot E --port 8765
  python asset_pipeline.py hash     --lot E
  python asset_pipeline.py upload   --lot E --bucket bbfx-assets --endpoint https://<acct>.r2.cloudflarestorage.com
  python asset_pipeline.py manifest --lot E

  # Run everything except upload + manifest in one shot:
  python asset_pipeline.py run-all --lot E --skip upload,manifest

Environment variables (read by `upload` only):
  R2_ACCESS_KEY_ID     — Cloudflare R2 access key
  R2_SECRET_ACCESS_KEY — Cloudflare R2 secret
  (or AWS_*/B2_* equivalents — boto3 reads them transparently)

Author: Sebastien Jullien — BBFx v3.5.2 Phase 1
"""

import argparse
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import time
import urllib.parse
import zipfile
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Any, Dict, List, Optional

# ── Auto-load .env from the bbfx-revival project root ─────────────────────
# Looks for `<repo>/bbfx-revival/.env` first, then falls back to `tools/.env`.
# Silently no-op if python-dotenv isn't installed (the rest of the script
# still works as long as the env vars are exported manually).
def _load_dotenv() -> None:
    here = Path(__file__).resolve().parent       # tools/
    candidates = [here.parent / ".env", here / ".env"]
    try:
        from dotenv import load_dotenv
    except ImportError:
        # Manual fallback: parse a minimal KEY=VALUE format ourselves so the
        # pipeline still works on a fresh `pip install` that hasn't picked up
        # python-dotenv yet.
        for env_path in candidates:
            if env_path.exists():
                for line in env_path.read_text(encoding="utf-8").splitlines():
                    line = line.strip()
                    if not line or line.startswith("#") or "=" not in line:
                        continue
                    k, _, v = line.partition("=")
                    k = k.strip()
                    v = v.strip().strip('"').strip("'")
                    # Don't overwrite vars already set in the shell — explicit beats implicit.
                    os.environ.setdefault(k, v)
                return
        return
    for env_path in candidates:
        if env_path.exists():
            load_dotenv(env_path, override=False)
            return

_load_dotenv()

# Lazy imports — each stage checks for its own deps so that `--help` and
# stages that don't need them work without the full install.
def _require(module: str, hint: str) -> Any:
    try:
        return __import__(module)
    except ImportError:
        print(f"ERROR: '{module}' not installed. {hint}", file=sys.stderr)
        sys.exit(2)


def _have(module: str) -> bool:
    try:
        __import__(module)
        return True
    except ImportError:
        return False


# tqdm optional (graceful fallback) — used everywhere, cheap to stub.
try:
    from tqdm import tqdm
except ImportError:
    def tqdm(it, **_):
        return it


# ───────────────────────────────────────────────────────────────────────────
# Layout
# ───────────────────────────────────────────────────────────────────────────

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT  = SCRIPT_DIR.parent          # bbfx-revival/
SEEDS_DIR  = SCRIPT_DIR / "seeds"
WORK_DIR   = SCRIPT_DIR / ".pipeline"   # gitignored working area
MANIFEST_DIR = REPO_ROOT / "lua" / "assets"
THIRDPARTY_DOC = REPO_ROOT.parent / "docs" / "THIRD_PARTY_ASSETS.md"


# ───────────────────────────────────────────────────────────────────────────
# Data model
# ───────────────────────────────────────────────────────────────────────────

@dataclass
class Candidate:
    """A single asset candidate flowing through the pipeline."""
    name: str                      # logical name ("organic_cells_01")
    source: str                    # "ambientcg" | "polyhaven" | "pexels" | "manual"
    source_id: str                 # upstream asset id
    asset_type: str                # "texture" | "video" | "mask"
    category: str                  # "organic" | "cosmic" | "geometric" | "mask" | "abstract" | ...
    download_url: str              # upstream HTTPS URL
    license: str                   # "CC0" | "CC-BY" | "CC-BY-SA" | ...
    author: str = ""               # required for CC-BY attribution
    width: int = 0
    height: int = 0
    duration_sec: float = 0.0      # videos only

    # Filled by later stages:
    local_path: str = ""           # after `download` and `process`
    gray_pair_path: str = ""       # for color textures with auto-generated gray pair
    pal_variant_path: str = ""     # for videos: the 720x576 PAL .ogv (sibling of canonical 720p)
    sha256: str = ""               # after `hash`
    size_bytes: int = 0            # after `hash`
    cdn_url: str = ""              # after `upload`
    approved: Optional[bool] = None   # after `review` (None = pending)
    notes: str = ""

    def slug(self) -> str:
        """Filesystem-safe identifier."""
        return f"{self.source}_{self.source_id}".replace("/", "_").lower()


@dataclass
class LotState:
    """Working state for a lot (E, J, or N), persisted to JSON."""
    lot: str
    candidates: List[Candidate] = field(default_factory=list)

    def save(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(
            {"lot": self.lot, "candidates": [asdict(c) for c in self.candidates]},
            indent=2, ensure_ascii=False
        ), encoding="utf-8")

    @classmethod
    def load(cls, path: Path, lot: str) -> "LotState":
        if not path.exists():
            return cls(lot=lot)
        raw = json.loads(path.read_text(encoding="utf-8"))
        return cls(lot=raw["lot"], candidates=[Candidate(**c) for c in raw["candidates"]])


# ───────────────────────────────────────────────────────────────────────────
# Lot configuration
# ───────────────────────────────────────────────────────────────────────────

LOT_CONFIG: Dict[str, Dict[str, Any]] = {
    "E": {
        "asset_types":  ["texture", "mask"],
        "target_count": 30,
        "categories":   ["organic", "cosmic", "geometric", "mask"],
        "manifest_out": "heritage_pack.lua",
        "default_resolution": "1k",
    },
    "J": {
        "asset_types":  ["video"],
        "target_count": 10,
        "categories":   ["abstract", "geometric", "particles", "slowmo", "glitch"],
        "manifest_out": "video_library.lua",
        "default_resolutions_video": [(1280, 720), (720, 576)],
    },
    "N": {
        "asset_types":  ["texture", "mask"],
        "target_count": 200,
        "categories":   ["organic", "cosmic", "geometric", "mask", "pairs"],
        "manifest_out": "heritage_pack.lua",
        "default_resolution": "1k",
        "extends_from": "E",
    },
}


def lot_state_path(lot: str) -> Path:
    return WORK_DIR / f"lot_{lot.lower()}_state.json"


# ───────────────────────────────────────────────────────────────────────────
# Stage 1 — DISCOVER
# ───────────────────────────────────────────────────────────────────────────

def stage_discover(lot: str, limit: int, sources: List[str], pexels_api_key: Optional[str]) -> None:
    """Query CC0 sources and merge the pre-curated seed file. Idempotent: re-run grows the candidate pool."""
    global requests
    requests = _require("requests", "Run: pip install -r tools/requirements.txt")
    cfg = LOT_CONFIG[lot]
    state = LotState.load(lot_state_path(lot), lot)

    # Lot extension: if `extends_from` is set and the current state is empty,
    # bootstrap by copying the parent lot's state (so the merged manifest
    # contains both lots without duplication).
    if not state.candidates:
        parent = cfg.get("extends_from")
        if parent:
            parent_state = LotState.load(lot_state_path(parent), parent)
            if parent_state.candidates:
                state.candidates = list(parent_state.candidates)
                print(f"[discover] bootstrapped from lot {parent}: {len(state.candidates)} candidates inherited")

    seen = {c.slug() for c in state.candidates}

    # 1) Load curated seed (always merged first — these are hand-picked).
    seed_path = SEEDS_DIR / f"lot_{lot.lower()}.json"
    if seed_path.exists():
        seed = json.loads(seed_path.read_text(encoding="utf-8"))
        added = 0
        for entry in seed:
            cand = Candidate(**entry)
            if cand.slug() not in seen:
                state.candidates.append(cand)
                seen.add(cand.slug())
                added += 1
        print(f"[discover] +{added} candidates from seed {seed_path.name} ({len(seed)} entries, {len(seed)-added} dedup)")

    # 2) Live-query sources.
    if "ambientcg" in sources and "texture" in cfg["asset_types"]:
        added = _discover_ambientcg(state, seen, limit)
        print(f"[discover] +{added} candidates from ambientCG")

    if "polyhaven" in sources and "texture" in cfg["asset_types"]:
        added = _discover_polyhaven(state, seen, limit)
        print(f"[discover] +{added} candidates from Polyhaven")

    if "pexels" in sources and "video" in cfg["asset_types"]:
        if not pexels_api_key:
            print("[discover] Pexels skipped (no --pexels-key / PEXELS_API_KEY env).")
        else:
            added = _discover_pexels(state, seen, limit, pexels_api_key)
            print(f"[discover] +{added} candidates from Pexels")

    if "pixabay" in sources and "video" in cfg["asset_types"]:
        pixabay_key = os.environ.get("PIXABAY_API_KEY")
        if not pixabay_key:
            print("[discover] Pixabay skipped (no PIXABAY_API_KEY env).")
        else:
            added = _discover_pixabay_video(state, seen, limit, pixabay_key)
            print(f"[discover] +{added} candidates from Pixabay")

    # ── Lot-specific augmentations (Lot N: cosmic photos + synthetic masks) ──
    if "pixabay-img" in sources and "texture" in cfg["asset_types"]:
        pixabay_key = os.environ.get("PIXABAY_API_KEY")
        if not pixabay_key:
            print("[discover] Pixabay images skipped (no PIXABAY_API_KEY env).")
        else:
            cosmic_queries = ["nebula", "galaxy", "plasma", "aurora", "lightning"]
            added = _discover_pixabay_image(state, seen, limit, pixabay_key,
                                            cosmic_queries, "cosmic")
            print(f"[discover] +{added} cosmic photos from Pixabay")

    if "synthetic-masks" in sources and "mask" in cfg["asset_types"]:
        masks_dir = WORK_DIR / lot.lower() / "synthetic_masks"
        added = _generate_synthetic_masks(state, seen, masks_dir)
        print(f"[discover] +{added} synthetic masks generated")

    state.save(lot_state_path(lot))
    print(f"[discover] Lot {lot}: {len(state.candidates)} total candidates "
          f"(target = {cfg['target_count']}). State saved to {lot_state_path(lot)}")


def _discover_ambientcg(state: LotState, seen: set, limit: int) -> int:
    """ambientCG full_json. Every asset is CC0 by site policy."""
    # Categories that yield VJ-friendly textures:
    queries = ["Wood", "Metal", "Fabric", "Concrete", "Marble", "Ground", "Tiles", "Plastic", "Rock"]
    count = 0
    for cat in queries:
        url = ("https://ambientcg.com/api/v2/full_json"
               f"?type=Material&include=imageData,downloadData&limit={limit}&category={cat}")
        try:
            r = requests.get(url, timeout=30)
            r.raise_for_status()
            data = r.json()
        except Exception as e:
            print(f"[discover/ambientcg] {cat}: {e}", file=sys.stderr)
            continue
        for asset in data.get("foundAssets", []):
            asset_id = asset.get("assetId")
            if not asset_id:
                continue
            # Pick smallest available zip (1K-JPG preferred for VJ; 2K only if 1K absent).
            # ambientCG occasionally returns `[]` instead of `{}` at one of the
            # nested levels for assets without published downloads (e.g. Ground102).
            # Type-guard each step so we skip those gracefully.
            def _dig(d, *keys, default=None):
                cur = d
                for k in keys:
                    if not isinstance(cur, dict):
                        return default
                    cur = cur.get(k)
                return cur if cur is not None else default
            zips = _dig(asset, "downloadFolders", "default",
                        "downloadFiletypeCategories", "zip", "downloads", default=[])
            if not isinstance(zips, list):
                continue
            target = None
            for z in zips:
                attr = z.get("attribute", "")
                if "1K-JPG" in attr:
                    target = z
                    break
            if not target and zips:
                target = zips[0]
            if not target:
                continue
            cand = Candidate(
                name=f"ambientcg_{asset_id}".lower(),
                source="ambientcg",
                source_id=asset_id,
                asset_type="texture",
                category=_classify_category(asset.get("displayCategory", "")),
                download_url=target.get("downloadLink", ""),
                license="CC0",
                width=1024, height=1024,
            )
            if cand.slug() in seen or not cand.download_url:
                continue
            state.candidates.append(cand)
            seen.add(cand.slug())
            count += 1
    return count


def _discover_polyhaven(state: LotState, seen: set, limit: int) -> int:
    """Polyhaven /assets?type=textures + /files/<id> for download URLs (with MD5)."""
    try:
        r = requests.get("https://api.polyhaven.com/assets?type=textures", timeout=30)
        r.raise_for_status()
        listing = r.json()
    except Exception as e:
        print(f"[discover/polyhaven] listing: {e}", file=sys.stderr)
        return 0

    count = 0
    for asset_id, meta in list(listing.items())[:limit]:
        try:
            f = requests.get(f"https://api.polyhaven.com/files/{asset_id}", timeout=30)
            f.raise_for_status()
            files = f.json()
        except Exception:
            continue

        # Some Polyhaven assets ship only Albedo, others Diffuse; some have
        # only 2k or 4k (not 1k). Guard each level — skip on shape mismatch.
        diffuse_block = files.get("Diffuse") or files.get("Albedo") or {}
        if not isinstance(diffuse_block, dict):
            continue
        res_block = diffuse_block.get("1k") or diffuse_block.get("2k") or {}
        if not isinstance(res_block, dict):
            continue
        diffuse = res_block.get("jpg") or {}
        if not isinstance(diffuse, dict):
            continue
        url = diffuse.get("url")
        md5 = diffuse.get("md5", "")
        if not url:
            continue

        author = ""
        authors = meta.get("authors") or {}
        if authors:
            author = next(iter(authors.keys()))

        cats = meta.get("categories") or []
        cand = Candidate(
            name=f"polyhaven_{asset_id}".lower(),
            source="polyhaven",
            source_id=asset_id,
            asset_type="texture",
            category=_classify_category(",".join(cats)),
            download_url=url,
            license="CC0",   # entire Polyhaven library is CC0
            author=author,
            width=1024, height=1024,
            notes=f"upstream_md5={md5}",
        )
        if cand.slug() in seen:
            continue
        state.candidates.append(cand)
        seen.add(cand.slug())
        count += 1
    return count


def _discover_pexels(state: LotState, seen: set, limit: int, api_key: str) -> int:
    """Pexels Videos API — Pexels License (commercial OK, attribution welcomed)."""
    queries = ["abstract", "geometric pattern", "particles", "glitch", "neon lights"]
    count = 0
    for q in queries:
        try:
            r = requests.get(
                "https://api.pexels.com/videos/search",
                params={"query": q, "per_page": min(limit, 20), "size": "medium"},
                headers={"Authorization": api_key},
                timeout=30,
            )
            r.raise_for_status()
            data = r.json()
        except Exception as e:
            print(f"[discover/pexels] {q}: {e}", file=sys.stderr)
            continue
        for v in data.get("videos", []):
            files = v.get("video_files", [])
            # Pick HD mp4 (we re-encode to Theora downstream).
            target = next((f for f in files if f.get("quality") == "hd" and f.get("file_type") == "video/mp4"), None)
            if not target:
                continue
            cand = Candidate(
                name=f"pexels_{v['id']}".lower(),
                source="pexels",
                source_id=str(v["id"]),
                asset_type="video",
                category=_classify_category(q),
                download_url=target["link"],
                license="Pexels",   # commercial-OK, attribution welcomed
                author=v.get("user", {}).get("name", ""),
                width=target.get("width", 1280),
                height=target.get("height", 720),
                duration_sec=float(v.get("duration", 0)),
            )
            if cand.slug() in seen:
                continue
            state.candidates.append(cand)
            seen.add(cand.slug())
            count += 1
    return count


def _discover_pixabay_image(state: LotState, seen: set, limit: int, api_key: str,
                             queries: List[str], category_label: str) -> int:
    """Pixabay still-image API — Pixabay Content License (commercial OK, no attribution required).

    Used for Lot N to fill the 'cosmic' category (nebula / galaxy / plasma / aurora /
    lightning) which ambientCG and Polyhaven don't cover.
    """
    count = 0
    for q in queries:
        try:
            r = requests.get(
                "https://pixabay.com/api/",
                params={"key": api_key, "q": q, "image_type": "photo",
                        "per_page": min(limit, 20), "safesearch": "true",
                        "min_width": 1024, "min_height": 1024},
                timeout=30,
            )
            r.raise_for_status()
            data = r.json()
        except Exception as e:
            print(f"[discover/pixabay-img] {q}: {e}", file=sys.stderr)
            continue
        for hit in data.get("hits", []):
            url = hit.get("largeImageURL") or hit.get("webformatURL")
            if not url:
                continue
            cand = Candidate(
                name=f"pixabay_img_{hit['id']}".lower(),
                source="pixabay",
                source_id=str(hit["id"]),
                asset_type="texture",
                category=category_label,
                download_url=url,
                license="Pixabay",
                author=hit.get("user", ""),
                width=hit.get("imageWidth", 1024),
                height=hit.get("imageHeight", 1024),
            )
            if cand.slug() in seen:
                continue
            state.candidates.append(cand)
            seen.add(cand.slug())
            count += 1
    return count


def _discover_pixabay_video(state: LotState, seen: set, limit: int, api_key: str) -> int:
    """Pixabay video API — Pixabay Content License (commercial OK, no attribution required)."""
    queries = ["abstract", "geometric", "particles", "neon", "glitch"]
    count = 0
    for q in queries:
        try:
            r = requests.get(
                "https://pixabay.com/api/videos/",
                params={"key": api_key, "q": q, "per_page": min(limit, 20),
                        "video_type": "all", "safesearch": "true"},
                timeout=30,
            )
            r.raise_for_status()
            data = r.json()
        except Exception as e:
            print(f"[discover/pixabay] {q}: {e}", file=sys.stderr)
            continue
        for hit in data.get("hits", []):
            videos = hit.get("videos", {})
            # Pixabay returns: large > medium > small > tiny. Pick the smallest >= 1280x720.
            target = None
            for size_key in ("medium", "small", "large"):
                v = videos.get(size_key, {})
                if v.get("width", 0) >= 1280 and v.get("url"):
                    target = v
                    break
            if not target:
                continue
            cand = Candidate(
                name=f"pixabay_{hit['id']}".lower(),
                source="pixabay",
                source_id=str(hit["id"]),
                asset_type="video",
                category=_classify_category(q),
                download_url=target["url"],
                license="Pixabay",   # commercial-OK, no attribution required
                author=hit.get("user", ""),
                width=target.get("width", 1280),
                height=target.get("height", 720),
                duration_sec=float(hit.get("duration", 0)),
            )
            if cand.slug() in seen:
                continue
            state.candidates.append(cand)
            seen.add(cand.slug())
            count += 1
    return count


def _generate_synthetic_masks(state: LotState, seen: set, out_dir: Path) -> int:
    """Procedurally generate the 20 mask textures missing from CC0 libraries.

    Categories produced:
      - 4 linear gradients (vertical, horizontal, diag-up, diag-down)
      - 4 radial gradients (centered, off-center 4 quadrants)
      - 4 sweep bands (horizontal narrow, vertical narrow, diagonal, S-curve)
      - 4 dot patterns (regular grid, halftone, voronoi, scattered)
      - 4 noise maps (small/medium/large/extra-large grain)

    The synthetic Candidate has download_url empty; local_path is set directly
    to the generated PNG. Hash + upload + manifest treat it like any other.
    """
    if not _have("PIL"):
        print("[discover/masks] Pillow missing — skipping synthetic mask generation", file=sys.stderr)
        return 0
    from PIL import Image, ImageDraw, ImageFilter
    import math, random

    out_dir.mkdir(parents=True, exist_ok=True)
    SIZE = 1024

    # ── Helpers ─────────────────────────────────────────────────────────
    def lin_grad(angle_deg: float) -> Image.Image:
        im = Image.new("L", (SIZE, SIZE))
        a = math.radians(angle_deg)
        cx, cy = math.cos(a), math.sin(a)
        diag = abs(cx) * SIZE + abs(cy) * SIZE
        px = im.load()
        for y in range(SIZE):
            for x in range(SIZE):
                t = (x * cx + y * cy) / diag + 0.5
                px[x, y] = int(255 * max(0.0, min(1.0, t)))
        return im

    def radial(cx_n: float, cy_n: float, falloff: float = 1.0) -> Image.Image:
        im = Image.new("L", (SIZE, SIZE))
        cx, cy = cx_n * SIZE, cy_n * SIZE
        rmax = math.hypot(SIZE, SIZE) * 0.5 / falloff
        px = im.load()
        for y in range(SIZE):
            for x in range(SIZE):
                d = math.hypot(x - cx, y - cy) / rmax
                px[x, y] = int(255 * max(0.0, min(1.0, 1.0 - d)))
        return im

    def sweep_band(angle_deg: float, width_n: float = 0.15) -> Image.Image:
        im = Image.new("L", (SIZE, SIZE))
        a = math.radians(angle_deg)
        cx, cy = math.cos(a), math.sin(a)
        # signed distance from center along direction (cx, cy)
        px = im.load()
        cx0, cy0 = SIZE * 0.5, SIZE * 0.5
        half = SIZE * width_n * 0.5
        for y in range(SIZE):
            for x in range(SIZE):
                d = (x - cx0) * cx + (y - cy0) * cy
                t = max(0.0, 1.0 - abs(d) / half)
                px[x, y] = int(255 * t)
        return im

    def s_curve_band() -> Image.Image:
        im = Image.new("L", (SIZE, SIZE))
        px = im.load()
        for y in range(SIZE):
            x_center = SIZE * (0.5 + 0.25 * math.sin(2 * math.pi * y / SIZE))
            half = SIZE * 0.08
            for x in range(SIZE):
                t = max(0.0, 1.0 - abs(x - x_center) / half)
                px[x, y] = int(255 * t)
        return im

    def dot_grid(spacing: int, dot_radius: int) -> Image.Image:
        im = Image.new("L", (SIZE, SIZE), 0)
        d = ImageDraw.Draw(im)
        for y in range(0, SIZE, spacing):
            for x in range(0, SIZE, spacing):
                d.ellipse((x - dot_radius, y - dot_radius,
                           x + dot_radius, y + dot_radius), fill=255)
        return im

    def halftone() -> Image.Image:
        im = Image.new("L", (SIZE, SIZE), 0)
        d = ImageDraw.Draw(im)
        spacing = 32
        for y in range(0, SIZE, spacing):
            for x in range(0, SIZE, spacing):
                # radius shrinks toward edges (vignette-like)
                cx, cy = SIZE * 0.5, SIZE * 0.5
                dist = math.hypot(x - cx, y - cy) / (SIZE * 0.7)
                r = max(2, int(spacing * 0.45 * (1.0 - min(1.0, dist))))
                d.ellipse((x - r, y - r, x + r, y + r), fill=255)
        return im

    def voronoi_dots(seed: int = 0) -> Image.Image:
        rng = random.Random(seed)
        N = 80
        pts = [(rng.randint(0, SIZE-1), rng.randint(0, SIZE-1)) for _ in range(N)]
        im = Image.new("L", (SIZE, SIZE))
        px = im.load()
        max_d = SIZE * 0.08
        for y in range(SIZE):
            for x in range(SIZE):
                d = min(math.hypot(x-p[0], y-p[1]) for p in pts)
                t = max(0.0, 1.0 - d / max_d)
                px[x, y] = int(255 * t)
        return im

    def scattered_dots(seed: int = 1) -> Image.Image:
        rng = random.Random(seed)
        im = Image.new("L", (SIZE, SIZE), 0)
        d = ImageDraw.Draw(im)
        for _ in range(600):
            x, y = rng.randint(0, SIZE-1), rng.randint(0, SIZE-1)
            r = rng.randint(2, 12)
            d.ellipse((x-r, y-r, x+r, y+r), fill=rng.randint(180, 255))
        return im

    def noise_map(scale: int, seed: int) -> Image.Image:
        # Coarse noise then upscale + blur
        rng = random.Random(seed)
        small = Image.new("L", (scale, scale))
        px = small.load()
        for y in range(scale):
            for x in range(scale):
                px[x, y] = rng.randint(0, 255)
        im = small.resize((SIZE, SIZE), Image.BILINEAR)
        return im.filter(ImageFilter.GaussianBlur(radius=2))

    # ── Catalog of 20 masks ─────────────────────────────────────────────
    catalog = [
        ("mask_grad_v",       lambda: lin_grad(90)),
        ("mask_grad_h",       lambda: lin_grad(0)),
        ("mask_grad_diag_up", lambda: lin_grad(45)),
        ("mask_grad_diag_dn", lambda: lin_grad(-45)),
        ("mask_radial_c",     lambda: radial(0.5, 0.5, 1.0)),
        ("mask_radial_tl",    lambda: radial(0.25, 0.25, 0.7)),
        ("mask_radial_tr",    lambda: radial(0.75, 0.25, 0.7)),
        ("mask_radial_bl",    lambda: radial(0.25, 0.75, 0.7)),
        ("mask_sweep_h",      lambda: sweep_band(0,    0.12)),
        ("mask_sweep_v",      lambda: sweep_band(90,   0.12)),
        ("mask_sweep_diag",   lambda: sweep_band(45,   0.10)),
        ("mask_sweep_s",      s_curve_band),
        ("mask_dots_grid_sm", lambda: dot_grid(spacing=24, dot_radius=6)),
        ("mask_dots_grid_lg", lambda: dot_grid(spacing=64, dot_radius=14)),
        ("mask_halftone",     halftone),
        ("mask_voronoi",      lambda: voronoi_dots(seed=42)),
        ("mask_scattered",    lambda: scattered_dots(seed=7)),
        ("mask_noise_fine",   lambda: noise_map(scale=64,  seed=101)),
        ("mask_noise_med",    lambda: noise_map(scale=32,  seed=202)),
        ("mask_noise_coarse", lambda: noise_map(scale=12,  seed=303)),
    ]

    added = 0
    for name, gen in catalog:
        slug_check = f"synthetic_{name}".lower()
        if any(c.slug() == slug_check for c in state.candidates):
            continue
        png_path = out_dir / f"{name}.png"
        if not png_path.exists():
            img = gen()
            img.save(png_path, "PNG", optimize=True)
        cand = Candidate(
            name=name,
            source="synthetic",
            source_id=name,
            asset_type="mask",
            category="mask",
            download_url="",         # synthetic, no upstream
            license="CC0",           # we own the generator → public domain
            author="bbfx asset_pipeline",
            width=SIZE, height=SIZE,
            local_path=str(png_path),
            approved=True,           # auto-approve generated content
            notes="procedurally generated",
        )
        if cand.slug() in seen:
            continue
        state.candidates.append(cand)
        seen.add(cand.slug())
        added += 1
    return added


def _classify_category(s: str) -> str:
    s = (s or "").lower()
    if any(k in s for k in ("wood", "fabric", "leaf", "plant", "ground", "moss", "skin")):
        return "organic"
    if any(k in s for k in ("nebula", "star", "galaxy", "plasma", "neon", "light")):
        return "cosmic"
    if any(k in s for k in ("metal", "concrete", "tile", "brick", "marble", "geometric", "pattern")):
        return "geometric"
    if any(k in s for k in ("gradient", "mask", "fade")):
        return "mask"
    if "particle" in s:
        return "particles"
    if "glitch" in s:
        return "glitch"
    return "abstract"


# ───────────────────────────────────────────────────────────────────────────
# Stage 2 — DOWNLOAD
# ───────────────────────────────────────────────────────────────────────────

def stage_download(lot: str) -> None:
    global requests
    requests = _require("requests", "Run: pip install -r tools/requirements.txt")
    state = LotState.load(lot_state_path(lot), lot)
    raw_dir = WORK_DIR / lot.lower() / "raw"
    raw_dir.mkdir(parents=True, exist_ok=True)

    for cand in tqdm(state.candidates, desc=f"download lot {lot}"):
        if cand.local_path and Path(cand.local_path).exists():
            continue
        ext = _guess_extension(cand)
        dest = raw_dir / f"{cand.slug()}{ext}"
        try:
            with requests.get(cand.download_url, stream=True, timeout=120) as r:
                r.raise_for_status()
                with open(dest, "wb") as f:
                    for chunk in r.iter_content(8192):
                        f.write(chunk)
        except Exception as e:
            print(f"[download] {cand.slug()}: {e}", file=sys.stderr)
            continue
        cand.local_path = str(dest)

    state.save(lot_state_path(lot))


def _guess_extension(c: Candidate) -> str:
    """Pick a file extension from URL path AND query string.

    ambientCG download links have the form `https://ambientcg.com/get?file=Wood095_1K-JPG.zip`
    so the extension lives in the `file=` query parameter, not the path.
    """
    parsed = urllib.parse.urlparse(c.download_url)
    candidates_str = [parsed.path.lower()]
    qs = urllib.parse.parse_qs(parsed.query)
    for key in ("file", "filename", "name"):
        for v in qs.get(key, []):
            candidates_str.append(v.lower())
    known = (".zip", ".jpg", ".jpeg", ".png", ".mp4", ".webm", ".mov", ".ogv")
    for s in candidates_str:
        for ext in known:
            if s.endswith(ext):
                return ".jpg" if ext == ".jpeg" else ext
    return ".bin"


# ───────────────────────────────────────────────────────────────────────────
# Stage 3 — PROCESS (extract zip, resize, gray-pair, theora encode)
# ───────────────────────────────────────────────────────────────────────────

def stage_process(lot: str, max_dim: int = 1024) -> None:
    global Image
    pil = _require("PIL", "Pillow missing. Run: pip install -r tools/requirements.txt")
    from PIL import Image  # noqa: F811 — re-bind global
    state = LotState.load(lot_state_path(lot), lot)
    out_dir = WORK_DIR / lot.lower() / "processed"
    out_dir.mkdir(parents=True, exist_ok=True)

    for cand in tqdm(state.candidates, desc=f"process lot {lot}"):
        if not cand.local_path or not Path(cand.local_path).exists():
            continue
        src = Path(cand.local_path)

        if cand.asset_type == "texture" or cand.asset_type == "mask":
            _process_image(cand, src, out_dir, max_dim)
        elif cand.asset_type == "video":
            _process_video(cand, src, out_dir)

    # Materialize gray pairs (textures) and PAL variants (videos) as
    # first-class sibling candidates so they flow through hash/upload/
    # manifest naturally and become independently resolvable at runtime
    # via `bbfx.assets.resolve("<name>_gray")` / `bbfx.assets.resolve("<name>_pal")`.
    _inject_gray_siblings(state)
    _inject_pal_siblings(state)

    state.save(lot_state_path(lot))


def _inject_pal_siblings(state: LotState) -> None:
    """For every video with a generated 720x576 PAL variant, ensure a sibling
    Candidate `<name>_pal` exists. Idempotent — won't duplicate on re-run.
    """
    existing = {c.name for c in state.candidates}
    new_siblings: List[Candidate] = []
    for c in state.candidates:
        if not c.pal_variant_path:
            continue
        if not Path(c.pal_variant_path).exists():
            continue
        pal_name = f"{c.name}_pal"
        if pal_name in existing:
            continue
        sibling = Candidate(
            name=pal_name,
            source=c.source,
            source_id=f"{c.source_id}_pal",
            asset_type="video",
            category=c.category,
            download_url="",                      # synthetic
            license=c.license,
            author=c.author,
            width=720, height=576,
            duration_sec=c.duration_sec,
            local_path=c.pal_variant_path,
            approved=c.approved,                  # inherit
            notes=f"PAL variant of {c.name}",
        )
        new_siblings.append(sibling)
        existing.add(pal_name)
    if new_siblings:
        state.candidates.extend(new_siblings)
        print(f"[process] +{len(new_siblings)} PAL-variant siblings injected")


def _inject_gray_siblings(state: LotState) -> None:
    """For every color texture with a generated gray pair, ensure a sibling
    Candidate exists. Idempotent — won't duplicate on re-run.
    """
    existing = {c.name for c in state.candidates}
    new_siblings: List[Candidate] = []
    for c in state.candidates:
        if not c.gray_pair_path:
            continue
        if not Path(c.gray_pair_path).exists():
            continue
        gray_name = f"{c.name}_gray"
        if gray_name in existing:
            continue
        sibling = Candidate(
            name=gray_name,
            source=c.source,
            source_id=f"{c.source_id}_gray",
            asset_type=c.asset_type,
            category="gray_pair",
            download_url="",                      # synthetic — no upstream URL
            license=c.license,
            author=c.author,
            width=c.width,
            height=c.height,
            local_path=c.gray_pair_path,
            approved=c.approved,                  # inherit approval from color
            notes=f"gray pair of {c.name}",
        )
        new_siblings.append(sibling)
        existing.add(gray_name)
    if new_siblings:
        state.candidates.extend(new_siblings)
        print(f"[process] +{len(new_siblings)} gray-pair siblings injected")


def _process_image(cand: Candidate, src: Path, out_dir: Path, max_dim: int) -> None:
    # 1) Extract from zip if needed (ambientCG ships zips with multiple maps; we want the diffuse/color JPG).
    # Detect zip by magic bytes (PK\x03\x04) rather than extension — older runs of the
    # download stage saved zips as .bin because of a query-string parsing miss.
    img_path = src
    is_zip = src.suffix.lower() == ".zip"
    if not is_zip:
        try:
            with open(src, "rb") as f:
                magic = f.read(4)
            if magic[:2] == b"PK":
                is_zip = True
        except Exception:
            pass
    if is_zip:
        try:
            with zipfile.ZipFile(src) as z:
                # Prefer Color/Diffuse/Albedo JPG.
                names = z.namelist()
                target = next((n for n in names if "_Color" in n and n.lower().endswith(".jpg")), None)
                if not target:
                    target = next((n for n in names if "Diffuse" in n and n.lower().endswith(".jpg")), None)
                if not target:
                    target = next((n for n in names if n.lower().endswith(".jpg")), None)
                if not target:
                    cand.notes = (cand.notes + "; no jpg in zip").strip("; ")
                    return
                extract_dir = src.parent / src.stem
                extract_dir.mkdir(exist_ok=True)
                z.extract(target, extract_dir)
                img_path = extract_dir / target
        except Exception as e:
            cand.notes = (cand.notes + f"; zip-extract-failed: {e}").strip("; ")
            return

    # Masks: keep as PNG (preserves luminance precision, no JPG ringing).
    # If the source is already a synthetic-generated PNG at the target size,
    # leave local_path unchanged — nothing to do.
    if cand.asset_type == "mask":
        try:
            im = Image.open(img_path).convert("L")  # single channel for masks
        except Exception as e:
            cand.notes = (cand.notes + f"; pil-open-failed: {e}").strip("; ")
            return
        w, h = im.size
        scale = min(1.0, max_dim / max(w, h))
        if scale < 1.0:
            im = im.resize((int(w * scale), int(h * scale)), Image.LANCZOS)
        out_path = out_dir / f"{cand.name}.png"
        # Re-save through the processed/ pipeline so subsequent stages all
        # find their inputs in one place.
        im.save(out_path, "PNG", optimize=True)
        cand.local_path = str(out_path)
        cand.width, cand.height = im.size
        # Masks don't get a gray pair — they ARE the luminance.
        return

    try:
        im = Image.open(img_path).convert("RGB")
    except Exception as e:
        cand.notes = (cand.notes + f"; pil-open-failed: {e}").strip("; ")
        return

    # 2) Resize: cap max(width,height) at max_dim while keeping aspect.
    w, h = im.size
    scale = min(1.0, max_dim / max(w, h))
    if scale < 1.0:
        im = im.resize((int(w * scale), int(h * scale)), Image.LANCZOS)

    # 3) Save color JPG (q90).
    color_path = out_dir / f"{cand.name}.jpg"
    im.save(color_path, "JPEG", quality=90, optimize=True)
    cand.local_path = str(color_path)
    cand.width, cand.height = im.size

    # 4) Auto-generate gray pair (Fanions pattern: one image, one desaturated copy).
    # Don't generate on gray-pair siblings themselves (avoid `<name>_gray_gray` chain).
    if cand.asset_type == "texture" and cand.category != "gray_pair":
        gray = im.convert("L").convert("RGB")
        gray_path = out_dir / f"{cand.name}_gray.jpg"
        gray.save(gray_path, "JPEG", quality=90, optimize=True)
        cand.gray_pair_path = str(gray_path)


def _process_video(cand: Candidate, src: Path, out_dir: Path) -> None:
    """Re-encode any input to Theora .ogv at 1280x720 + 720x576 PAL.

    Requires ffmpeg with libtheora + libvorbis support on PATH.
    """
    if not _has_ffmpeg():
        print(f"[process/video] ffmpeg not on PATH — skipping {cand.slug()}", file=sys.stderr)
        return

    targets = [(1280, 720, "_720p"), (720, 576, "_pal")]
    canonical_path = ""
    pal_path = ""
    for w, h, suffix in targets:
        out = out_dir / f"{cand.name}{suffix}.ogv"
        if not out.exists():
            cmd = [
                "ffmpeg", "-y",
                "-i", str(src),
                "-vf", f"scale={w}:{h}:force_original_aspect_ratio=decrease,pad={w}:{h}:(ow-iw)/2:(oh-ih)/2",
                "-c:v", "libtheora", "-q:v", "8",
                "-c:a", "libvorbis", "-q:a", "5",
                str(out),
            ]
            try:
                subprocess.run(cmd, check=True, capture_output=True)
            except subprocess.CalledProcessError as e:
                cand.notes = (cand.notes + f"; ffmpeg-failed-{w}x{h}").strip("; ")
                print(f"[process/video] ffmpeg failed for {cand.slug()} {w}x{h}: {e.stderr.decode(errors='ignore')[:200]}", file=sys.stderr)
                continue
        if suffix == "_720p":
            canonical_path = str(out)
        elif suffix == "_pal":
            pal_path = str(out)

    # Canonical = 720p (preferred for modern VJ); PAL goes to a sibling Candidate
    # via _inject_pal_siblings after the process loop.
    cand.local_path        = canonical_path or pal_path  # graceful if 720p failed
    cand.pal_variant_path  = pal_path
    cand.width, cand.height = 1280, 720


def _has_ffmpeg() -> bool:
    return shutil.which("ffmpeg") is not None


# ───────────────────────────────────────────────────────────────────────────
# Stage 4 — REVIEW (local HTTP thumbs)
# ───────────────────────────────────────────────────────────────────────────

def stage_review(lot: str, port: int) -> None:
    """Spawn a local HTTP server to manually approve/reject candidates."""
    import http.server
    import socketserver
    state_path = lot_state_path(lot)
    state = LotState.load(state_path, lot)

    def render_html() -> bytes:
        rows = []
        for i, c in enumerate(state.candidates):
            status = ("approved" if c.approved is True
                      else "rejected" if c.approved is False
                      else "pending")
            preview = ""
            if c.local_path and Path(c.local_path).exists():
                rel = Path(c.local_path).resolve()
                preview = (f'<img src="/file?p={urllib.parse.quote(str(rel))}" '
                           f'style="width:200px;height:200px;object-fit:cover">')
            rows.append(f"""
<tr class="{status}">
  <td>{i}</td>
  <td>{preview}</td>
  <td><b>{c.name}</b><br>{c.source}/{c.source_id}<br>{c.category} — {c.asset_type}<br>{c.license}</td>
  <td>
    <button onclick="vote({i},true)">[OK]</button>
    <button onclick="vote({i},false)">[NO]</button>
    <span class="status">{status}</span>
  </td>
</tr>""")
        body = f"""<!doctype html><html><head><meta charset=utf-8><style>
body{{font-family:monospace;background:#222;color:#ddd}}
table{{border-collapse:collapse}}
td{{padding:8px;border:1px solid #444;vertical-align:top}}
.approved{{background:#143}}.rejected{{background:#411}}.pending{{}}
button{{margin:2px}}
</style></head><body>
<h1>BBFx asset review — Lot {lot}</h1>
<p>Total: {len(state.candidates)} | Approved: {sum(1 for c in state.candidates if c.approved is True)} |
   Rejected: {sum(1 for c in state.candidates if c.approved is False)} |
   Pending: {sum(1 for c in state.candidates if c.approved is None)}</p>
<table>{''.join(rows)}</table>
<script>
async function vote(i, ok){{
  await fetch('/vote?i='+i+'&v='+ok, {{method:'POST'}});
  location.reload();
}}
</script></body></html>"""
        return body.encode("utf-8")

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a, **k): pass

        def _send(self, code, body, ctype="text/html; charset=utf-8"):
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            u = urllib.parse.urlparse(self.path)
            if u.path == "/":
                return self._send(200, render_html())
            if u.path == "/file":
                qs = urllib.parse.parse_qs(u.query)
                p = Path(qs.get("p", [""])[0])
                if not p.exists():
                    return self._send(404, b"not found")
                ctype = "image/jpeg" if p.suffix.lower() in (".jpg", ".jpeg") else "image/png"
                return self._send(200, p.read_bytes(), ctype)
            return self._send(404, b"not found")

        def do_POST(self):
            u = urllib.parse.urlparse(self.path)
            if u.path == "/vote":
                qs = urllib.parse.parse_qs(u.query)
                i = int(qs["i"][0]); v = qs["v"][0] == "true"
                state.candidates[i].approved = v
                state.save(state_path)
                return self._send(200, b"ok", "text/plain")
            return self._send(404, b"not found")

    print(f"[review] Open http://localhost:{port}/  (Ctrl-C to stop and persist)")
    with socketserver.TCPServer(("127.0.0.1", port), Handler) as srv:
        try:
            srv.serve_forever()
        except KeyboardInterrupt:
            pass
    print(f"[review] Final state saved to {state_path}")


# ───────────────────────────────────────────────────────────────────────────
# Stage 5 — HASH
# ───────────────────────────────────────────────────────────────────────────

def stage_hash(lot: str) -> None:
    state = LotState.load(lot_state_path(lot), lot)
    for cand in tqdm(state.candidates, desc=f"hash lot {lot}"):
        if not cand.local_path or not Path(cand.local_path).exists():
            continue
        if cand.sha256:
            continue
        h = hashlib.sha256()
        size = 0
        with open(cand.local_path, "rb") as f:
            for chunk in iter(lambda: f.read(65536), b""):
                h.update(chunk)
                size += len(chunk)
        cand.sha256 = h.hexdigest()
        cand.size_bytes = size
    state.save(lot_state_path(lot))


# ───────────────────────────────────────────────────────────────────────────
# Stage 6 — UPLOAD (S3-compatible: Cloudflare R2 / Backblaze B2 / AWS)
# ───────────────────────────────────────────────────────────────────────────

def stage_upload(lot: str, bucket: str, endpoint: str, public_base: str) -> None:
    global boto3
    boto3 = _require("boto3", "Run: pip install -r tools/requirements.txt")

    # Validate inputs: better a precise error here than a cryptic boto3 traceback.
    missing = [name for name, val in [
        ("--bucket / R2_BUCKET",            bucket),
        ("--endpoint / R2_ENDPOINT",        endpoint),
        ("--public-base / R2_PUBLIC_BASE",  public_base),
    ] if not val]
    if missing:
        print(f"ERROR: missing required value(s): {', '.join(missing)}", file=sys.stderr)
        print("Set them in tools/.env (or bbfx-revival/.env) or pass on the command line.", file=sys.stderr)
        sys.exit(2)

    access_key = os.environ.get("R2_ACCESS_KEY_ID")     or os.environ.get("AWS_ACCESS_KEY_ID")
    secret_key = os.environ.get("R2_SECRET_ACCESS_KEY") or os.environ.get("AWS_SECRET_ACCESS_KEY")
    if not access_key or not secret_key:
        print("ERROR: R2_ACCESS_KEY_ID / R2_SECRET_ACCESS_KEY not set "
              "(or AWS_ACCESS_KEY_ID / AWS_SECRET_ACCESS_KEY).", file=sys.stderr)
        print("Set them in tools/.env (or bbfx-revival/.env).", file=sys.stderr)
        sys.exit(2)

    state = LotState.load(lot_state_path(lot), lot)

    s3 = boto3.client(
        "s3",
        endpoint_url=endpoint,
        aws_access_key_id=access_key,
        aws_secret_access_key=secret_key,
        region_name="auto",
    )

    for cand in tqdm(state.candidates, desc=f"upload lot {lot}"):
        if cand.approved is not True:
            continue
        if cand.cdn_url or not cand.local_path:
            continue
        key = f"bbfx/v1/{cand.sha256[:2]}/{cand.sha256}"
        try:
            s3.upload_file(cand.local_path, bucket, key)
            cand.cdn_url = f"{public_base.rstrip('/')}/{key}"
        except Exception as e:
            print(f"[upload] {cand.slug()}: {e}", file=sys.stderr)
            continue
    state.save(lot_state_path(lot))


# ───────────────────────────────────────────────────────────────────────────
# Stage 7 — MANIFEST (write Lua tables)
# ───────────────────────────────────────────────────────────────────────────

def stage_manifest(lot: str, allow_unuploaded: bool = False) -> None:
    state = LotState.load(lot_state_path(lot), lot)
    cfg = LOT_CONFIG[lot]
    approved = [c for c in state.candidates if c.approved is True and c.sha256]
    if not approved:
        print(f"[manifest] No approved+hashed candidates for lot {lot} — nothing to write.", file=sys.stderr)
        return

    # Refuse to write a manifest with broken source_urls (the most common pitfall:
    # running `manifest` before `upload`). Color entries would fall back to the
    # upstream URL (often a .zip wrapper for ambientCG, useless at runtime) and
    # gray-pair siblings have no upstream URL at all.
    not_uploaded = [c for c in approved if not c.cdn_url]
    if not_uploaded and not allow_unuploaded:
        no_upstream = [c for c in not_uploaded if not c.download_url]
        wrong_ext = [c for c in not_uploaded
                     if c.download_url and not c.download_url.lower().endswith(
                         (".jpg", ".jpeg", ".png", ".ogv", ".mp4", ".webm"))]
        print(f"[manifest] BLOCKED — {len(not_uploaded)}/{len(approved)} approved candidates have no `cdn_url`.", file=sys.stderr)
        if no_upstream:
            print(f"           {len(no_upstream)} synthetic entries (e.g. gray pairs) have no upstream URL — runtime cannot resolve them.", file=sys.stderr)
        if wrong_ext:
            print(f"           {len(wrong_ext)} entries point to wrappers (e.g. .zip from ambientCG) instead of the extracted asset.", file=sys.stderr)
        print("           Run `python tools/asset_pipeline.py upload --lot {lot}` first, then re-run manifest.".format(lot=lot), file=sys.stderr)
        print("           Override (not recommended): pass --allow-unuploaded.", file=sys.stderr)
        sys.exit(2)

    out_path = MANIFEST_DIR / cfg["manifest_out"]
    MANIFEST_DIR.mkdir(parents=True, exist_ok=True)

    lines: List[str] = []
    lines.append(f"-- {cfg['manifest_out']}")
    lines.append(f"-- BBFx asset manifest, lot {lot}.")
    lines.append(f"-- Auto-generated by tools/asset_pipeline.py — do not edit by hand.")
    lines.append(f"-- Each entry verified by SHA-256 against the upstream/CDN binary.")
    lines.append("")
    lines.append("return {")

    for c in approved:
        lines.append(f'  ["{c.name}"] = {{')
        lines.append(f'    file        = "{Path(c.local_path).name}",')
        lines.append(f'    hash_sha256 = "{c.sha256}",')
        lines.append(f'    size_bytes  = {c.size_bytes},')
        lines.append(f'    type        = "{c.asset_type}",')
        lines.append(f'    category    = "{c.category}",')
        lines.append(f'    license     = "{c.license}",')
        if c.author:
            lines.append(f'    author      = "{_lua_escape(c.author)}",')
        if c.cdn_url:
            lines.append(f'    source_url  = "{c.cdn_url}",')
        else:
            lines.append(f'    source_url  = "{c.download_url}",')
        if c.gray_pair_path:
            # Cross-reference the gray sibling by its logical manifest name
            # (resolvable via bbfx.assets.resolve("<name>_gray")), not by filename.
            lines.append(f'    gray_pair   = "{c.name}_gray",')
        if c.pal_variant_path:
            lines.append(f'    pal_variant = "{c.name}_pal",')
        if c.duration_sec:
            lines.append(f'    duration_sec = {c.duration_sec:.2f},')
        lines.append(f'    width       = {c.width},')
        lines.append(f'    height      = {c.height},')
        lines.append(f'  }},')

    lines.append("}")
    lines.append("")
    out_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"[manifest] wrote {out_path} ({len(approved)} entries)")

    _write_attribution_doc(state)


def _lua_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


def _write_attribution_doc(state: LotState) -> None:
    """Append CC-BY/Pexels attributions to docs/THIRD_PARTY_ASSETS.md."""
    THIRDPARTY_DOC.parent.mkdir(parents=True, exist_ok=True)
    header = "# Third-party assets — attribution\n\n"
    if not THIRDPARTY_DOC.exists():
        THIRDPARTY_DOC.write_text(header, encoding="utf-8")

    existing = THIRDPARTY_DOC.read_text(encoding="utf-8")
    section_marker = f"## Lot {state.lot}\n"
    if section_marker in existing:
        # Replace existing section.
        before, _, after = existing.partition(section_marker)
        next_marker_idx = after.find("\n## ")
        after = after[next_marker_idx:] if next_marker_idx >= 0 else ""
        existing = before
    else:
        after = ""

    body = [section_marker]
    body.append(f"_Auto-generated {time.strftime('%Y-%m-%d')} by tools/asset_pipeline.py._\n\n")
    body.append("| Asset | Source | License | Author / Attribution |\n")
    body.append("|-------|--------|---------|----------------------|\n")
    for c in state.candidates:
        if c.approved is not True or c.license == "CC0":
            continue
        body.append(f"| `{c.name}` | [{c.source}](https://{c.source}.com/) | {c.license} | {c.author or '—'} |\n")
    body.append("\n")

    THIRDPARTY_DOC.write_text(existing + "".join(body) + after, encoding="utf-8")
    print(f"[manifest] attribution doc updated: {THIRDPARTY_DOC}")


# ───────────────────────────────────────────────────────────────────────────
# CLI
# ───────────────────────────────────────────────────────────────────────────

def main() -> None:
    p = argparse.ArgumentParser(description="BBFx asset pipeline (Phase 1)")
    sub = p.add_subparsers(dest="cmd", required=True)

    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--lot", required=True, choices=sorted(LOT_CONFIG.keys()))

    sd = sub.add_parser("discover", parents=[common])
    sd.add_argument("--limit", type=int, default=20)
    sd.add_argument("--sources", default="ambientcg,polyhaven,pexels,pixabay",
                    help="Comma-separated: ambientcg, polyhaven, pexels, pixabay")
    sd.add_argument("--pexels-key", default=os.environ.get("PEXELS_API_KEY"))

    sub.add_parser("download", parents=[common])

    sp = sub.add_parser("process", parents=[common])
    sp.add_argument("--max-dim", type=int, default=1024)

    sr = sub.add_parser("review", parents=[common])
    sr.add_argument("--port", type=int, default=8765)

    sub.add_parser("hash", parents=[common])

    su = sub.add_parser("upload", parents=[common])
    # All three can be set via .env (R2_BUCKET / R2_ENDPOINT / R2_PUBLIC_BASE).
    # If absent from both .env and the CLI, argparse will error out with a clear message.
    su.add_argument("--bucket", default=os.environ.get("R2_BUCKET"),
                    help="Bucket name. Default: $R2_BUCKET from .env")
    su.add_argument("--endpoint", default=os.environ.get("R2_ENDPOINT"),
                    help="S3-compatible endpoint, e.g. https://<acct>.r2.cloudflarestorage.com. "
                         "Default: $R2_ENDPOINT from .env")
    su.add_argument("--public-base", default=os.environ.get("R2_PUBLIC_BASE"),
                    help="Public CDN base URL, e.g. https://pub-xxx.r2.dev. "
                         "Default: $R2_PUBLIC_BASE from .env")

    sm = sub.add_parser("manifest", parents=[common])
    sm.add_argument("--allow-unuploaded", action="store_true",
                    help="Write manifest even if some approved entries lack a cdn_url. "
                         "DANGEROUS: gray pairs and ambientCG wrappers will be unresolvable at runtime.")

    sa = sub.add_parser("run-all", parents=[common])
    sa.add_argument("--skip", default="upload,manifest",
                    help="Comma-separated stages to skip when running everything")
    sa.add_argument("--limit", type=int, default=20)
    sa.add_argument("--sources", default="ambientcg,polyhaven,pexels,pixabay")
    sa.add_argument("--pexels-key", default=os.environ.get("PEXELS_API_KEY"))

    args = p.parse_args()
    WORK_DIR.mkdir(parents=True, exist_ok=True)

    if args.cmd == "discover":
        stage_discover(args.lot, args.limit, [s.strip() for s in args.sources.split(",")], args.pexels_key)
    elif args.cmd == "download":
        stage_download(args.lot)
    elif args.cmd == "process":
        stage_process(args.lot, args.max_dim)
    elif args.cmd == "review":
        stage_review(args.lot, args.port)
    elif args.cmd == "hash":
        stage_hash(args.lot)
    elif args.cmd == "upload":
        stage_upload(args.lot, args.bucket, args.endpoint, args.public_base)
    elif args.cmd == "manifest":
        stage_manifest(args.lot, allow_unuploaded=args.allow_unuploaded)
    elif args.cmd == "run-all":
        skip = {s.strip() for s in args.skip.split(",") if s.strip()}
        if "discover" not in skip:
            stage_discover(args.lot, args.limit, [s.strip() for s in args.sources.split(",")], args.pexels_key)
        if "download" not in skip: stage_download(args.lot)
        if "process"  not in skip: stage_process(args.lot, 1024)
        if "review"   not in skip: stage_review(args.lot, 8765)
        if "hash"     not in skip: stage_hash(args.lot)
        # upload + manifest skipped by default in run-all (require credentials).


if __name__ == "__main__":
    main()
