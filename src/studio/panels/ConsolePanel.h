#pragma once

#include <string>
#include <vector>
#include <deque>
#include <sol/sol.hpp>

namespace bbfx {

class ConsolePanel {
public:
    explicit ConsolePanel(sol::state& lua);
    ~ConsolePanel();
    void render();
    void addLog(const std::string& text);

private:
    void executeCommand(const std::string& cmd);
    std::vector<std::string> getCompletions(const std::string& prefix) const;
    void loadPersistentHistory();
    void appendPersistentHistory(const std::string& cmd);
    static bool containsSecret(const std::string& cmd);
    static std::string getHistoryPath();

    sol::state& mLua;
    char mInputBuf[1024] = {};
    char mMultiBuf[8192] = {};                   ///< multi-line buffer (do...end blocks)
    bool mMultiMode = false;                     ///< toggle single-line / multi-line REPL
    std::deque<std::string> mInputHistory;
    std::vector<std::string> mOutput;
    int mHistoryPos = -1;
    bool mScrollToBottom = false;
    bool mFocusInputNextFrame = false;
    static constexpr size_t kMaxHistory = 1000;  ///< raised from 100 (v3.5.2 spec)
    static constexpr size_t kMaxOutput = 500;
};

} // namespace bbfx
