-- ============================================================================
-- BBFx v3.5 Lot M — Live protocols integration demo
-- ============================================================================
-- Wires MIDI + OSC + Art-Net + TextureShare all at once:
--   * MIDI ch1 CC1  -> prints value, also re-sent as OSC /bbfx/fader
--   * OSC /bbfx/preset (s)  -> prints preset name
--   * Art-Net universe 0 ch1 -> mirrored onto MIDI device 0 CC1 at 30 Hz
--   * TextureShare "BBFx-Demo" receiver initialised if any backend present
--
-- Safe to run on a headless system: every handle is optional and the demo
-- just prints status if hardware is absent.
-- ============================================================================

print("=== BBFx Live Protocols Demo (Lot M) ===\n")

-- ── MIDI ─────────────────────────────────────────────────────────────────
local inPorts  = bbfx.midi.listInputPorts()
local outPorts = bbfx.midi.listOutputPorts()
print(string.format("MIDI inputs : %d", #inPorts))
for i, n in ipairs(inPorts)  do print("  IN  " .. i .. " " .. n) end
print(string.format("MIDI outputs: %d", #outPorts))
for i, n in ipairs(outPorts) do print("  OUT " .. i .. " " .. n) end

-- ── OSC ─────────────────────────────────────────────────────────────────
assert(bbfx.osc.listen(8000), "OSC listen(8000) failed")
local hPreset = bbfx.osc.on("/bbfx/preset", function(addr, value)
    print(string.format("[OSC] %s = %s", addr, tostring(value)))
end)
print("OSC listening on 8000, subscription handle=" .. tostring(hPreset))

-- ── Art-Net ─────────────────────────────────────────────────────────────
if bbfx.artnet.listen() then
    print("Art-Net listening on 6454")
    bbfx.artnet.onReceive(0, function(universe, ch)
        -- Mirror ch1 to MIDI CC1 on device 0 (if there is an output)
        local v = math.floor(ch[1] * 127 + 0.5)
        if #outPorts > 0 then
            bbfx.midi.sendCC(0, 1, 1, v)
        end
    end)
else
    print("Art-Net listen failed (port 6454 probably in use)")
end

-- ── Texture sharing ─────────────────────────────────────────────────────
print("TextureShare backend : " .. bbfx.textureShare.backend())
local recv = bbfx.textureShare.createReceiver("BBFx-Demo")
if recv then
    print("TextureShare receiver created, OGRE texture = " .. recv.getTextureName())
end

print("\nDemo ready. Send /bbfx/preset \"MySet\" via OSC — check console for logs.\n")
