#pragma once

#include "WarpProfile.h"
#include "BlendProfile.h"
#include <nlohmann/json.hpp>
#include <vector>

namespace Ogre { class Camera; }

namespace bbfx {

class SurfaceMap;
class OutputManager;

/// Per-zone warp/blend state captured at a point in time.
struct ZoneSceneEntry {
    int          zoneId       = -1;
    WarpProfile  warp;
    bool         warpEnabled  = false;
    BlendProfile blend;
    bool         blendEnabled = false;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

/// Captures zone warp/blend + main camera state for scene switching.
/// Parallel to DagSnapshot but for spatial configuration.
/// (v3.4 Lot O — EPIC-159)
class ZoneSnapshot {
public:
    ZoneSnapshot();

    /// Capture current state from SurfaceMap zones and OGRE camera.
    void capture(const SurfaceMap& map, const Ogre::Camera* cam);

    /// Interpolate between two snapshots and apply to SurfaceMap + OutputManager + Camera.
    /// t = 0.0 → 100% a, t = 1.0 → 100% b.
    static void apply(const ZoneSnapshot& a, const ZoneSnapshot& b, float t,
                      SurfaceMap& map, OutputManager& mgr, Ogre::Camera* cam);

    /// JSON serialisation.
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

    bool empty() const { return mEntries.empty(); }
    int zoneCount() const { return static_cast<int>(mEntries.size()); }

    const std::vector<ZoneSceneEntry>& getEntries() const { return mEntries; }

    float camPosX() const { return mCamPosX; }
    float camPosY() const { return mCamPosY; }
    float camPosZ() const { return mCamPosZ; }
    float camFov()  const { return mCamFov; }

private:
    std::vector<ZoneSceneEntry> mEntries;
    float mCamPosX = 0.f;
    float mCamPosY = 5.f;
    float mCamPosZ = 10.f;
    float mCamFov  = 60.f;

    /// Find entry by zoneId (nullptr if not found).
    static const ZoneSceneEntry* findEntry(const std::vector<ZoneSceneEntry>& entries, int zoneId);
};

} // namespace bbfx
