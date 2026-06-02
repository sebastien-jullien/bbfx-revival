// noise_generator.frag — NoiseTextureNode GPU path (v3.5.2 Lot M)
// 4 noise types branched on `noiseType` uniform: Perlin (0), Worley (1),
// Simplex (2), Voronoi (3). Animatable via `timeOffset` (third dimension)
// and `displacement` (2D shift on the sample plane).
#version 330 core

uniform int   noiseType;
uniform float scaleParam;
uniform int   octaves;
uniform float lacunarity;
uniform float persistence;
uniform float timeOffset;
uniform vec2  displacement;
uniform int   seedOffset;

in  vec2 oUv0;
out vec4 fragColor;

// ── 32-bit hash (xxhash-like, fast and decent) ────────────────────────────
uint hash32(uint x) {
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
uint hash2(int xi, int yi, int seed) {
    return hash32(uint(xi) * 374761393u + uint(yi) * 668265263u + uint(seed) * 982451653u);
}
float hashf(int xi, int yi, int seed) {
    return float(hash2(xi, yi, seed) & 0xffffffu) / float(0xffffffu);
}

// ── Perlin 2D ──────────────────────────────────────────────────────────────
float fade(float t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }
float grad(int xi, int yi, float dx, float dy, int seed) {
    uint h = hash2(xi, yi, seed) & 7u;
    vec2 g = vec2(0.0);
    if      (h == 0u) g = vec2( 1.0,  0.0);
    else if (h == 1u) g = vec2(-1.0,  0.0);
    else if (h == 2u) g = vec2( 0.0,  1.0);
    else if (h == 3u) g = vec2( 0.0, -1.0);
    else if (h == 4u) g = vec2( 1.0,  1.0);
    else if (h == 5u) g = vec2(-1.0,  1.0);
    else if (h == 6u) g = vec2( 1.0, -1.0);
    else              g = vec2(-1.0, -1.0);
    return g.x * dx + g.y * dy;
}
float perlin2D(vec2 p, int seed) {
    int xi = int(floor(p.x));
    int yi = int(floor(p.y));
    float xf = p.x - float(xi);
    float yf = p.y - float(yi);
    float u = fade(xf);
    float v = fade(yf);
    float n00 = grad(xi,     yi,     xf,     yf,     seed);
    float n10 = grad(xi + 1, yi,     xf - 1.0, yf,     seed);
    float n01 = grad(xi,     yi + 1, xf,     yf - 1.0, seed);
    float n11 = grad(xi + 1, yi + 1, xf - 1.0, yf - 1.0, seed);
    return mix(mix(n00, n10, u), mix(n01, n11, u), v) * 0.5 + 0.5;
}

// ── Worley (cellular distance) ─────────────────────────────────────────────
float worley2D(vec2 p, int seed) {
    int xi = int(floor(p.x));
    int yi = int(floor(p.y));
    float minDist = 1e9;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int cx = xi + dx;
            int cy = yi + dy;
            float fx = hashf(cx, cy, seed);
            float fy = hashf(cx, cy, seed + 1);
            vec2 cellPt = vec2(float(cx) + fx, float(cy) + fy);
            float d2 = dot(cellPt - p, cellPt - p);
            if (d2 < minDist) minDist = d2;
        }
    }
    return clamp(sqrt(minDist), 0.0, 1.0);
}

// ── Voronoi (closest cell id) ──────────────────────────────────────────────
float voronoi2D(vec2 p, int seed) {
    int xi = int(floor(p.x));
    int yi = int(floor(p.y));
    uint closest = 0u;
    float minDist = 1e9;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int cx = xi + dx;
            int cy = yi + dy;
            uint h = hash2(cx, cy, seed);
            float fx = float(h & 0xffffu) / 65535.0;
            float fy = float((h >> 16) & 0xffffu) / 65535.0;
            vec2 cellPt = vec2(float(cx) + fx, float(cy) + fy);
            float d2 = dot(cellPt - p, cellPt - p);
            if (d2 < minDist) { minDist = d2; closest = h; }
        }
    }
    return float(closest & 0xffffu) / 65535.0;
}

// ── Simplex (approximation via Perlin with rotated grid) ──────────────────
float simplex2D(vec2 p, int seed) {
    // True simplex would require its own gradient table; this is a close
    // visual proxy that uses Perlin on a 1.2x rotated grid.
    float c = cos(0.5);
    float s = sin(0.5);
    vec2 q = vec2(c * p.x - s * p.y, s * p.x + c * p.y) * 1.2;
    return perlin2D(q, seed + 7);
}

void main() {
    vec2 p = (oUv0 + displacement) * scaleParam + vec2(timeOffset);
    float v = 0.0;
    if (noiseType == 0) {
        // Perlin with FBM octaves
        float amp = 1.0;
        float freq = 1.0;
        float total = 0.0;
        for (int i = 0; i < octaves; ++i) {
            v += amp * perlin2D(p * freq, seedOffset + i);
            total += amp;
            amp *= persistence;
            freq *= lacunarity;
        }
        if (total > 0.0) v /= total;
    } else if (noiseType == 1) {
        v = worley2D(p, seedOffset);
    } else if (noiseType == 2) {
        v = simplex2D(p, seedOffset);
    } else {
        v = voronoi2D(p, seedOffset);
    }
    fragColor = vec4(v, v, v, 1.0);
}
