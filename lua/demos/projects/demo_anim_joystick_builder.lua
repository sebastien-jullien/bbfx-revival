-- demo_anim_joystick_builder.lua — BBFx v3.5.2 Lot AZ.2 (2026-06-01)
--
-- Showcase + test du pipeline d'animation de mesh (AnimationStateNode) **piloté
-- au joystick** : le stick gauche contrôle le SENS et la VITESSE de lecture de
-- l'animation squelettique.
--
-- Architecture :
--   - `SceneObjectNode "ninja"` (ninja.mesh — squelette OGRE classique, clip "Walk").
--   - `AnimationStateNode "anim"` joue "Walk", entity-link sur le ninja. Mode
--     auto-advance (le port `dt` est auto-lié à RootTime par la factory ; aucun
--     `time` lié) → la vitesse ET le sens viennent du port `speed` (négatif = arrière).
--   - `GamepadNode "gamepad"` → `leftStickY`.
--   - `MathNode "stick_mul"` : `leftStickY × -0.5`  (SDL : haut = -1 → +0.5 = avant rapide).
--   - `MathNode "speed_add"` : `+ 0.17` de base → au repos le ninja marche en avant
--     à ~0.17× ; stick HAUT = jusqu'à ~0.67× avant ; stick BAS = ralentit, s'arrête,
--     puis repart EN ARRIÈRE jusqu'à ~-0.33×. « dans un sens dans l'autre, plus ou moins vite ».
--     (Échelle ÷6 vs design initial : 1× au repos rendait le test trop rapide.)
--
-- Sans manette : le ninja marche en avant à ~0.17× (valeur de repos de la chaîne).
-- En live (console) : `dbg.set("anim","speed",-0.3)` → arrière ; `dbg.set("anim","speed",0.65)` → rapide ;
--                     `dbg.set("anim","speed",0)` → figé.
--
-- 120 BPM.

return {
    name = "Animation Joystick (mesh animé piloté manette)",
    bpm = 120,
    description = "Ninja animé (clip Walk) dont le SENS et la VITESSE de lecture sont pilotés au stick gauche "
               .. "(haut = avant rapide, bas = arrière, repos = marche avant 1×). Teste AnimationStateNode en auto-advance.",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(120) end

        dbg.create("SceneObjectNode",    "ninja")
        dbg.create("AnimationStateNode", "anim")
        dbg.create("GamepadNode",        "gamepad")
        dbg.create("MathNode",           "stick_mul")  -- leftStickY × -3
        dbg.create("MathNode",           "speed_add")  -- + 1.0 base
        dbg.create("LightNode",          "light_key")
        dbg.create("LightNode",          "light_fill")
        dbg.create("CameraNode",         "cam_orbit")
        _dbg_process_pending()

        -- Mesh riggé. ninja.mesh a son origine aux pieds (y=0) et ~100 u de haut ;
        -- on l'abaisse pour CENTRER le corps sur l'origine (la caméra orbit regarde
        -- (0,0,0)) → le ninja entier est cadré, plus de troncature en haut.
        dbg.set_param("ninja", "mesh_file", "ninja.mesh")
        dbg.set("ninja", "scale.x", 0.4); dbg.set("ninja","scale.y",0.4); dbg.set("ninja","scale.z",0.4)
        dbg.set("ninja", "position.y", -20.0)   -- pieds à -20, centre ~0, tête ~+20
        dbg.set("ninja", "rotation.y", 180.0)   -- ninja.mesh regarde +Z par défaut → 180° pour faire FACE à la caméra (sinon on voit son dos)

        -- AnimationStateNode : clip "Walk", loop. (Le port dt est auto-lié par la factory.)
        dbg.set_param("anim", "animation_name", "Walk")
        dbg.set_param("anim", "loop", "true")

        -- Chaîne stick → vitesse (échelle ÷6 vs design initial : 1× était trop rapide à tester).
        dbg.set("stick_mul", "operation", 2.0)   -- multiply
        dbg.set("stick_mul", "b", -0.5)          -- haut (-1) → +0.5 (avant rapide)
        dbg.set("speed_add", "operation", 0.0)   -- add
        dbg.set("speed_add", "b", 0.17)          -- base ~0.17× (marche avant très calme au repos)
        -- Repos = 0.17× ; stick HAUT = jusqu'à ~0.67× avant ; stick BAS = ralentit, s'arrête, repart en ARRIÈRE jusqu'à ~-0.33×.

        -- Caméra en orbite (regarde l'origine = le centre du ninja recentré) + lumières.
        dbg.set_param("cam_orbit", "mode", "orbit")
        dbg.set("cam_orbit", "orbit_radius", 80.0)
        dbg.set("cam_orbit", "orbit_speed",   0.12)
        dbg.set("cam_orbit", "orbit_height",  18.0)
        dbg.set("light_key",  "position.x", 100.0); dbg.set("light_key","position.y",120.0); dbg.set("light_key","position.z",100.0)
        dbg.set("light_fill", "position.x",-100.0); dbg.set("light_fill","position.y",-30.0); dbg.set("light_fill","position.z",-60.0)
        dbg.set("light_fill", "diffuse.r", 0.3); dbg.set("light_fill","diffuse.g",0.4); dbg.set("light_fill","diffuse.b",0.8)

        -- Links.
        dbg.link("time", "dt", "cam_orbit", "dt")
        dbg.link("ninja", "entity", "anim", "entity")        -- entity-link : ninja → anim
        dbg.link("gamepad", "leftStickY", "stick_mul", "a")  -- stick → ×-3
        dbg.link("stick_mul", "out", "speed_add", "a")       -- + base
        dbg.link("speed_add", "out", "anim", "speed")        -- → vitesse/sens de l'anim
    end
}
