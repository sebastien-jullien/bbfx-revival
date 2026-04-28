#version 330 core

uniform sampler2D tex0;
uniform float lut_intensity;
uniform float temperature;
uniform float tint;
uniform float contrast;
uniform float saturation;

in  vec2 oUv0;
out vec4 fragColor;

void main() {
    vec3 col = texture(tex0, oUv0).rgb;

    // Temperature (warm/cool shift)
    // Positive = warm (more red/yellow), negative = cool (more blue)
    float temp = temperature * 0.1;
    col.r += temp;
    col.b -= temp;

    // Tint (green/magenta shift)
    float t = tint * 0.1;
    col.g += t;

    // Contrast (S-curve)
    col = (col - 0.5) * (1.0 + contrast) + 0.5;

    // Saturation
    float lum = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(lum), col, 1.0 + saturation);

    // Mix with original based on intensity
    vec3 orig = texture(tex0, oUv0).rgb;
    col = mix(orig, col, lut_intensity);

    fragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
