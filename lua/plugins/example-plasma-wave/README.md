# Plasma Wave

Audio-reactive plasma generator showcased as a v3.5 example plugin.

![Plasma Wave preview](thumbnail.png)

## Features

- Classic plasma procedural noise driven by 4 summed sinusoids.
- Audio-reactive scaling from FFT low-band (permission : `audio`).
- 8 bindable ports — `speed`, `scale`, `audioGain`, `time`, and 4
  outputs (`value`, `r`, `g`, `b`).
- Fragment shader `shaders/plasma_wave.frag` for the GPU path.

## Usage

1. Install from Community Browser, or drop this directory under
   `Documents/BBFx/plugins/example.plasma-wave/`.
2. Enable in Plugin Manager (Ctrl+Shift+X).
3. In the NodeEditor, right-click → Source → PlasmaWave. Connect the
   `time` port to a RootTimeNode.totalTime and the `value` output to a
   MixerNode fader for an audio-reactive mix.

## Permissions

| Permission | Usage                                       |
|------------|---------------------------------------------|
| `audio`    | Reads `bbfx.audio.getBands()` low-band FFT. |

## Changelog

### 1.0.0 — 2026-04-18

- Initial release for BBFx v3.5.
- Lua-side registration + GLSL fragment shader.
- Audio-reactive gain on FFT band[0].
