#version 330 core
// Motion trail / echo effect — temporal blur using multiple UV offsets
uniform float time;
uniform float trail_length;  // 0..1 (def 0.3)
uniform float decay;         // 0..1 (def 0.85)
uniform sampler2D tex0;

in vec2 oUv0;
out vec4 fragColor;

void main() {
    vec4 color = texture(tex0, oUv0);

    // Create echo trails by sampling at time-shifted UV positions
    float angle = time * 0.5;
    vec2 dir = vec2(cos(angle), sin(angle)) * trail_length * 0.02;

    vec4 trail1 = texture(tex0, oUv0 - dir) * decay;
    vec4 trail2 = texture(tex0, oUv0 - dir * 2.0) * decay * decay;
    vec4 trail3 = texture(tex0, oUv0 - dir * 3.0) * decay * decay * decay;
    vec4 trail4 = texture(tex0, oUv0 - dir * 4.0) * decay * decay * decay * decay;

    // Blend trails with original
    fragColor = max(color, max(trail1, max(trail2, max(trail3, trail4))));
}
