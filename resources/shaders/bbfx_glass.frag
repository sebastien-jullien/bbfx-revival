#version 330 core
uniform sampler2D tex0;
uniform float time;
uniform float distortion;
uniform float speed;
in vec2 oUv0;
out vec4 fragColor;

void main() {
    // Procedural water-like distortion (replaces normal map dependency)
    vec2 uv = oUv0;
    float t = time * speed;
    vec2 wave;
    wave.x = sin(uv.y * 20.0 + t * 2.0) * cos(uv.x * 15.0 + t * 1.5);
    wave.y = cos(uv.x * 18.0 + t * 1.8) * sin(uv.y * 22.0 + t * 2.2);
    uv += wave * distortion * 0.01;
    fragColor = texture(tex0, uv);
}
