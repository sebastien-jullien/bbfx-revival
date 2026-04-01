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
};

} // namespace bbfx
