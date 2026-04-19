// ============================================================================
// BBFx v3.5 example plugin — Plasma Wave fragment shader
// ============================================================================
#version 330 core

in  vec2 vUV;
out vec4 fragColor;

uniform float uTime;
uniform float uSpeed;
uniform float uScale;
uniform float uAudioGain;

void main() {
    vec2 p = (vUV - 0.5) * 4.0;
    float t = uTime * uSpeed;

    // Classic plasma : sum of sinusoidal fields.
    float v  = sin(p.x * 10.0 * uScale + t);
          v += sin(p.y * 10.0 * uScale + t * 0.7);
          v += sin((p.x + p.y) * 6.0 * uScale + t * 0.8);
          v += sin(length(p) * 8.0 * uScale - t);
    v = v / 4.0;

    // Audio-reactive gain bump.
    float a = uAudioGain;
    v *= (1.0 + a * 0.5);

    vec3 col = vec3(
        0.5 + 0.5 * sin(v * 3.1415926 + 0.0),
        0.5 + 0.5 * sin(v * 3.1415926 + 2.094),
        0.5 + 0.5 * sin(v * 3.1415926 + 4.188)
    );
    fragColor = vec4(col, 1.0);
}
