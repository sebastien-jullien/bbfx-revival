#version 330 core
// Digital glitch effect — block displacement + RGB offset + scanlines + noise
uniform float time;
uniform float intensity;   // 0..1 (def 0.5)
uniform float block_size;  // pixels (def 8.0)
uniform float rgb_shift;   // UV offset (def 0.01)
uniform sampler2D tex0;

in vec2 oUv0;
out vec4 fragColor;

float hash(vec2 p) {
    float h = dot(p, vec2(127.1, 311.7));
    return fract(sin(h) * 43758.5453);
}

void main() {
    vec2 uv = oUv0;
    vec2 texSize = vec2(textureSize(tex0, 0));
    float blockW = block_size / texSize.x;
    float blockH = block_size / texSize.y;

    // Block displacement
    vec2 block = floor(uv / vec2(blockW, blockH));
    float blockNoise = hash(block + floor(time * 3.0));
    float threshold = 1.0 - intensity * 0.8;

    if (blockNoise > threshold) {
        float shift = (blockNoise - threshold) * 0.1;
        uv.x += shift * sign(blockNoise - 0.5);
    }

    // Scanlines
    float scanline = sin(uv.y * texSize.y * 1.5 + time * 10.0) * 0.03 * intensity;

    // RGB channel separation
    float shift = rgb_shift * intensity;
    float r = texture(tex0, uv + vec2(shift, 0.0)).r;
    float g = texture(tex0, uv).g;
    float b = texture(tex0, uv - vec2(shift, 0.0)).b;

    // Noise burst
    float noise = hash(uv * texSize + time * 100.0) * intensity * 0.15;

    fragColor = vec4(r + noise + scanline, g + noise, b + noise - scanline, 1.0);
}
