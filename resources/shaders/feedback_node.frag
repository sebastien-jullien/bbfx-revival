// feedback_node.frag — TextureFeedbackNode shader (v3.5.2 Lot K)
// 2 TUS in: tex0 (current frame source) + prevFrame (frame N-1 cached).
// Computes the next-frame composite according to:
//   - decay        : multiplier on prev_frame alpha (0..1)
//   - displacement : 2D shift applied to the prev_frame sample UV
//   - zoom         : scale around center (1.0 = no zoom)
//   - rotateAngle  : rotation in radians around center
//   - blendMode    : 0=Additive, 1=Screen, 2=Max
//
// The PostProcessStack-style PrevFrame RTT (cf. v3.5.1 Lot H) is bound
// automatically when the material has a `prevFrame` named-constant —
// TextureFeedbackNode performs the RTT swap itself between frames.
#version 330 core

uniform sampler2D tex0;
uniform sampler2D prevFrame;
uniform float decay;
uniform vec2  displacement;
uniform float zoom;
uniform float rotateAngle;
uniform int   blendMode;

in  vec2 oUv0;
out vec4 fragColor;

void main() {
    // Current frame sample
    vec4 current = texture(tex0, oUv0);

    // Previous frame UV with zoom + rotate + displacement around the center.
    vec2 c = vec2(0.5);
    vec2 uv = oUv0 - c;
    float ca = cos(rotateAngle);
    float sa = sin(rotateAngle);
    uv = vec2(uv.x * ca - uv.y * sa,
              uv.x * sa + uv.y * ca);
    uv /= max(0.001, zoom);
    uv += c + displacement;

    vec4 prev = texture(prevFrame, clamp(uv, 0.0, 1.0)) * decay;

    if (blendMode == 0) {
        fragColor = clamp(current + prev, 0.0, 1.0);   // Additive
    } else if (blendMode == 1) {
        fragColor = 1.0 - (1.0 - current) * (1.0 - prev); // Screen
    } else {
        fragColor = max(current, prev);                   // Max
    }
}
