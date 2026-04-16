#pragma once
#include <nlohmann/json.hpp>

namespace bbfx {

/// Edge blend profile for multi-projector overlap blending.
/// Each value is in [0, 0.5] and represents the fraction of the image width/height
/// used for gamma-corrected edge fade.
struct BlendProfile {
    float left   = 0.0f;  // fraction of width for left edge fade
    float right  = 0.0f;  // fraction of width for right edge fade
    float top    = 0.0f;  // fraction of height for top edge fade
    float bottom = 0.0f;  // fraction of height for bottom edge fade
    float gamma  = 2.2f;  // gamma curve exponent (1.0 = linear, 2.2 = standard)

    void reset() {
        left = right = top = bottom = 0.0f;
        gamma = 2.2f;
    }

    bool isActive() const {
        return left > 0.0f || right > 0.0f || top > 0.0f || bottom > 0.0f;
    }

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

} // namespace bbfx
