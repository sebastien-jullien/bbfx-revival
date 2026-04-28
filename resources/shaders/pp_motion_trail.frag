#version 330 core

uniform sampler2D tex0;
uniform sampler2D prevFrame;
uniform float trail_decay;

in  vec2 oUv0;
out vec4 fragColor;

void main() {
    // Current frame
    vec4 current = texture(tex0, oUv0);

    // Previous frame with decay
    vec4 prev = texture(prevFrame, oUv0) * (1.0 - trail_decay * 0.1);

    // Real motion trail: blend current on top of fading history
    fragColor = max(current, prev);
}
