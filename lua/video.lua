-- video.lua — BBFx v2.4 Video Management
-- Ported from 2006 production code (prog/workspace/bbfx/bbfx/src/lua/0.1/video.lua)

require 'helpers'
require 'object'

Video = {
    clips = {},
    materials = {}
}

-- Create a video clip from an .ogg file
function Video:createClip(filename)
    local clip = bbfx.TheoraClip(filename)
    self.clips[filename] = clip
    self.materials[filename] = clip:getMaterialName()
    print("[video] Created clip: " .. filename .. " (" .. clip:getWidth() .. "x" .. clip:getHeight() .. ")")
    return clip
end

-- Display a clip on a billboard overlay in front of the camera
function Video:overlay(clip, camera, width, height)
    width = width or 20
    height = height or 15
    local obj = Object:fromBillboard(clip:getMaterialName(), width, height)
    obj:translateZ(-50) -- Position in front of camera
    return obj
end

-- Prepare a crossfade between two clips (or textures)
function Video:crossfade(tex1Name, tex2Name)
    local matName = UID("crossfade/")
    local fader = bbfx.TextureCrossfader(matName, tex1Name, tex2Name)
    return fader
end

-- Get a clip by filename
function Video:getClip(filename)
    return self.clips[filename]
end

-- Get the material name for a clip
function Video:getMaterial(filename)
    return self.materials[filename]
end
