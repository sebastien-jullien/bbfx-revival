#version 330 core
uniform float time;
uniform float scale;
uniform float speed;
uniform float complexity;
in vec2 oUv0;
out vec4 fragColor;
void main() {
    vec2 uv = oUv0 * scale;
    float t = time * speed;
    float v = 0.0;
    v += sin(uv.x * 10.0 + t);
    v += sin((uv.y * 10.0 + t) * 0.5);
    v += sin((uv.x * 10.0 + uv.y * 10.0 + t) * 0.33);
    if (complexity > 1.0) v += sin(length(uv * 10.0 - vec2(5.0)) + t);
    if (complexity > 2.0) v += sin(sqrt(uv.x * uv.x * 100.0 + uv.y * uv.y * 100.0) + t);
    v *= 0.5;
    vec3 col = vec3(sin(v * 3.14159), sin(v * 3.14159 + 2.094), sin(v * 3.14159 + 4.189));
    fragColor = vec4(col * 0.5 + 0.5, 1.0);
}
