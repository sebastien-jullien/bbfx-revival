#pragma once
#include <nlohmann/json.hpp>
#include <cmath>

namespace bbfx {

/// 4-corner quad warp profile for keystone / projector calibration.
/// corners[8] = { tl_x, tl_y,  tr_x, tr_y,  bl_x, bl_y,  br_x, br_y }
/// Identity :    TL=(0,0)      TR=(1,0)       BL=(0,1)       BR=(1,1)
struct WarpProfile {
    float corners[8];

    WarpProfile() { reset(); }

    void reset() {
        corners[0] = 0.f; corners[1] = 0.f; // TL
        corners[2] = 1.f; corners[3] = 0.f; // TR
        corners[4] = 0.f; corners[5] = 1.f; // BL
        corners[6] = 1.f; corners[7] = 1.f; // BR
    }

    bool isIdentity(float tol = 1e-6f) const {
        static const float kId[8] = {0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f, 1.f};
        for (int i = 0; i < 8; ++i)
            if (std::fabs(corners[i] - kId[i]) > tol) return false;
        return true;
    }

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

} // namespace bbfx
