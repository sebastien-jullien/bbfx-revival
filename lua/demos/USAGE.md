# BBFx Demos

All demos are launched from the build directory:
```
cd build/windows-debug/Debug
bbfx.exe lua/demos/demo_xxx.lua
```

Press **ESC** to quit any demo.

---

## demo_geosphere.lua (v2.3)

**The signature BBFx effect.** An ogre head with Perlin noise vertex deformation, rotating in 3D with orbital camera.

- **What you see:** An ogre head deforming organically via Perlin noise, rotating slowly
- **Controls:** Up/Down = adjust displacement, Left/Right = rotation speed, Joystick axis 1 = Perlin displacement
- **Note:** The "geosphere" name is historical — the mesh used is ogrehead.mesh. In 2006, this was applied to a geosphere mesh

---

## demo_particles.lua (v2.3)

**Particle systems demo.** An ogre head surrounded by OGRE particle effects.

- **What you see:** An ogre head with particle effects (if particle templates are available in resources)
- **Controls:** ESC to quit

---

## demo_video.lua (v2.4/v2.8)

**Theora video playback.** A video plays on a billboard in front of the camera.

- **What you see:** The `bombe.ogg` video playing as a dynamic texture on a billboard
- **Controls:** P = play/pause, R = rewind, B = toggle reverse direction
- **Requires:** `resources/video/bombe.ogg` and `resources/video/bombe_reverse.ogg`

---

## demo_v25.lua (v2.5)

**Animator Avance demo.** Demonstrates the full v2.5 feature set: temporal nodes, spline animation, presets, declarative graph.

- **What you see:** An ogre head with Perlin deformation driven by LFO + RampNode (pulsing effect), with a secondary object animated on a spline path
- **Controls:** `[`/`]` = LFO frequency, P = play/pause spline animation, R = reset, N = next texture preset
- **Features shown:** LFONode, RampNode, Animation spline, PerlinPulse preset, declarative `build()` style

---

## demo_declarative.lua (v2.5)

**Declarative graph style.** Shows how to build an animation graph in < 15 lines of Lua.

- **What you see:** An ogre head with LFO-driven Perlin displacement (same visual as demo_v25 but with minimal code)
- **Controls:** `[`/`]` = LFO frequency
- **Purpose:** Demonstrates the `build({nodes, links})` declarative API

---

## demo_shell.lua (v2.6)

**Interactive shell demo.** REPL console + TCP shell server + hot reload.

- **What you see:** An ogre head with LFO-driven Perlin, plus an interactive console prompt (`bbfx>`)
- **Controls:** Type Lua expressions at the `bbfx>` prompt
- **Commands:** `graph()`, `ports("name")`, `set("name","port",val)`, `help()`, `quit()`
- **TCP:** Connect from another terminal: `python lua/shell/client.py`
- **Hot reload:** `watch("lua/myfile.lua")` to auto-reload on edit

---

## demo_audio.lua (v2.7)

**Audio reactive demo.** Visuals respond to microphone input in real-time.

- **What you see:** An ogre head with Perlin deformation that pulses with the audio level. HUD shows BPM and frequency bands
- **Controls:** H = toggle HUD, `[`/`]` = LFO frequency
- **Requires:** A microphone connected to the system. Play music near the mic to see the effect
- **Note:** If no microphone is detected, the demo runs silently with no audio modulation

---

## demo_gpu.lua (v2.8)

**GPU shader demo.** Perlin noise on GPU via GLSL vertex shader.

- **What you see:** An ogre head with GPU-accelerated Perlin deformation + audio reactive modulation + profiler overlay
- **Controls:** H = toggle HUD, P = toggle profiler, `[`/`]` = displacement
- **Requires:** OpenGL 3.3+ GPU, microphone (optional)
- **Note:** Falls back to CPU PerlinFxNode if GPU shader loading fails

---

## demo_production.lua (v2.9)

**Production pipeline demo.** Record a session, replay it offline, export PNG frames.

- **What you see:** An ogre head with Perlin deformation + audio. Console shows pipeline commands
- **Controls:**
  - R = start/stop recording inputs to `session.bbfx-session`
  - P = replay the recorded session in offline mode (60fps fixed)
  - E = toggle PNG export on/off during replay
- **Pipeline:** Perform live (R) -> Stop recording (R) -> Replay offline (P) -> Toggle export with E -> Press E again to stop -> Quit with ESC
- **Output:** PNG frames in `output/frames/` relative to the launch directory. Typical debug path: `build/windows-debug/Debug/output/frames/`
- **Files:** Exported frames are named `frame_000001.png`, `frame_000002.png`, etc.
- **Video conversion:** `ffmpeg -framerate 60 -i output/frames/frame_%06d.png -c:v libx264 output.mp4`
