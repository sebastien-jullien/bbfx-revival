#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace bbfx {

enum class ParamType {
    FLOAT, INT, BOOL, STRING, ENUM, COLOR, VEC3,
    MESH, TEXTURE, MATERIAL, SHADER, PARTICLE, COMPOSITOR
};

struct ParamDef {
    std::string name;
    std::string label;      // display label (defaults to name if empty)
    ParamType type = ParamType::FLOAT;

    // Current values
    float floatVal = 0.0f;
    int intVal = 0;
    bool boolVal = false;
    std::string stringVal;
    float colorVal[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float vec3Val[3] = {0.0f, 0.0f, 0.0f};

    // Constraints
    float minVal = 0.0f;
    float maxVal = 1.0f;
    float stepVal = 0.01f;
    std::vector<std::string> choices;  // for ENUM, MESH, TEXTURE, etc.

    const std::string& displayLabel() const { return label.empty() ? name : label; }
};

class ParamSpec {
public:
    void addParam(const ParamDef& def);
    ParamDef* getParam(const std::string& name);
    const ParamDef* getParam(const std::string& name) const;
    const std::vector<ParamDef>& getParams() const { return mParams; }
    bool empty() const { return mParams.empty(); }

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

private:
    std::vector<ParamDef> mParams;
};

} // namespace bbfx
