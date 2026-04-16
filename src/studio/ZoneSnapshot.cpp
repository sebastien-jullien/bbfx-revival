#include "ZoneSnapshot.h"
#include "SurfaceMap.h"
#include "OutputManager.h"
#include <OgreCamera.h>
#include <OgreSceneNode.h>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace bbfx {

// ── ZoneSceneEntry JSON ───────────────────────────────────────────────────────

nlohmann::json ZoneSceneEntry::toJson() const {
    return {
        {"zoneId",       zoneId},
        {"warp",         warp.toJson()},
        {"warpEnabled",  warpEnabled},
        {"blend",        blend.toJson()},
        {"blendEnabled", blendEnabled}
    };
}

void ZoneSceneEntry::fromJson(const nlohmann::json& j) {
    zoneId      = j.value("zoneId", -1);
    warpEnabled = j.value("warpEnabled", false);
    blendEnabled = j.value("blendEnabled", false);
    if (j.contains("warp"))  warp.fromJson(j["warp"]);
    if (j.contains("blend")) blend.fromJson(j["blend"]);
}

// ── ZoneSnapshot ──────────────────────────────────────────────────────────────

ZoneSnapshot::ZoneSnapshot() = default;

void ZoneSnapshot::capture(const SurfaceMap& map, const Ogre::Camera* cam) {
    mEntries.clear();
    for (const auto& zone : map.getAllZones()) {
        ZoneSceneEntry entry;
        entry.zoneId      = zone.id;
        entry.warp        = zone.warpProfile;
        entry.warpEnabled = zone.warpEnabled;
        entry.blend       = zone.blendProfile;
        entry.blendEnabled = zone.blendEnabled;
        mEntries.push_back(entry);
    }

    if (cam) {
        auto pos = cam->getDerivedPosition();
        mCamPosX = pos.x;
        mCamPosY = pos.y;
        mCamPosZ = pos.z;
        mCamFov  = cam->getFOVy().valueDegrees();
    }
}

// ── Lerp helpers ──────────────────────────────────────────────────────────────

static float lerp(float a, float b, float t) { return a * (1.f - t) + b * t; }

static float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }

static WarpProfile lerpWarp(const WarpProfile& a, const WarpProfile& b, float t) {
    WarpProfile r;
    for (int i = 0; i < 8; ++i)
        r.corners[i] = lerp(a.corners[i], b.corners[i], t);
    return r;
}

static BlendProfile lerpBlend(const BlendProfile& a, const BlendProfile& b, float t) {
    BlendProfile r;
    r.left   = clampf(lerp(a.left,   b.left,   t), 0.f, 0.5f);
    r.right  = clampf(lerp(a.right,  b.right,  t), 0.f, 0.5f);
    r.top    = clampf(lerp(a.top,    b.top,    t), 0.f, 0.5f);
    r.bottom = clampf(lerp(a.bottom, b.bottom, t), 0.f, 0.5f);
    r.gamma  = clampf(lerp(a.gamma,  b.gamma,  t), 1.f, 3.f);
    return r;
}

// ── apply ─────────────────────────────────────────────────────────────────────

const ZoneSceneEntry* ZoneSnapshot::findEntry(const std::vector<ZoneSceneEntry>& entries, int zoneId) {
    for (const auto& e : entries)
        if (e.zoneId == zoneId) return &e;
    return nullptr;
}

void ZoneSnapshot::apply(const ZoneSnapshot& a, const ZoneSnapshot& b, float t,
                         SurfaceMap& map, OutputManager& mgr, Ogre::Camera* cam)
{
    // Optimisation: t == 0 → apply A directly, t == 1 → apply B directly
    const ZoneSnapshot& src = (t <= 0.f) ? a : (t >= 1.f) ? b : a; // just for camera shortcut
    float tClamped = clampf(t, 0.f, 1.f);

    // Collect all unique zone IDs from both snapshots
    std::vector<int> zoneIds;
    for (const auto& e : a.mEntries) zoneIds.push_back(e.zoneId);
    for (const auto& e : b.mEntries) {
        bool found = false;
        for (int id : zoneIds) if (id == e.zoneId) { found = true; break; }
        if (!found) zoneIds.push_back(e.zoneId);
    }

    for (int zoneId : zoneIds) {
        const ZoneSceneEntry* ea = findEntry(a.mEntries, zoneId);
        const ZoneSceneEntry* eb = findEntry(b.mEntries, zoneId);

        WarpProfile  wp;
        bool         warpEn  = false;
        BlendProfile bp;
        bool         blendEn = false;

        if (ea && eb) {
            // Both present: lerp
            if (tClamped <= 0.f) {
                wp = ea->warp; warpEn = ea->warpEnabled;
                bp = ea->blend; blendEn = ea->blendEnabled;
            } else if (tClamped >= 1.f) {
                wp = eb->warp; warpEn = eb->warpEnabled;
                bp = eb->blend; blendEn = eb->blendEnabled;
            } else {
                wp = lerpWarp(ea->warp, eb->warp, tClamped);
                warpEn = (tClamped < 0.5f) ? ea->warpEnabled : eb->warpEnabled;
                bp = lerpBlend(ea->blend, eb->blend, tClamped);
                blendEn = (tClamped < 0.5f) ? ea->blendEnabled : eb->blendEnabled;
            }
        } else if (ea) {
            // Only in A: keep A
            wp = ea->warp; warpEn = ea->warpEnabled;
            bp = ea->blend; blendEn = ea->blendEnabled;
        } else if (eb) {
            // Only in B: keep B
            wp = eb->warp; warpEn = eb->warpEnabled;
            bp = eb->blend; blendEn = eb->blendEnabled;
        }

        // Push to SurfaceMap zone
        auto* zone = map.getZone(zoneId);
        if (zone) {
            zone->warpProfile  = wp;
            zone->warpEnabled  = warpEn;
            zone->blendProfile = bp;
            zone->blendEnabled = blendEn;

            // If zone has an assigned output slot, push to OutputManager via applyZoneWarpBlend
            if (zone->outputSlotId >= 0) {
                mgr.applyZoneWarpBlend(zone->outputSlotId, wp, warpEn, bp, blendEn);
            }
        }
    }

    // Camera interpolation
    if (cam) {
        float cx, cy, cz, cfov;
        if (tClamped <= 0.f) {
            cx = a.mCamPosX; cy = a.mCamPosY; cz = a.mCamPosZ; cfov = a.mCamFov;
        } else if (tClamped >= 1.f) {
            cx = b.mCamPosX; cy = b.mCamPosY; cz = b.mCamPosZ; cfov = b.mCamFov;
        } else {
            cx = lerp(a.mCamPosX, b.mCamPosX, tClamped);
            cy = lerp(a.mCamPosY, b.mCamPosY, tClamped);
            cz = lerp(a.mCamPosZ, b.mCamPosZ, tClamped);
            cfov = lerp(a.mCamFov, b.mCamFov, tClamped);
        }
        cfov = clampf(cfov, 5.f, 170.f);

        auto* node = cam->getParentSceneNode();
        if (node) node->setPosition(Ogre::Vector3(cx, cy, cz));
        cam->setFOVy(Ogre::Degree(cfov));
    }
}

// ── JSON ──────────────────────────────────────────────────────────────────────

nlohmann::json ZoneSnapshot::toJson() const {
    nlohmann::json entriesArr = nlohmann::json::array();
    for (const auto& e : mEntries) entriesArr.push_back(e.toJson());
    return {
        {"entries", entriesArr},
        {"camPosX", mCamPosX},
        {"camPosY", mCamPosY},
        {"camPosZ", mCamPosZ},
        {"camFov",  mCamFov}
    };
}

void ZoneSnapshot::fromJson(const nlohmann::json& j) {
    mEntries.clear();
    if (j.contains("entries") && j["entries"].is_array()) {
        for (const auto& ej : j["entries"]) {
            ZoneSceneEntry e;
            e.fromJson(ej);
            mEntries.push_back(e);
        }
    }
    mCamPosX = j.value("camPosX", 0.f);
    mCamPosY = j.value("camPosY", 5.f);
    mCamPosZ = j.value("camPosZ", 10.f);
    mCamFov  = j.value("camFov", 60.f);
}

} // namespace bbfx
