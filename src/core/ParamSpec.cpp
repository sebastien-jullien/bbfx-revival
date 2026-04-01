#include "ParamSpec.h"

namespace bbfx {

void ParamSpec::addParam(const ParamDef& def) {
    mParams.push_back(def);
}

ParamDef* ParamSpec::getParam(const std::string& name) {
    for (auto& p : mParams) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

const ParamDef* ParamSpec::getParam(const std::string& name) const {
    for (auto& p : mParams) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

static std::string paramTypeToString(ParamType t) {
    switch (t) {
        case ParamType::FLOAT:      return "float";
        case ParamType::INT:        return "int";
        case ParamType::BOOL:       return "bool";
        case ParamType::STRING:     return "string";
        case ParamType::ENUM:       return "enum";
        case ParamType::COLOR:      return "color";
        case ParamType::VEC3:       return "vec3";
        case ParamType::MESH:       return "mesh";
        case ParamType::TEXTURE:    return "texture";
        case ParamType::MATERIAL:   return "material";
        case ParamType::SHADER:     return "shader";
        case ParamType::PARTICLE:   return "particle";
        case ParamType::COMPOSITOR: return "compositor";
    }
    return "float";
}

static ParamType paramTypeFromString(const std::string& s) {
    if (s == "float")      return ParamType::FLOAT;
    if (s == "int")        return ParamType::INT;
    if (s == "bool")       return ParamType::BOOL;
    if (s == "string")     return ParamType::STRING;
    if (s == "enum")       return ParamType::ENUM;
    if (s == "color")      return ParamType::COLOR;
    if (s == "vec3")       return ParamType::VEC3;
    if (s == "mesh")       return ParamType::MESH;
    if (s == "texture")    return ParamType::TEXTURE;
    if (s == "material")   return ParamType::MATERIAL;
    if (s == "shader")     return ParamType::SHADER;
    if (s == "particle")   return ParamType::PARTICLE;
    if (s == "compositor") return ParamType::COMPOSITOR;
    return ParamType::FLOAT;
}

nlohmann::json ParamSpec::toJson() const {
    nlohmann::json j = nlohmann::json::object();
    for (auto& p : mParams) {
        switch (p.type) {
            case ParamType::FLOAT:
                j[p.name] = p.floatVal;
                break;
            case ParamType::INT:
                j[p.name] = p.intVal;
                break;
            case ParamType::BOOL:
                j[p.name] = p.boolVal;
                break;
            case ParamType::STRING:
            case ParamType::ENUM:
            case ParamType::MESH:
            case ParamType::TEXTURE:
            case ParamType::MATERIAL:
            case ParamType::SHADER:
            case ParamType::PARTICLE:
            case ParamType::COMPOSITOR:
                j[p.name] = p.stringVal;
                break;
            case ParamType::COLOR:
                j[p.name] = {p.colorVal[0], p.colorVal[1], p.colorVal[2], p.colorVal[3]};
                break;
            case ParamType::VEC3:
                j[p.name] = {p.vec3Val[0], p.vec3Val[1], p.vec3Val[2]};
                break;
        }
    }
    return j;
}

void ParamSpec::fromJson(const nlohmann::json& j) {
    for (auto& p : mParams) {
        if (!j.contains(p.name)) continue;
        const auto& v = j[p.name];
        switch (p.type) {
            case ParamType::FLOAT:
                if (v.is_number()) p.floatVal = v.get<float>();
                break;
            case ParamType::INT:
                if (v.is_number()) p.intVal = v.get<int>();
                break;
            case ParamType::BOOL:
                if (v.is_boolean()) p.boolVal = v.get<bool>();
                break;
            case ParamType::STRING:
            case ParamType::ENUM:
            case ParamType::MESH:
            case ParamType::TEXTURE:
            case ParamType::MATERIAL:
            case ParamType::SHADER:
            case ParamType::PARTICLE:
            case ParamType::COMPOSITOR:
                if (v.is_string()) p.stringVal = v.get<std::string>();
                break;
            case ParamType::COLOR:
                if (v.is_array() && v.size() >= 3) {
                    p.colorVal[0] = v[0].get<float>();
                    p.colorVal[1] = v[1].get<float>();
                    p.colorVal[2] = v[2].get<float>();
                    if (v.size() >= 4) p.colorVal[3] = v[3].get<float>();
                }
                break;
            case ParamType::VEC3:
                if (v.is_array() && v.size() >= 3) {
                    p.vec3Val[0] = v[0].get<float>();
                    p.vec3Val[1] = v[1].get<float>();
                    p.vec3Val[2] = v[2].get<float>();
                }
                break;
        }
    }
}

} // namespace bbfx
