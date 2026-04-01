#version 330 core
uniform float time;
uniform float zoom;
uniform float center_x;
uniform float center_y;
uniform float max_iter;
uniform float color_speed;
in vec2 oUv0;
out vec4 fragColor;
void main() {
    float z = max(zoom, 0.5);
    vec2 c = (oUv0 - 0.5) * 2.0 / z + vec2(center_x, center_y);
    vec2 zn = vec2(0.0);
    int iter = 0;
    int mi = int(max(max_iter, 50.0));
    for (int i = 0; i < 1000; i++) {
        if (i >= mi) break;
        zn = vec2(zn.x * zn.x - zn.y * zn.y + c.x, 2.0 * zn.x * zn.y + c.y);
        if (dot(zn, zn) > 4.0) break;
        iter++;
    }
    float t = float(iter) / float(mi);
    vec3 col = 0.5 + 0.5 * cos(6.2831 * (t * 3.0 + time * color_speed + vec3(0.0, 0.33, 0.67)));
    if (iter >= mi) col = vec3(0.0);
    fragColor = vec4(col, 1.0);
}
