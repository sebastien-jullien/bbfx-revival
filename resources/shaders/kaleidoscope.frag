#version 330 core
uniform sampler2D tex0;
uniform float time;
uniform float segments;
uniform float rotation;
uniform float zoom;
in vec2 oUv0;
out vec4 fragColor;
void main() {
    vec2 uv = oUv0 - 0.5;
    float angle = atan(uv.y, uv.x) + radians(rotation) + time * 0.2;
    float r = length(uv) * zoom;
    float seg = max(segments, 2.0);
    float segAngle = 3.14159 * 2.0 / seg;
    angle = mod(angle, segAngle);
    if (angle > segAngle * 0.5) angle = segAngle - angle; // mirror
    vec2 texUv = vec2(cos(angle), sin(angle)) * r + 0.5;
    fragColor = texture(tex0, texUv);
}
