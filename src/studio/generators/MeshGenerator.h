#pragma once

#include <OgreMesh.h>
#include <string>

namespace bbfx {

/// Procedural mesh generators for the Studio.
/// Each method creates an Ogre::Mesh and registers it in MeshManager.
class MeshGenerator {
public:
    static Ogre::MeshPtr generateGeosphere(int frequency);
    static Ogre::MeshPtr generateSphere(int rings, int segments);
    static Ogre::MeshPtr generateTorus(float majorRadius, float minorRadius, int rings, int segments);
    static Ogre::MeshPtr generateCylinder(float radius, float height, int segments);
    static Ogre::MeshPtr generateCone(float radius, float height, int segments);
    static Ogre::MeshPtr generatePlane(float width, float height, int segX, int segY);
    static Ogre::MeshPtr generateTorusKnot(int p, int q, float radius, float tubeRadius);
    static Ogre::MeshPtr generateGeoEllipse(int frequency, float eccentricity);
    static Ogre::MeshPtr generateCube(float size, int segments = 1);
    static Ogre::MeshPtr generateMobius(float radius, float width, int segments, int sides);
    static Ogre::MeshPtr generateLissajous(int a, int b, int c, float scale, int segments, int sides);
    static Ogre::MeshPtr generateHelix(float radius, float pitch, int turns, int segments, int sides);
    static Ogre::MeshPtr generateDiamond(float radius, float height, int segments);
    static Ogre::MeshPtr generateStar3D(float outerRadius, float innerRadius, int points, float depth);

    /// Pre-register all canonical procedural meshes in MeshManager.
    /// Must be called once at startup before any scene loading.
    /// Creates: torus.mesh, cylinder.mesh, cone.mesh, plane_1m.mesh,
    ///          torusknot.mesh, cube_1m.mesh, bbfx_plane.mesh
    static void registerDefaults();
};

} // namespace bbfx
