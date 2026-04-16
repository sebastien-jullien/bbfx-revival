#include "BlendProfile.h"

namespace bbfx {

nlohmann::json BlendProfile::toJson() const {
    return {
        {"left",   left},
        {"right",  right},
        {"top",    top},
        {"bottom", bottom},
        {"gamma",  gamma}
    };
}

void BlendProfile::fromJson(const nlohmann::json& j) {
    left   = j.value("left",   0.0f);
    right  = j.value("right",  0.0f);
    top    = j.value("top",    0.0f);
    bottom = j.value("bottom", 0.0f);
    gamma  = j.value("gamma",  2.2f);
}

} // namespace bbfx
