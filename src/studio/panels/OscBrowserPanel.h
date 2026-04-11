#pragma once
#include <imgui.h>
#include <string>
#include <vector>

namespace bbfx {

/// Panel showing auto-discovered OSC addresses in a tree view.
class OscBrowserPanel {
public:
    void render();
    void addAddress(const std::string& address);

private:
    std::vector<std::string> mAddresses;
};

} // namespace bbfx
