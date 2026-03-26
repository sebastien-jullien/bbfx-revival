-- effect.lua — BBFx v2.3 Scene Effects
-- Ported from 2006 production code (prog/workspace/bbfx/bbfx/src/lua/0.1/effect.lua)

Effect = {}

function Effect.skybox(enabled, material)
    material = material or "Examples/SceneSkyBox1"
    scene:setSkyBox(enabled, material)
end

function Effect.sky(enabled, skyName, curvature)
    curvature = curvature or 5
    skyName = skyName or "CloudySky"
    Ogre.setSkyDome(scene, enabled, "Examples/" .. tostring(skyName), curvature)
end

function Effect.fog(intensity, colour, fogType)
    fogType = fogType or 1 -- FOG_EXP
    colour = colour or Ogre.ColourValue(1, 1, 1)
    intensity = intensity or 0
    scene:setFog(fogType, colour, intensity, 0, 1)
end

function Effect.bg(colour)
    colour = colour or Ogre.ColourValue(0, 0, 0)
    local vp = engine:getRenderWindow():getViewport(0)
    vp:setBackgroundColour(colour)
end

function Effect.shadows(technique)
    technique = technique or 0 -- SHADOWTYPE_NONE
    Ogre.setShadowTechnique(scene, technique)
end

function Effect.ambient(r, g, b)
    scene:setAmbientLight(Ogre.ColourValue(r or 0, g or 0, b or 0))
end

function Effect.clear()
    Ogre.clearScene(scene)
end
