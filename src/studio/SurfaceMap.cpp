#include "SurfaceMap.h"
#include <OgreSceneManager.h>
#include <OgreCamera.h>
#include <OgreSceneNode.h>
#include <iostream>
#include <algorithm>

namespace bbfx {

// ── Zone JSON ─────────────────────────────────────────────────────────────────

nlohmann::json Zone::toJson() const {
    return {
        {"id",           id},
        {"name",         name},
        {"x",            x},
        {"y",            y},
        {"width",        width},
        {"height",       height},
        {"outputSlotId", outputSlotId},
        {"customCamera", customCamera},
        {"cameraName",   cameraName},
        {"warpEnabled",  warpEnabled},
        {"warp",         warpProfile.toJson()},
        {"blendEnabled", blendEnabled},
        {"blend",        blendProfile.toJson()}
    };
}

void Zone::fromJson(const nlohmann::json& j) {
    if (j.contains("id"))           id           = j["id"].get<int>();
    if (j.contains("name"))         name         = j["name"].get<std::string>();
    if (j.contains("x"))            x            = j["x"].get<float>();
    if (j.contains("y"))            y            = j["y"].get<float>();
    if (j.contains("width"))        width        = j["width"].get<float>();
    if (j.contains("height"))       height       = j["height"].get<float>();
    if (j.contains("outputSlotId")) outputSlotId = j["outputSlotId"].get<int>();
    if (j.contains("customCamera")) customCamera = j["customCamera"].get<bool>();
    if (j.contains("cameraName"))   cameraName   = j["cameraName"].get<std::string>();
    if (j.contains("warpEnabled"))  warpEnabled  = j["warpEnabled"].get<bool>();
    if (j.contains("blendEnabled")) blendEnabled = j["blendEnabled"].get<bool>();
    if (j.contains("warp"))         warpProfile.fromJson(j["warp"]);
    if (j.contains("blend"))        blendProfile.fromJson(j["blend"]);
}

// ── SurfaceMap CRUD ───────────────────────────────────────────────────────────

int SurfaceMap::addZone(const std::string& name, float x, float y, float w, float h) {
    Zone zone;
    zone.id     = mNextZoneId++;
    zone.name   = name.empty() ? ("Zone " + std::to_string(zone.id)) : name;
    zone.x      = x;
    zone.y      = y;
    zone.width  = w;
    zone.height = h;
    int id = zone.id;
    mZones.push_back(std::move(zone));
    std::cout << "[SurfaceMap] Zone " << id << " \"" << mZones.back().name << "\" added." << std::endl;
    return id;
}

void SurfaceMap::removeZone(int id) {
    auto it = std::find_if(mZones.begin(), mZones.end(),
                           [id](const Zone& z) { return z.id == id; });
    if (it != mZones.end()) {
        mZones.erase(it);
        std::cout << "[SurfaceMap] Zone " << id << " removed." << std::endl;
    }
}

void SurfaceMap::clear() {
    mZones.clear();
    std::cout << "[SurfaceMap] All zones cleared." << std::endl;
}

Zone* SurfaceMap::getZone(int id) {
    for (auto& z : mZones) if (z.id == id) return &z;
    return nullptr;
}

const Zone* SurfaceMap::getZone(int id) const {
    for (const auto& z : mZones) if (z.id == id) return &z;
    return nullptr;
}

// ── Camera management ─────────────────────────────────────────────────────────

/// Helper: create a scene node + camera pair, copy transforms from MainCamera if available.
static void createZoneCameraInternal(const std::string& camName, int zoneId,
                                     Ogre::SceneManager* sm) {
    // Scene node name mirrors the camera.
    std::string nodeName = camName + "_Node";
    Ogre::SceneNode* node = nullptr;
    try {
        node = sm->getRootSceneNode()->createChildSceneNode(nodeName);
    } catch (...) {
        // Node might already exist — try to reuse.
        if (sm->hasSceneNode(nodeName)) {
            node = sm->getSceneNode(nodeName);
        } else {
            return;
        }
    }

    Ogre::Camera* cam = sm->createCamera(camName);
    cam->setNearClipDistance(0.1f);
    cam->setFarClipDistance(1000.f);
    cam->setAutoAspectRatio(true);

    // Copy transform from MainCamera's scene node.
    if (sm->hasCamera("MainCamera")) {
        auto* main = sm->getCamera("MainCamera");
        auto* mainNode = main->getParentSceneNode();
        if (mainNode) {
            node->setPosition(mainNode->getPosition());
            node->setOrientation(mainNode->getOrientation());
        } else {
            node->setPosition(0.f, 0.f, 10.f);
        }
        cam->setFOVy(main->getFOVy());
        cam->setNearClipDistance(main->getNearClipDistance());
        cam->setFarClipDistance(main->getFarClipDistance());
    } else {
        node->setPosition(0.f, 0.f, 10.f);
        node->lookAt(Ogre::Vector3::ZERO, Ogre::Node::TS_WORLD);
    }

    node->attachObject(cam);
    std::cout << "[SurfaceMap] Camera \"" << camName << "\" created for zone " << zoneId << std::endl;
}

void SurfaceMap::createCameraForZone(int zoneId, Ogre::SceneManager* sm) {
    if (!sm) return;
    auto* zone = getZone(zoneId);
    if (!zone) return;

    std::string camName = "Zone_" + std::to_string(zoneId) + "_Camera";

    // Destroy existing camera with same name if any.
    destroyCameraForZone(zoneId, sm);

    createZoneCameraInternal(camName, zoneId, sm);

    zone->cameraName   = camName;
    zone->customCamera = true;
}

void SurfaceMap::destroyCameraForZone(int zoneId, Ogre::SceneManager* sm) {
    if (!sm) return;
    auto* zone = getZone(zoneId);
    if (!zone || zone->cameraName.empty()) return;

    std::string nodeName = zone->cameraName + "_Node";
    if (sm->hasCamera(zone->cameraName)) {
        sm->destroyCamera(zone->cameraName);
        std::cout << "[SurfaceMap] Camera \"" << zone->cameraName << "\" destroyed." << std::endl;
    }
    if (sm->hasSceneNode(nodeName)) {
        sm->destroySceneNode(nodeName);
    }
    zone->cameraName.clear();
    zone->customCamera = false;
}

void SurfaceMap::destroyAllCameras(Ogre::SceneManager* sm) {
    if (!sm) return;
    for (auto& zone : mZones) {
        if (!zone.cameraName.empty()) {
            std::string nodeName = zone.cameraName + "_Node";
            if (sm->hasCamera(zone.cameraName)) sm->destroyCamera(zone.cameraName);
            if (sm->hasSceneNode(nodeName))     sm->destroySceneNode(nodeName);
        }
        zone.cameraName.clear();
        zone.customCamera = false;
    }
}

// ── Zone/slot linkage ─────────────────────────────────────────────────────────

void SurfaceMap::assignZoneToSlot(int zoneId, int outputSlotId) {
    auto* zone = getZone(zoneId);
    if (zone) {
        zone->outputSlotId = outputSlotId;
        std::cout << "[SurfaceMap] Zone " << zoneId
                  << " assigned to output " << outputSlotId << std::endl;
    }
}

void SurfaceMap::unassignZone(int zoneId) {
    auto* zone = getZone(zoneId);
    if (zone) {
        zone->outputSlotId = -1;
        std::cout << "[SurfaceMap] Zone " << zoneId << " unassigned." << std::endl;
    }
}

void SurfaceMap::onSlotRemoved(int outputSlotId) {
    for (auto& zone : mZones) {
        if (zone.outputSlotId == outputSlotId) {
            zone.outputSlotId = -1;
            std::cout << "[SurfaceMap] Zone " << zone.id
                      << " unassigned (slot " << outputSlotId << " removed)." << std::endl;
        }
    }
}

// ── JSON serialisation ────────────────────────────────────────────────────────

nlohmann::json SurfaceMap::toJson() const {
    nlohmann::json j;
    j["canvasWidth"]       = canvasWidth;
    j["canvasHeight"]      = canvasHeight;
    j["backgroundImage"]   = backgroundImagePath;
    nlohmann::json zonesArr = nlohmann::json::array();
    for (const auto& z : mZones) zonesArr.push_back(z.toJson());
    j["zones"] = zonesArr;
    return j;
}

void SurfaceMap::fromJson(const nlohmann::json& j, Ogre::SceneManager* sm) {
    clear();
    if (j.contains("canvasWidth"))    canvasWidth       = j["canvasWidth"].get<float>();
    if (j.contains("canvasHeight"))   canvasHeight      = j["canvasHeight"].get<float>();
    if (j.contains("backgroundImage")) backgroundImagePath = j["backgroundImage"].get<std::string>();

    if (!j.contains("zones") || !j["zones"].is_array()) return;

    for (const auto& jz : j["zones"]) {
        Zone zone;
        zone.fromJson(jz);
        // Ensure id is consistent.
        if (zone.id >= mNextZoneId) mNextZoneId = zone.id + 1;
        // Recreate OGRE camera if needed.
        if (zone.customCamera && !zone.cameraName.empty() && sm) {
            if (!sm->hasCamera(zone.cameraName)) {
                createZoneCameraInternal(zone.cameraName, zone.id, sm);
            }
        }
        mZones.push_back(std::move(zone));
    }
    std::cout << "[SurfaceMap] Loaded " << mZones.size() << " zones." << std::endl;
}

} // namespace bbfx
