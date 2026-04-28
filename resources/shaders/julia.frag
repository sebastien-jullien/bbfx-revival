#version 330 core
// Julia set fractal with animable c parameters
uniform float zoom;        // def 1.0
uniform float cx;          // def -0.7
uniform float cy;          // def 0.27
uniform float max_iter;    // def 100
uniform float color_speed; // def 1.0
uniform float time;
uniform sampler2D tex0;
in vec2 oUv0;
out vec4 fragColor;

void main() {
    vec2 z = (oUv0 - 0.5) * 2.0 / zoom;
    vec2 c = vec2(cx, cy);
    float i;
    for (i = 0.0; i < max_iter; i += 1.0) {
        float x = z.x * z.x - z.y * z.y + c.x;
        float y = 2.0 * z.x * z.y + c.y;
        z = vec2(x, y);
        if (dot(z, z) > 4.0) break;
    }
    float t = i / max_iter;
    // Smooth coloring
    vec3 col = 0.5 + 0.5 * cos(6.28318 * (t * color_speed + time * 0.1 + vec3(0.0, 0.33, 0.67)));
    if (i >= max_iter) col = vec3(0.0);
    fragColor = vec4(col, 1.0);
}
