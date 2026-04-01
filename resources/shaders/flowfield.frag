#version 330 core
uniform float time;
uniform float scale;
uniform float speed;
uniform float curl;
in vec2 oUv0;
out vec4 fragColor;
vec2 rotate2(vec2 v, float a) { float c = cos(a), s = sin(a); return vec2(v.x*c - v.y*s, v.x*s + v.y*c); }
void main() {
    vec2 uv = oUv0 * scale;
    float angle = sin(uv.x * 3.0 + time * speed) * cos(uv.y * 2.0 + time * speed * 0.7) * curl;
    vec2 dir = rotate2(vec2(1.0, 0.0), angle);
    float line = abs(dot(fract(uv * 5.0) - 0.5, normalize(dir)));
    line = smoothstep(0.05, 0.0, line);
    vec3 col = mix(vec3(0.02, 0.02, 0.08), vec3(0.2, 0.6, 1.0), line);
    fragColor = vec4(col, 1.0);
}
