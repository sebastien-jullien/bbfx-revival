#pragma once
#include <string>
#include <vector>
#include <imgui.h>

namespace bbfx {

enum class ToastSeverity { Info, Warning, Error };

/// Non-intrusive toast notification system. Renders stacked at bottom-right.
class ToastSystem {
public:
    static ToastSystem& instance();

    void toast(const std::string& message,
               ToastSeverity severity = ToastSeverity::Info,
               float durationSec = 3.0f);

    /// Call each frame in the main render loop.
    void render();

private:
    ToastSystem() = default;

    struct Toast {
        std::string message;
        ToastSeverity severity;
        float remaining;  // seconds left
        float total;      // total duration
    };
    std::vector<Toast> mToasts;
    static constexpr int kMaxToasts = 3;
};

} // namespace bbfx
