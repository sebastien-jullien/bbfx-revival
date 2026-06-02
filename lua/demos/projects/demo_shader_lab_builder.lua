-- demo_shader_lab_builder.lua — BBFx v3.5.2 Sprint S8 Lot AU (I-2010, fix Lot AU.1 2026-05-12)
--
-- Showcase : déformateurs vertex + bruit volumétrique + ColorShift.
--   - WaveVertexShader sur une sphere (onde forte) + un knot (onde fine)
--   - PerlinFxNode sur un cube + une column (bruit animé)
--   - ColorShiftNode hue cycle (sphere + knot)
--   - CameraNode en mode crane
--
-- NB : les NoiseTextureNode/GrayscaleNode (RTT procéduraux) ont été retirés de
-- cette démo — sur reload du `.bbfx-project` ils tentaient un `getBuffer` sur
-- une texture source/RTT pas encore chargée → assertion OgreTexture.cpp:415.
-- Ils restent disponibles dans le Studio (Asset Browser / création manuelle).
--
-- Layout : caméra MOBILE (crane) · pas de texture/vidéo caméra · 100 BPM.

return {
    name = "Shader Lab",
    bpm = 100,
    description = "WaveVertexShader (sphere + knot) + PerlinFx (cube + column) + ColorShift hue cycle — caméra crane.",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(100) end

        dbg.create("SceneObjectNode",  "sphere")
        dbg.create("SceneObjectNode",  "cube")
        dbg.create("SceneObjectNode",  "knot")
        dbg.create("SceneObjectNode",  "column")
        dbg.create("WaveVertexShader", "wave_sphere")
        dbg.create("WaveVertexShader", "wave_knot")
        dbg.create("PerlinFxNode",     "perlin_cube")
        dbg.create("PerlinFxNode",     "perlin_col")
        dbg.create("ColorShiftNode",   "hue_sphere")
        dbg.create("ColorShiftNode",   "hue_knot")
        dbg.create("MathNode",         "hue_anim")
        dbg.create("CameraNode",       "cam")
        dbg.create("LightNode",        "light_key")
        dbg.create("LightNode",        "light_fill")
        _dbg_process_pending()

        dbg.set_param("sphere", "mesh_file", "sphere.mesh"); dbg.set_param("sphere","material","Examples/Chrome")
        dbg.set("sphere","position.y",30.0); dbg.set("sphere","scale.x",0.4); dbg.set("sphere","scale.y",0.4); dbg.set("sphere","scale.z",0.4)
        dbg.set_param("cube",  "mesh_file", "cube.mesh"); dbg.set_param("cube","material","Examples/EnvMappedRustySteel")
        dbg.set("cube","position.x",-90.0); dbg.set("cube","position.z",-30.0); dbg.set("cube","scale.x",0.4); dbg.set("cube","scale.y",0.4); dbg.set("cube","scale.z",0.4)
        dbg.set_param("knot",  "mesh_file", "knot.mesh"); dbg.set_param("knot","material","Examples/Chrome")
        dbg.set("knot","position.x",90.0); dbg.set("knot","position.z",-30.0)
        dbg.set_param("column","mesh_file", "column.mesh"); dbg.set_param("column","material","Examples/EnvMappedRustySteel")
        dbg.set("column","position.z",95.0); dbg.set("column","scale.x",0.3); dbg.set("column","scale.y",0.3); dbg.set("column","scale.z",0.3)

        dbg.set("wave_sphere","amplitude",10.0); dbg.set("wave_sphere","frequency",4.0); dbg.set("wave_sphere","speed",2.0)
        dbg.set("wave_knot",  "amplitude",4.0);  dbg.set("wave_knot","frequency",6.0);  dbg.set("wave_knot","speed",3.0)
        dbg.set("perlin_cube","density",4.0); dbg.set("perlin_cube","displacement",0.3); dbg.set("perlin_cube","timeDensity",5.0)
        dbg.set("perlin_col", "density",3.0); dbg.set("perlin_col","displacement",0.2); dbg.set("perlin_col","timeDensity",4.0)

        dbg.set("hue_sphere","brightness",1.3); dbg.set("hue_sphere","saturation",1.0)
        dbg.set("hue_knot",  "brightness",1.2); dbg.set("hue_knot","saturation",1.0)
        dbg.set("hue_anim","operation",2.0); dbg.set("hue_anim","b",180.0)

        dbg.set_param("cam", "mode", "crane")
        dbg.set("cam","crane_amplitude",60.0); dbg.set("cam","crane_speed",0.2)
        dbg.set("light_key","position.x",80.0); dbg.set("light_key","position.y",110.0); dbg.set("light_key","position.z",80.0)
        dbg.set("light_fill","position.x",-80.0); dbg.set("light_fill","position.y",-20.0); dbg.set("light_fill","position.z",-50.0)
        dbg.set("light_fill","diffuse.r",0.35); dbg.set("light_fill","diffuse.g",0.4); dbg.set("light_fill","diffuse.b",0.8)

        -- Links
        dbg.link("time","dt","wave_sphere","dt")
        dbg.link("time","dt","wave_knot","dt")
        dbg.link("time","dt","perlin_cube","dt")
        dbg.link("time","dt","perlin_col","dt")
        dbg.link("time","dt","cam","dt")

        dbg.link("sphere","entity","wave_sphere","entity")
        dbg.link("sphere","entity","hue_sphere","entity")
        dbg.link("cube","entity","perlin_cube","entity")
        dbg.link("knot","entity","wave_knot","entity")
        dbg.link("knot","entity","hue_knot","entity")
        dbg.link("column","entity","perlin_col","entity")

        dbg.link("time","beatFrac","hue_anim","a")
        dbg.link("hue_anim","out","hue_sphere","hue_shift")
        dbg.link("hue_anim","out","hue_knot","hue_shift")

        dbg.link("time","beat","knot","rotation.y")
        dbg.link("time","beat","column","rotation.y")
    end
}
