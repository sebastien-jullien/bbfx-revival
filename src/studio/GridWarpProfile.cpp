#include "GridWarpProfile.h"

namespace bbfx {

nlohmann::json GridWarpProfile::toJson() const {
    nlohmann::json j;
    nlohmann::json arr = nlohmann::json::array();
    for (int i = 0; i < CNT * 2; ++i)
        arr.push_back(pts[i]);
    j["pts"] = arr;
    return j;
}

void GridWarpProfile::fromJson(const nlohmann::json& j) {
    reset();
    if (j.contains("pts") && j["pts"].is_array()) {
        auto& arr = j["pts"];
        int cnt = std::min(static_cast<int>(arr.size()), CNT * 2);
        for (int i = 0; i < cnt; ++i)
            pts[i] = arr[i].get<float>();
    }
}

} // namespace bbfx
