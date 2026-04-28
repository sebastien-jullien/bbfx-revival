#version 330 core
uniform sampler2D tex0;
uniform float time;
uniform float distortionFreq;
uniform float distortionScale;
uniform float distortionRoll;
uniform float interference;
uniform float frameLimit;
uniform float frameSharpness;
in vec2 oUv0;
out vec4 fragColor;

// Pseudo-random hash (replaces 3D noise textures)
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise2d(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
    float t = mod(time, 120.0);
    float sinT = sin(t);

    // Scanline jitter
    float jitter = noise2d(vec2(oUv0.y * 100.0, t * 10.0));
    vec2 uv = oUv0;
    uv.x += (jitter - 0.5) * 0.01 * distortionScale;

    // Vertical roll
    uv.y += sin(uv.y * distortionFreq + t * distortionRoll) * 0.01;

    vec4 col = texture(tex0, uv);

    // Noise interference
    float n = noise2d(oUv0 * 6.0 + vec2(sinT * 50.0, t * 30.0));
    col.rgb += (n - 0.5) * interference;

    // Vignette / frame shape
    vec2 edge = abs(oUv0 - vec2(0.5)) * 2.0;
    float frame = clamp(1.0 - pow(edge.x, frameSharpness) - pow(edge.y, frameSharpness), frameLimit, 1.0);
    col.rgb *= frame;

    fragColor = col;
}
