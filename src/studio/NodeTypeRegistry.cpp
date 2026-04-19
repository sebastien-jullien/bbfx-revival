#include "NodeTypeRegistry.h"

#include "../plugin/PluginRegistry.h"

namespace bbfx {

NodeTypeRegistry& NodeTypeRegistry::instance() {
    static NodeTypeRegistry reg;
    return reg;
}

void NodeTypeRegistry::registerType(const NodeTypeInfo& info) {
    mTypes[info.typeName] = info;
}

void NodeTypeRegistry::registerTypeFromPlugin(const std::string& pluginId,
                                               const NodeTypeInfo& info) {
    NodeTypeInfo copy = info;
    copy.pluginId = pluginId;
    mTypes[copy.typeName] = std::move(copy);
    PluginRegistry::instance().trackNodeType(pluginId, info.typeName);
}

void NodeTypeRegistry::unregisterByPlugin(const std::string& pluginId) {
    // Collect first to avoid iterator invalidation.
    std::vector<std::string> toErase;
    for (const auto& [name, info] : mTypes) {
        if (info.pluginId == pluginId) toErase.push_back(name);
    }
    for (const auto& n : toErase) {
        mTypes.erase(n);
    }
}

bool NodeTypeRegistry::unregisterType(const std::string& typeName) {
    return mTypes.erase(typeName) > 0;
}

const NodeTypeInfo* NodeTypeRegistry::getType(const std::string& typeName) const {
    auto it = mTypes.find(typeName);
    return it != mTypes.end() ? &it->second : nullptr;
}

std::map<std::string, std::vector<const NodeTypeInfo*>> NodeTypeRegistry::getByCategory() const {
    std::map<std::string, std::vector<const NodeTypeInfo*>> result;
    for (auto& [name, info] : mTypes) {
        result[info.category].push_back(&info);
    }
    return result;
}

std::vector<std::string> NodeTypeRegistry::getTypeNames() const {
    std::vector<std::string> names;
    names.reserve(mTypes.size());
    for (auto& [name, info] : mTypes) {
        names.push_back(name);
    }
    return names;
}

AnimationNode* NodeTypeRegistry::create(const std::string& typeName,
                                         const std::string& nodeName,
                                         sol::state& lua) {
    auto it = mTypes.find(typeName);
    if (it == mTypes.end()) return nullptr;
    if (!it->second.factory) return nullptr;
    return it->second.factory(nodeName, lua);
}

} // namespace bbfx
