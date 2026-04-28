#version 330 core
uniform sampler2D tex0;
uniform float threshold;
uniform float intensity;
uniform float blur_size;
in vec2 oUv0;
out vec4 fragColor;

void main() {
    vec2 texSize = vec2(textureSize(tex0, 0));
    vec2 texel = blur_size / texSize;

    // 9-tap gaussian blur on bright pixels
    vec4 sum = vec4(0.0);
    float weights[9] = float[](
        0.0162, 0.0540, 0.1216, 0.1945, 0.2274,
        0.1945, 0.1216, 0.0540, 0.0162
    );

    // Horizontal + vertical combined (cross pattern for single-pass approximation)
    for (int i = -4; i <= 4; i++) {
        vec2 offsetH = vec2(float(i) * texel.x, 0.0);
        vec2 offsetV = vec2(0.0, float(i) * texel.y);
        vec4 sampleH = texture(tex0, oUv0 + offsetH);
        vec4 sampleV = texture(tex0, oUv0 + offsetV);

        // Extract bright pixels above threshold
        float lumH = dot(sampleH.rgb, vec3(0.299, 0.587, 0.114));
        float lumV = dot(sampleV.rgb, vec3(0.299, 0.587, 0.114));
        vec4 brightH = sampleH * max(0.0, lumH - threshold) / max(lumH, 0.001);
        vec4 brightV = sampleV * max(0.0, lumV - threshold) / max(lumV, 0.001);

        sum += (brightH + brightV) * weights[i + 4] * 0.5;
    }

    // Original + bloom blend
    vec4 original = texture(tex0, oUv0);
    fragColor = original + sum * intensity;
}
