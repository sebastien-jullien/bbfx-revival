#pragma once

#include "WarpProfile.h"
#include "BlendProfile.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace Ogre { class SceneManager; class Camera; }

namespace bbfx {

/// A rectangular projection zone on the physical surface.
/// All coordinates are normalised [0, 1] relative to the canvas.
struct Zone {
    int         id           = -1;
    std::string name;
    float       x            = 0.f;  ///< Left edge (normalised)
    float       y            = 0.f;  ///< Top edge (normalised)
    float       width        = 0.5f; ///< Width (normalised)
    float       height       = 0.5f; ///< Height (normalised)
    int         outputSlotId = -1;   ///< Assigned output slot (-1 = unassigned)

    // ── Camera (v3.4) ──────────────────────────────────────────────────────
    bool        customCamera = false;
    std::string cameraName;   ///< OGRE camera name, e.g. "Zone_0_Camera"

    // ── Warp/Blend override per-zone (v3.4) ────────────────────────────────
    WarpProfile  warpProfile;
    BlendProfile blendProfile;
    bool         warpEnabled  = false;
    bool         blendEnabled = false;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

/// Manages the collection of projection zones and the shared canvas.
///
/// The canvas represents the physical projection surface(s) in 2D (top view).
/// canvasWidth × canvasHeight are in metres (default: 10 × 3).
class SurfaceMap {
public:
    SurfaceMap() = default;
    ~SurfaceMap() = default;

    // ── CRUD ────────────────────────────────────────────────────────────────

    /// Add a zone with default position (centred).
    int addZone(const std::string& name, float x, float y, float w, float h);
    /// Remove a zone by id.
    void removeZone(int id);
    /// Clear all zones.
    void clear();

    Zone*       getZone(int id);
    const Zone* getZone(int id) const;
    const std::vector<Zone>& getAllZones() const { return mZones; }
    int size() const { return static_cast<int>(mZones.size()); }

    // ── Canvas ──────────────────────────────────────────────────────────────

    float canvasWidth  = 10.f; ///< Physical width in metres
    float canvasHeight =  3.f; ///< Physical height in metres
    std::string backgroundImagePath; ///< Optional background image for the canvas

    // ── Camera management ───────────────────────────────────────────────────

    /// Create an OGRE camera for a zone and store its name in zone.cameraName.
    void createCameraForZone(int zoneId, Ogre::SceneManager* sm);
    /// Destroy the OGRE camera for a zone.
    void destroyCameraForZone(int zoneId, Ogre::SceneManager* sm);
    /// Destroy all zone cameras.
    void destroyAllCameras(Ogre::SceneManager* sm);

    // ── Zone/slot linkage ────────────────────────────────────────────────────

    /// Assign a zone to an output slot id.  Clears any previous assignment.
    void assignZoneToSlot(int zoneId, int outputSlotId);
    /// Unassign a zone.
    void unassignZone(int zoneId);
    /// Called when an output slot is removed: clear all references to it.
    void onSlotRemoved(int outputSlotId);

    // ── JSON serialisation ───────────────────────────────────────────────────

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j, Ogre::SceneManager* sm);

private:
    std::vector<Zone> mZones;
    int mNextZoneId = 0;
};

} // namespace bbfx
