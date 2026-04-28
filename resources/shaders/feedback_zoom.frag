#version 330 core

uniform sampler2D tex0;
uniform sampler2D prevFrame;
uniform float decay;
uniform float zoom;
uniform float rotation;

in  vec2 oUv0;
out vec4 fragColor;

void main() {
    // Current frame
    vec4 current = texture(tex0, oUv0);

    // Sample previous frame with zoom + rotation from center
    vec2 center = vec2(0.5);
    vec2 uv = oUv0 - center;

    // Apply zoom (scale towards center)
    uv *= 1.0 - zoom * 0.02;

    // Apply rotation
    float angle = rotation * 0.01;
    float ca = cos(angle), sa = sin(angle);
    uv = vec2(uv.x * ca - uv.y * sa, uv.x * sa + uv.y * ca);

    uv += center;

    // Sample feedback with decay
    vec4 prev = texture(prevFrame, clamp(uv, 0.0, 1.0));
    vec4 feedback = prev * (1.0 - decay * 0.05);

    // Blend: current frame on top of fading feedback
    fragColor = max(current, feedback);
}
