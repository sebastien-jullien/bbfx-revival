-- ─────────────────────────────────────────────────────────────────────
-- Preset: theora_on_geosphere
-- Demo end-to-end Sprint S5 (Lots T + V) :
--   TheoraClipNode (bombe.ogg) → MaterialBridgeNode → SceneObjectNode (Geosphere8000)
--
-- Mecanisme :
--   - TheoraClipNode expose mClip->getMaterialName() via mirror ParamSpec
--     `material_out` (Lot V).
--   - MaterialBridgeNode `material_source` port pulle ce mirror (Lot T).
--   - L'entity port lie la SceneObjectNode -> material applique sur sub-entity.
--
-- Demontre la video-sur-mesh-3D via DAG pure, sans wiring Lua hors-DAG.
-- ─────────────────────────────────────────────────────────────────────

local ParamSpec = require "paramspec"

return {
    name = "theora_on_geosphere",
    version = 1,
    category = "VJ",
    description = "TheoraClipNode -> MaterialBridgeNode -> Geosphere : video bombe.ogg appliquee sur sphere 3D via DAG pure (Sprint S5 demo)",
    tags = {"vj", "video", "mesh", "demo", "lot_v", "material_bridge", "theora"},
    params = ParamSpec.declare({
        ParamSpec.enum("lighting_mode", "unlit", {"unlit", "lit", "emissive"},
                       {label="Lighting"}),
    }),
    build = function(params)
        local lighting = params.lighting_mode or "unlit"
        return {
            type = "CompositionNode",
            primary = "geo",
            nodes = {
                {name="cam",   type="CameraNode"},
                {name="light", type="LightNode"},
                {name="geo",   type="SceneObjectNode",
                  params={mesh="Geosphere8000.mesh"}},
                {name="clip",  type="TheoraClipNode"},
                {name="mb",    type="MaterialBridgeNode",
                  params={lighting_mode=lighting}},
            },
            links = {
                -- Geosphere -> MaterialBridge (entity link)
                {from="geo",  fromPort="entity",         to="mb", toPort="entity"},
                -- TheoraClip -> MaterialBridge (material_source pulls `material_out` mirror)
                -- Any output port works here; getSourceNodes follows the link to find the upstream node.
                {from="clip", fromPort="material_ready", to="mb", toPort="material_source"},
            },
            params = params,
        }
    end
}
