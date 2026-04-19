#pragma once

#include <functional>
#include <string>

namespace bbfx {

/// v3.5 Lot R — signed-distance-field primitives + boolean ops +
/// marching-cubes meshing helpers.
///
/// All SDF functions follow Inigo Quilez's conventions: they return the
/// signed distance to the surface (negative inside, 0 on surface, positive
/// outside) from a sample point.
class SDFPrimitives {
public:
    // --- Primitives -----------------------------------------------------
    static float sphere(float x, float y, float z,
                         float cx, float cy, float cz, float radius);
    static float box   (float x, float y, float z,
                         float cx, float cy, float cz,
                         float hx, float hy, float hz);
    static float torus (float x, float y, float z,
                         float cx, float cy, float cz,
                         float majorR, float minorR);

    // --- Boolean ops ----------------------------------------------------
    static float opUnion       (float d1, float d2);
    static float opIntersection(float d1, float d2);
    static float opSubtraction (float d1, float d2);
    static float opSmoothUnion (float d1, float d2, float k);

    // --- Marching cubes -------------------------------------------------
    using Field = std::function<float(float x, float y, float z)>;

    /// Mesh an isosurface of `field` over an axis-aligned box defined by
    /// the corners (x0,y0,z0)..(x1,y1,z1) at `resolution` cells per axis.
    /// Builds an OGRE ManualObject mesh and returns its name; empty on
    /// failure.
    static std::string toMesh(const std::string& meshName,
                                  Field field,
                                  float x0, float y0, float z0,
                                  float x1, float y1, float z1,
                                  int resolution);
};

} // namespace bbfx
