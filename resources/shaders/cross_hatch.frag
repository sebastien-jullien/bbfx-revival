#version 330 core

uniform sampler2D tex0;
uniform float line_density;
uniform float threshold;

in  vec2 oUv0;
out vec4 fragColor;

void main() {
    vec4 col = texture(tex0, oUv0);
    float lum = dot(col.rgb, vec3(0.299, 0.587, 0.114));

    // Base paper color
    vec3 paper = vec3(0.95);
    vec3 ink = vec3(0.05);

    // Scale coordinates for hatching
    float scale = line_density;
    vec2 p = oUv0 * scale;

    // Hatching layers — more layers for darker areas
    float hatch = 1.0;

    // Layer 1: diagonal /
    float line1 = abs(sin((p.x + p.y) * 3.14159));
    if (lum < threshold) {
        hatch = min(hatch, smoothstep(0.3, 0.5, line1));
    }

    // Layer 2: diagonal \ (for darker areas)
    float line2 = abs(sin((p.x - p.y) * 3.14159));
    if (lum < threshold * 0.7) {
        hatch = min(hatch, smoothstep(0.3, 0.5, line2));
    }

    // Layer 3: horizontal (for very dark areas)
    float line3 = abs(sin(p.y * 3.14159 * 2.0));
    if (lum < threshold * 0.4) {
        hatch = min(hatch, smoothstep(0.3, 0.5, line3));
    }

    // Layer 4: vertical (for near-black)
    float line4 = abs(sin(p.x * 3.14159 * 2.0));
    if (lum < threshold * 0.2) {
        hatch = min(hatch, smoothstep(0.3, 0.5, line4));
    }

    vec3 result = mix(ink, paper, hatch);
    fragColor = vec4(result, 1.0);
}
