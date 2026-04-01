#version 330 core
uniform float time;
uniform float speed;
uniform float twist;
uniform float radius;
in vec2 oUv0;
out vec4 fragColor;
void main() {
    vec2 uv = oUv0 * 2.0 - 1.0;
    float a = atan(uv.y, uv.x) + twist * sin(time * 0.5);
    float r = length(uv);
    float d = radius / (r + 0.001);
    vec2 texUv = vec2(a / 3.14159, d + time * speed);
    vec3 col = 0.5 + 0.5 * cos(6.283 * (texUv.x + texUv.y + vec3(0.0, 0.33, 0.67)));
    col *= smoothstep(0.0, 0.2, r); // darken center
    col *= exp(-r * 0.5); // fog at edges
    fragColor = vec4(col, 1.0);
}
