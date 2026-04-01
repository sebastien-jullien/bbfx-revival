#version 330 core
uniform sampler2D tex0;
uniform float time;
uniform float threshold;
uniform float intensity;
in vec2 oUv0;
out vec4 fragColor;
void main() {
    vec4 col = texture(tex0, oUv0);
    float lum = dot(col.rgb, vec3(0.299, 0.587, 0.114));
    if (lum > threshold) {
        // Shift pixels horizontally based on luminance
        float shift = (lum - threshold) * intensity * 0.05;
        shift *= sin(time * 3.0 + oUv0.y * 50.0);
        col = texture(tex0, oUv0 + vec2(shift, 0.0));
    }
    fragColor = col;
}
