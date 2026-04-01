#version 330 core
uniform float time;
uniform float scale;
uniform float speed;
uniform float edge_width;
in vec2 oUv0;
out vec4 fragColor;
vec2 hash2(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return fract(sin(p) * 43758.5453);
}
void main() {
    vec2 uv = oUv0 * scale;
    vec2 ip = floor(uv);
    vec2 fp = fract(uv);
    float minDist = 1.0;
    for (int y = -1; y <= 1; y++)
    for (int x = -1; x <= 1; x++) {
        vec2 neighbor = vec2(float(x), float(y));
        vec2 point = hash2(ip + neighbor);
        point = 0.5 + 0.5 * sin(time * speed + 6.2831 * point);
        float d = length(neighbor + point - fp);
        minDist = min(minDist, d);
    }
    float edge = smoothstep(edge_width, edge_width + 0.02, minDist);
    vec3 col = mix(vec3(0.1, 0.8, 1.0), vec3(0.02, 0.02, 0.1), edge);
    fragColor = vec4(col, 1.0);
}
