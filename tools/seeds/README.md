# Asset pipeline seeds

Pre-curated candidate lists, merged into the pipeline state at the `discover` stage.

## `lot_e.json` — Heritage Pack 30 textures

20 curated candidates (12 ambientCG + 8 Polyhaven, all CC0). Run
`asset_pipeline.py discover --lot E` to grow the pool to 30+ via live API queries.

## `lot_j.json` — VJ Loops Pack 10 videos

Empty by default. **Video sources require an API key** because there is no stable
public direct-download URL for CC0/Pexels-license clips:

- **Pexels**: free key at https://www.pexels.com/api/ — set `PEXELS_API_KEY=...`
- **Pixabay**: free key at https://pixabay.com/api/docs/ — set `PIXABAY_API_KEY=...`

Then run:

```sh
python asset_pipeline.py discover --lot J --sources pexels,pixabay --limit 30
```

Alternatively, hand-curate by adding entries to `lot_j.json` matching the `Candidate` schema
(see `asset_pipeline.py:Candidate`). The pipeline will pick them up on the next `discover`.

## Manual curation format

Each entry is a JSON object with these fields (everything past `license` is optional and gets filled by later stages):

```json
{
  "name": "logical_unique_name",
  "source": "ambientcg | polyhaven | pexels | pixabay | manual",
  "source_id": "upstream_id",
  "asset_type": "texture | mask | video",
  "category": "organic | cosmic | geometric | mask | abstract | particles | glitch | slowmo",
  "download_url": "https://...",
  "license": "CC0 | CC-BY | Pexels | Pixabay",
  "author": "Required for CC-BY",
  "width": 1024,
  "height": 1024
}
```
