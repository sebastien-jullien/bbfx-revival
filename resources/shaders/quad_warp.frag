#version 330 core

// Quad Warp — inverse bilinear mapping (Inigo Quilez's algorithm)
// Remaps output pixels to the sub-quad defined by the 4 corners.
// Identity corners: TL=(0,0) TR=(1,0) BL=(0,1) BR=(1,1)

uniform sampler2D tex0;

uniform float tl_x;
uniform float tl_y;
uniform float tr_x;
uniform float tr_y;
uniform float bl_x;
uniform float bl_y;
uniform float br_x;
uniform float br_y;

in  vec2 oUv0;
out vec4 fragColor;

float cross2(vec2 a, vec2 b) { return a.x * b.y - a.y * b.x; }

void main() {
    vec2 tl = vec2(tl_x, tl_y);
    vec2 tr = vec2(tr_x, tr_y);
    vec2 bl = vec2(bl_x, bl_y);
    vec2 br = vec2(br_x, br_y);
    vec2 p  = oUv0;

    // Bilinear parameterisation:
    //   pos(s,t) = (1-s)(1-t)*tl + s(1-t)*tr + (1-s)t*bl + s*t*br
    //   h = pos(s,t) - tl = s*e + t*f + s*t*g
    // with e = tr-tl, f = bl-tl, g = tl-tr-bl+br
    vec2 e = tr - tl;
    vec2 f = bl - tl;
    vec2 g = tl - tr - bl + br;
    vec2 h = p - tl;

    float k2 = cross2(g, f);
    float k1 = cross2(e, f) + cross2(h, g);
    float k0 = cross2(h, e);

    float s, t;

    if (abs(k2) < 1e-6) {
        // Linear (degenerate quad) case
        float denom = k1 - k0;
        t = (abs(denom) > 1e-9) ? -k0 / denom : 0.0;
        float ex = e.x + g.x * t;
        float ey = e.y + g.y * t;
        s = (abs(ex) > abs(ey)) ? (h.x - f.x * t) / (ex + 1e-12)
                                 : (h.y - f.y * t) / (ey + 1e-12);
    } else {
        float disc = k1 * k1 - 4.0 * k0 * k2;
        if (disc < 0.0) { fragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }
        float w   = sqrt(disc);
        float ik2 = 0.5 / k2;

        // Try first root
        t = (-k1 - w) * ik2;
        float ex = e.x + g.x * t, ey = e.y + g.y * t;
        s = (abs(ex) > abs(ey)) ? (h.x - f.x * t) / (ex + 1e-12)
                                 : (h.y - f.y * t) / (ey + 1e-12);

        if (s < -1e-4 || s > 1.0 + 1e-4 || t < -1e-4 || t > 1.0 + 1e-4) {
            // Try second root
            t  = (-k1 + w) * ik2;
            ex = e.x + g.x * t; ey = e.y + g.y * t;
            s  = (abs(ex) > abs(ey)) ? (h.x - f.x * t) / (ex + 1e-12)
                                      : (h.y - f.y * t) / (ey + 1e-12);
        }
    }

    if (s < -1e-4 || s > 1.0 + 1e-4 || t < -1e-4 || t > 1.0 + 1e-4) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    } else {
        fragColor = texture(tex0, clamp(vec2(s, t), 0.0, 1.0));
    }
}
