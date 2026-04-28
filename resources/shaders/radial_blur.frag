#version 330 core

uniform sampler2D tex0;
uniform float center_x;
uniform float center_y;
uniform float intensity;
uniform float samples;

in  vec2 oUv0;
out vec4 fragColor;

void main() {
    vec2 center = vec2(center_x, center_y);
    vec2 dir = oUv0 - center;
    float dist = length(dir);

    int numSamples = int(clamp(samples, 4.0, 32.0));
    float blurAmount = intensity * 0.01 * dist;

    vec4 result = vec4(0.0);
    float totalWeight = 0.0;

    for (int i = 0; i < numSamples; i++) {
        float t = float(i) / float(numSamples - 1) - 0.5;
        vec2 samplePos = oUv0 - dir * t * blurAmount;

        // Gaussian-ish weight
        float w = 1.0 - abs(t) * 2.0;
        w = max(w, 0.0);

        result += texture(tex0, samplePos) * w;
        totalWeight += w;
    }

    fragColor = result / totalWeight;
}
