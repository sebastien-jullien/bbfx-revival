#pragma once
#include <nlohmann/json.hpp>
#include <cmath>

namespace bbfx {

/// NxN grid warp profile for multi-point projector calibration (v3.4 Lot K).
///
/// The grid stores source UV coordinates for each control point.
/// Identity: point (row, col) maps to (col/(N-1), row/(N-1)).
///
/// Points are stored row-major: pts[row * N + col] = {x, y}.
struct GridWarpProfile {
    static constexpr int N   = 4;          ///< Grid dimension (4x4 = 16 control points)
    static constexpr int CNT = N * N;      ///< Total control point count
    float pts[CNT * 2];                    ///< Flat array: [idx*2+0]=x, [idx*2+1]=y

    GridWarpProfile() { reset(); }

    /// Reset to identity (evenly spaced grid).
    void reset() {
        for (int r = 0; r < N; ++r)
            for (int c = 0; c < N; ++c) {
                pts[(r * N + c) * 2 + 0] = static_cast<float>(c) / static_cast<float>(N - 1);
                pts[(r * N + c) * 2 + 1] = static_cast<float>(r) / static_cast<float>(N - 1);
            }
    }

    /// Returns true if all points are at their identity positions.
    bool isIdentity(float tol = 1e-6f) const {
        GridWarpProfile id;
        for (int i = 0; i < CNT * 2; ++i)
            if (std::fabs(pts[i] - id.pts[i]) > tol) return false;
        return true;
    }

    /// Set a single control point position (row, col) to source UV (x, y).
    void setPoint(int row, int col, float x, float y) {
        pts[(row * N + col) * 2 + 0] = x;
        pts[(row * N + col) * 2 + 1] = y;
    }

    /// Get source UV of control point (row, col).
    float getX(int row, int col) const { return pts[(row * N + col) * 2 + 0]; }
    float getY(int row, int col) const { return pts[(row * N + col) * 2 + 1]; }

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

} // namespace bbfx
