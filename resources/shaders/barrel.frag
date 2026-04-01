#version 330 core
uniform sampler2D tex0;
uniform float distortion;
in vec2 oUv0;
out vec4 fragColor;
void main() {
    vec2 uv = oUv0 * 2.0 - 1.0;
    float r2 = dot(uv, uv);
    uv *= 1.0 + distortion * r2;
    uv = uv * 0.5 + 0.5;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    else
        fragColor = texture(tex0, uv);
}
