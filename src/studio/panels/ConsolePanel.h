#pragma once

#include <string>
#include <vector>
#include <deque>
#include <sol/sol.hpp>

namespace bbfx {

class ConsolePanel {
public:
    explicit ConsolePanel(sol::state& lua);
    void render();
    void addLog(const std::string& text);

private:
    void executeCommand(const std::string& cmd);
    std::vector<std::string> getCompletions(const std::string& prefix) const;

    sol::state& mLua;
    char mInputBuf[1024] = {};
    std::deque<std::string> mInputHistory;
    std::vector<std::string> mOutput;
    int mHistoryPos = -1;
    bool mScrollToBottom = false;
    static constexpr size_t kMaxHistory = 100;
    static constexpr size_t kMaxOutput = 500;
};

} // namespace bbfx
