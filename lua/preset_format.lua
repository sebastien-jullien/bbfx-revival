-- preset_format.lua — BBFx Preset Format v2
-- Presets use this format:
--   return {
--       name = "preset_name",
--       version = 2,
--       category = "Geometry",  -- Geometry, Color, PostProcess, Particle, Camera, Composition
--       description = "Short description",
--       tags = {"tag1", "tag2"},
--       params = ParamSpec.declare({...}),  -- optional, from paramspec.lua
--       build = function(params)            -- creates nodes, returns root node or subgraph
--           ...
--       end
--   }
--
-- v1 presets (simple table with nodes array) are still supported for backward compatibility.
-- The PresetBrowserPanel detects the format via the presence of "version" field.

local PresetFormat = {}
PresetFormat.VERSION = 2
return PresetFormat
