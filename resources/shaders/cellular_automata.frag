#version 330 core
// Procedural cellular automata approximation
uniform float scale;      // def 20.0
uniform float speed;      // def 1.0
uniform float threshold;  // def 0.5
uniform float time;
uniform sampler2D tex0;
in vec2 oUv0;
out vec4 fragColor;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    vec2 cell = floor(oUv0 * scale);
    float t = floor(time * speed);
    // Pseudo-cellular state based on hash of cell + time
    float state = 0.0;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;
            state += step(threshold, hash(cell + vec2(dx, dy) + t * 0.1));
        }
    }
    float self = hash(cell + t * 0.1);
    // Game of Life rules approximation
    float alive = 0.0;
    if (self > threshold) {
        alive = (state >= 2.0 && state <= 3.0) ? 1.0 : 0.0;
    } else {
        alive = (state >= 2.5 && state <= 3.5) ? 1.0 : 0.0;
    }
    vec3 col = alive * vec3(0.2, 0.8, 0.3);
    fragColor = vec4(col, 1.0);
}
