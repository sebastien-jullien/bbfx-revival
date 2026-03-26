-- note.lua — BBFx v2.3 Polymorphic Note Dispatch
-- Ported from 2006 production code (prog/workspace/bbfx/bbfx/src/lua/0.1/note.lua)
-- Lua 5.4 compatible (table.unpack, {...} for varargs)

require 'helpers'

Note = {
    map = {}
}
Note.__index = Note

function Note:new()
    local o = {}
    setmetatable(o, self)
    return o
end

function Note:delete()
    self.map[self] = nil
end

function Note:on() end
function Note:off() end

-- Note.Animation: enable/disable an animation
Note.Animation = {}
Note.Animation.__index = Note.Animation
setmetatable(Note.Animation, Note)

function Note.Animation:on(speed)
    if self.anim then self.anim:enable(speed) end
end

function Note.Animation:off()
    if self.anim then self.anim:disable() end
end

function Note:fromAnimation(anim)
    local note = Note:new()
    setmetatable(note, Note.Animation)
    note.anim = anim
    return note
end

-- Note.Object: attach/detach scene objects
Note.Object = {}
Note.Object.__index = Note.Object
setmetatable(Note.Object, Note)

function Note.Object:on()
    if self.object then self.object:attach() end
end

function Note.Object:off()
    if self.object then self.object:detach() end
end

function Note:fromObject(object)
    local note = Note:new()
    setmetatable(note, Note.Object)
    note.object = object
    return note
end

-- Note.Effect: toggle scene effects
Note.Effect = {}
Note.Effect.__index = Note.Effect
setmetatable(Note.Effect, Note)

function Note.Effect:on()
    if self.effect then self.effect(table.unpack(self.args)) end
end

function Note.Effect:off()
    if self.effect then self.effect(false) end
end

function Note:fromEffect(effect, ...)
    local note = Note:new()
    setmetatable(note, Note.Effect)
    note.effect = effect
    note.args = {...}
    return note
end

-- Note.Action: call a curried function
Note.Action = {}
Note.Action.__index = Note.Action
setmetatable(Note.Action, Note)

function Note.Action:on()
    if self.action then self.action() end
end

function Note.Action:off() end

function Note:fromAction(instance, action, ...)
    local note = Note:new()
    setmetatable(note, Note.Action)
    note.action = curry(action, instance, ...)
    return note
end
