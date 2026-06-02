#include "LearnBindingManager.h"

namespace bbfx {

LearnBindingManager& LearnBindingManager::instance() {
    static LearnBindingManager s;
    return s;
}

void LearnBindingManager::addBinding(const Binding& b) {
    mBindings.push_back(b);
}

bool LearnBindingManager::removeBindingAt(size_t index) {
    if (index >= mBindings.size()) return false;
    mBindings.erase(mBindings.begin() + index);
    return true;
}

void LearnBindingManager::clearAll() {
    mBindings.clear();
}

float LearnBindingManager::applyTransform(const Binding& b, float rawValue) {
    float v = rawValue * b.scale + b.offset;
    if (b.invert) v = (1.0f - v);
    return v;
}

int LearnBindingManager::findBindingIndex(SourceType type, int sourceId) const {
    for (size_t i = 0; i < mBindings.size(); ++i) {
        if (mBindings[i].sourceType == type && mBindings[i].sourceId == sourceId)
            return static_cast<int>(i);
    }
    return -1;
}

nlohmann::json LearnBindingManager::toJson() const {
    nlohmann::json j;
    j["version"] = 1;
    j["bindings"] = nlohmann::json::array();
    for (auto& b : mBindings) {
        nlohmann::json e;
        e["port_path"]   = b.portPath;
        e["source_type"] = static_cast<int>(b.sourceType);
        e["source_id"]   = b.sourceId;
        e["scale"]       = b.scale;
        e["offset"]      = b.offset;
        e["invert"]      = b.invert;
        j["bindings"].push_back(e);
    }
    return j;
}

void LearnBindingManager::fromJson(const nlohmann::json& j) {
    mBindings.clear();
    if (!j.is_object()) return;
    if (!j.contains("bindings") || !j["bindings"].is_array()) return;
    for (auto& e : j["bindings"]) {
        Binding b;
        b.portPath   = e.value("port_path", "");
        b.sourceType = static_cast<SourceType>(e.value("source_type", 0));
        b.sourceId   = e.value("source_id", 0);
        b.scale      = e.value("scale", 1.0f);
        b.offset     = e.value("offset", 0.0f);
        b.invert     = e.value("invert", false);
        mBindings.push_back(b);
    }
}

} // namespace bbfx
