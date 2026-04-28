#version 330 core
uniform sampler2D tex0;
uniform float focus;
uniform float range;
uniform float blur_amount;
in vec2 oUv0;
out vec4 fragColor;

void main() {
    vec2 texSize = vec2(textureSize(tex0, 0));
    vec2 texel = 1.0 / texSize;

    // Distance from focus line (vertical position based)
    float dist = abs(oUv0.y - focus);
    float blur = clamp(dist / max(range, 0.001), 0.0, 1.0) * blur_amount;

    if (blur < 0.001) {
        fragColor = texture(tex0, oUv0);
        return;
    }

    // Variable-radius gaussian blur
    vec4 sum = vec4(0.0);
    float totalWeight = 0.0;
    int samples = int(min(blur * 8.0, 12.0));
    samples = max(samples, 1);

    for (int x = -samples; x <= samples; x++) {
        for (int y = -samples; y <= samples; y++) {
            vec2 offset = vec2(float(x), float(y)) * texel * blur;
            float w = exp(-float(x*x + y*y) / (2.0 * max(blur * blur, 0.01)));
            sum += texture(tex0, oUv0 + offset) * w;
            totalWeight += w;
        }
    }

    fragColor = sum / totalWeight;
}
