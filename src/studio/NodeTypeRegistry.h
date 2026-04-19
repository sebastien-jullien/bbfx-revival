#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <imgui.h>
#include <sol/forward.hpp>

namespace bbfx {

class AnimationNode;

struct NodeTypeInfo {
    std::string typeName;
    std::string category;
    ImVec4 color;
    std::function<AnimationNode*(const std::string& name, sol::state& lua)> factory;
    std::string pluginId;  // empty for builtins; set by registerTypeFromPlugin
};

class NodeTypeRegistry {
public:
    static NodeTypeRegistry& instance();

    void registerType(const NodeTypeInfo& info);

    // Convenience wrapper used by plugin sandboxes: stamps `pluginId` on the
    // registered type and tracks it in PluginRegistry so unregisterByPlugin
    // knows what to tear down.
    void registerTypeFromPlugin(const std::string& pluginId, const NodeTypeInfo& info);

    // Remove every node type that was contributed by `pluginId`. Used by
    // PluginManager at disable/unload to avoid dangling factories referencing
    // destroyed sol::environments.
    void unregisterByPlugin(const std::string& pluginId);

    // Drop a specific type by name. Returns true if the type existed.
    bool unregisterType(const std::string& typeName);

    const NodeTypeInfo* getType(const std::string& typeName) const;
    std::map<std::string, std::vector<const NodeTypeInfo*>> getByCategory() const;
    std::vector<std::string> getTypeNames() const;
    AnimationNode* create(const std::string& typeName, const std::string& nodeName,
                          sol::state& lua);

private:
    NodeTypeRegistry() = default;
    std::map<std::string, NodeTypeInfo> mTypes;
};

} // namespace bbfx
