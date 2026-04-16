#include "WarpProfile.h"

namespace bbfx {

nlohmann::json WarpProfile::toJson() const {
    return {
        {"tl_x", corners[0]}, {"tl_y", corners[1]},
        {"tr_x", corners[2]}, {"tr_y", corners[3]},
        {"bl_x", corners[4]}, {"bl_y", corners[5]},
        {"br_x", corners[6]}, {"br_y", corners[7]}
    };
}

void WarpProfile::fromJson(const nlohmann::json& j) {
    corners[0] = j.value("tl_x", 0.f);
    corners[1] = j.value("tl_y", 0.f);
    corners[2] = j.value("tr_x", 1.f);
    corners[3] = j.value("tr_y", 0.f);
    corners[4] = j.value("bl_x", 0.f);
    corners[5] = j.value("bl_y", 1.f);
    corners[6] = j.value("br_x", 1.f);
    corners[7] = j.value("br_y", 1.f);
}

} // namespace bbfx
