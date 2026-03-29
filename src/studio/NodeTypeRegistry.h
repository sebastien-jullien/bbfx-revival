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
};

class NodeTypeRegistry {
public:
    static NodeTypeRegistry& instance();

    void registerType(const NodeTypeInfo& info);
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
