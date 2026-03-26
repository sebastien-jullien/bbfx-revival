#pragma once
#include <string>
#include <optional>

namespace bbfx {

/// Non-blocking stdin reader for the REPL console.
/// Uses _kbhit()/_getch() on Windows to read characters without blocking.
/// Accumulates into a line buffer, returns the complete line on Enter.
class StdinReader {
public:
    StdinReader() = default;

    /// Poll for a complete line from stdin.
    /// Returns the line (without newline) if Enter was pressed, std::nullopt otherwise.
    /// Call this once per frame.
    std::optional<std::string> poll();

private:
    std::string mBuffer;
};

} // namespace bbfx
