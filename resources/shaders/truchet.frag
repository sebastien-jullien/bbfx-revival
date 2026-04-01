#version 330 core
uniform float time;
uniform float scale;
uniform float line_width;
in vec2 oUv0;
out vec4 fragColor;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
void main() {
    vec2 uv = oUv0 * scale;
    vec2 id = floor(uv);
    vec2 f = fract(uv);
    float r = hash(id + floor(time * 0.5));
    float d;
    if (r < 0.5)
        d = min(length(f - vec2(0.0, 0.0)), length(f - vec2(1.0, 1.0)));
    else
        d = min(length(f - vec2(1.0, 0.0)), length(f - vec2(0.0, 1.0)));
    float arc = abs(d - 0.5);
    float line = smoothstep(line_width, line_width * 0.5, arc);
    vec3 col = mix(vec3(0.02), vec3(0.0, 0.8, 1.0), line);
    fragColor = vec4(col, 1.0);
}
