#version 330 core

uniform sampler2D tex0;
uniform float radius;

in  vec2 oUv0;
out vec4 fragColor;

void main() {
    // Simplified Kuwahara filter — divides neighborhood into 4 quadrants
    // and picks the one with minimum variance (smoothest region)

    vec2 texSize = vec2(textureSize(tex0, 0));
    vec2 texel = 1.0 / texSize;
    int r = int(clamp(radius, 1.0, 8.0));

    // 4 quadrant accumulators
    vec3 mean[4];
    vec3 sigma[4];
    float count[4];

    for (int i = 0; i < 4; i++) {
        mean[i] = vec3(0.0);
        sigma[i] = vec3(0.0);
        count[i] = 0.0;
    }

    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            vec2 offset = vec2(float(dx), float(dy)) * texel;
            vec3 col = texture(tex0, oUv0 + offset).rgb;

            // Determine quadrant: 0=TL, 1=TR, 2=BL, 3=BR
            int qx = dx >= 0 ? 1 : 0;
            int qy = dy >= 0 ? 1 : 0;
            int q = qy * 2 + qx;

            mean[q] += col;
            sigma[q] += col * col;
            count[q] += 1.0;
        }
    }

    // Find quadrant with minimum variance
    float minVar = 1e10;
    vec3 result = vec3(0.0);

    for (int i = 0; i < 4; i++) {
        if (count[i] > 0.0) {
            mean[i] /= count[i];
            sigma[i] = sigma[i] / count[i] - mean[i] * mean[i];
            float variance = sigma[i].r + sigma[i].g + sigma[i].b;
            if (variance < minVar) {
                minVar = variance;
                result = mean[i];
            }
        }
    }

    fragColor = vec4(result, 1.0);
}
