-- demo_projection_mapping_builder.lua — BBFx v3.5.2 Sprint S8 Lot AU (I-2012)
--
-- Showcase : scène pensée pour le multi-output / projection mapping. La config
-- des sorties (outputs, surface zones, warp, blend gamma) est portée par le
-- `.bbfx-project` lui-même (sections `outputs` + `extra.surfaceMap`) — au moment
-- du bake elle reflète l'état courant de l'OutputManager. La scène est volontairement
-- lisible (grille de cubes + un mesh central) pour que le warp/blend soit visible
-- sur les bords des zones.
--
-- Layout : caméra FIXE · pas de texture/vidéo caméra · 120 BPM · 2 outputs / 2 zones.

return {
    name = "Projection Mapping",
    bpm = 120,
    description = "Grille de cubes + mesh central — scène lisible pour démontrer le multi-output, les surface zones et le warp/blend (config portée par le .bbfx-project).",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(120) end

        dbg.create("SceneObjectNode", "center")
        dbg.create("LightNode",       "light")
        dbg.create("PerlinFxNode",    "perlin")
        dbg.create("ColorShiftNode",  "hue")
        dbg.create("MathNode",        "hue_anim")
        _dbg_process_pending()

        dbg.set_param("center", "mesh_file", "geosphere4500.mesh")
        dbg.set_param("center", "material",  "BaseWhiteNoLighting")
        dbg.set("center", "position.y", 10.0)
        dbg.set("center", "scale.x", 0.06); dbg.set("center","scale.y",0.06); dbg.set("center","scale.z",0.06)

        -- A 3×3 grid of small cubes around the center — visible reference for
        -- warp/blend on the zone edges.
        local idx = 0
        for gx = -1, 1 do
            for gz = -1, 1 do
                idx = idx + 1
                local cname = "cube_" .. idx
                dbg.create("SceneObjectNode", cname)
            end
        end
        _dbg_process_pending()
        idx = 0
        for gx = -1, 1 do
            for gz = -1, 1 do
                idx = idx + 1
                local cname = "cube_" .. idx
                dbg.set_param(cname, "mesh_file", "cube.mesh")
                dbg.set_param(cname, "material",  "BaseWhite")
                dbg.set(cname, "position.x", gx * 90.0)
                dbg.set(cname, "position.y", 0.0)
                dbg.set(cname, "position.z", gz * 90.0)
                dbg.set(cname, "scale.x", 0.4); dbg.set(cname,"scale.y",0.4); dbg.set(cname,"scale.z",0.4)
            end
        end

        dbg.set("perlin","density",4.0); dbg.set("perlin","displacement",0.2); dbg.set("perlin","timeDensity",5.0)
        dbg.set("hue","brightness",1.2); dbg.set("hue","saturation",1.0)
        dbg.set("hue_anim","operation",2.0); dbg.set("hue_anim","b",180.0)
        dbg.set("light","position.y",150.0)

        dbg.link("time","dt","perlin","dt")
        dbg.link("center","entity","perlin","entity")
        dbg.link("center","entity","hue","entity")
        dbg.link("time","beatFrac","hue_anim","a")
        dbg.link("hue_anim","out","hue","hue_shift")

        -- Slow rotation on the center mesh and the corner cubes.
        dbg.link("time","beat","center","rotation.y")
    end
}
