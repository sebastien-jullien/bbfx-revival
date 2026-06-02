#include "ConsolePanel.h"
#include "../../core/Animator.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <regex>
#include <sstream>
#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <shlobj.h>
#endif

namespace bbfx {

ConsolePanel::ConsolePanel(sol::state& lua) : mLua(lua) {
    addLog("BBFx Studio Console v3.5.2 — Lua REPL");
    addLog("Type Lua expressions. Tab=complete, Up/Down=history, [M]=multi-line block.");
    loadPersistentHistory();

    // Register global console commands in Lua
    mLua.set_function("graph", [this]() {
        auto* animator = Animator::instance();
        if (!animator) { addLog("Animator not initialized"); return; }
        addLog("--- DAG Graph ---");
        for (auto& name : animator->getRegisteredNodeNames()) {
            auto* node = animator->getRegisteredNode(name);
            if (!node) continue;
            std::string line = "  " + name + " (" + node->getTypeName() + ")";
            auto& ins = node->getInputs();
            auto& outs = node->getOutputs();
            if (!ins.empty()) {
                line += "  in:";
                for (auto& [pn, p] : ins) line += " " + pn + "=" + std::to_string(p->getValue());
            }
            if (!outs.empty()) {
                line += "  out:";
                for (auto& [pn, p] : outs) line += " " + pn + "=" + std::to_string(p->getValue());
            }
            addLog(line);
        }
        auto links = animator->getLinks();
        addLog("--- Links (" + std::to_string(links.size()) + ") ---");
        for (auto& lk : links) {
            addLog("  " + lk.fromNode + "." + lk.fromPort + " -> " + lk.toNode + "." + lk.toPort);
        }
    });

    mLua.set_function("ports", [this](const std::string& nodeName) {
        auto* animator = Animator::instance();
        if (!animator) return;
        auto* node = animator->getRegisteredNode(nodeName);
        if (!node) { addLog("Node '" + nodeName + "' not found"); return; }
        addLog("--- " + nodeName + " (" + node->getTypeName() + ") ---");
        addLog("Inputs:");
        for (auto& [name, port] : node->getInputs())
            addLog("  " + name + " = " + std::to_string(port->getValue()));
        addLog("Outputs:");
        for (auto& [name, port] : node->getOutputs())
            addLog("  " + name + " = " + std::to_string(port->getValue()));
    });

    mLua.set_function("set", [this](const std::string& nodeName, const std::string& portName, float value) {
        auto* animator = Animator::instance();
        if (!animator) return;
        auto* node = animator->getRegisteredNode(nodeName);
        if (!node) { addLog("Node '" + nodeName + "' not found"); return; }
        auto& inputs = node->getInputs();
        auto it = inputs.find(portName);
        if (it != inputs.end()) {
            it->second->setValue(value);
            addLog(nodeName + "." + portName + " = " + std::to_string(value));
        } else {
            addLog("Port '" + portName + "' not found on '" + nodeName + "'");
        }
    });

    mLua.set_function("help", [this]() {
        addLog("--- BBFx Studio Console ---");
        addLog("  graph()                    List all nodes and links");
        addLog("  ports(\"nodeName\")          Show ports of a node");
        addLog("  set(\"node\", \"port\", val)   Set a port value");
        addLog("  help()                     Show this help");
        addLog("  Any Lua expression         Evaluated and result shown");
    });
}

void ConsolePanel::render() {
    // Set a reasonable default size on first appearance
    ImGui::SetNextWindowSizeConstraints({300, 200}, {2000, 1000});
    static bool firstOpen = true;
    if (firstOpen) {
        ImGui::SetNextWindowSize({600, 300}, ImGuiCond_FirstUseEver);
        firstOpen = false;
    }
    if (!ImGui::Begin("Console")) {
        ImGui::End();
        return;
    }

    // Mode toggle (single-line / multi-line)
    ImGui::Checkbox("Multi-line", &mMultiMode);
    ImGui::SameLine();

    // Copy button + Clear button in toolbar
    if (ImGui::SmallButton("Copy All")) {
        std::string all;
        for (auto& line : mOutput) { all += line + "\n"; }
#ifdef _WIN32
        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, all.size() + 1);
            if (hg) {
                memcpy(GlobalLock(hg), all.c_str(), all.size() + 1);
                GlobalUnlock(hg);
                SetClipboardData(CF_TEXT, hg);
            }
            CloseClipboard();
        }
#endif
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
        mOutput.clear();
    }
    ImGui::Separator();

    // Output area
    float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("##console_output", ImVec2(0, -footerHeight), true);
    for (auto& line : mOutput) {
        bool isError = line.find("error") != std::string::npos ||
                       line.find("Error") != std::string::npos;
        if (isError) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.3f, 0.3f, 1));
        ImGui::TextWrapped("%s", line.c_str());
        if (isError) ImGui::PopStyleColor();
    }
    if (mScrollToBottom) {
        ImGui::SetScrollHereY(1.0f);
        mScrollToBottom = false;
    }
    ImGui::EndChild();

    // ── Input area ───────────────────────────────────────────────────────
    ImGui::Separator();
    bool reclaim = false;

    if (mMultiMode) {
        // Multi-line REPL (do...end blocks etc.)
        ImGui::InputTextMultiline("##console_input_multi", mMultiBuf, sizeof(mMultiBuf),
                                  ImVec2(-1, 100));
        if (ImGui::Button("Execute") || (ImGui::IsKeyDown(ImGuiMod_Shift)
                                        && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
            std::string cmd(mMultiBuf);
            // Trim trailing whitespace
            while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == ' ' || cmd.back() == '\t')) {
                cmd.pop_back();
            }
            if (!cmd.empty()) {
                addLog("bbfx> " + cmd);
                executeCommand(cmd);
                mInputHistory.push_back(cmd);
                if (mInputHistory.size() > kMaxHistory) mInputHistory.pop_front();
                mHistoryPos = -1;
                if (!containsSecret(cmd)) appendPersistentHistory(cmd);
            }
            mMultiBuf[0] = '\0';
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Shift+Enter to execute)");
        ImGui::End();
        return;
    }

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                ImGuiInputTextFlags_EscapeClearsAll |
                                ImGuiInputTextFlags_CallbackHistory;

    auto historyCallback = [](ImGuiInputTextCallbackData* data) -> int {
        auto* panel = static_cast<ConsolePanel*>(data->UserData);
        if (panel->mInputHistory.empty()) return 0;

        if (data->EventKey == ImGuiKey_UpArrow) {
            if (panel->mHistoryPos == -1)
                panel->mHistoryPos = static_cast<int>(panel->mInputHistory.size()) - 1;
            else if (panel->mHistoryPos > 0)
                panel->mHistoryPos--;
        } else if (data->EventKey == ImGuiKey_DownArrow) {
            if (panel->mHistoryPos != -1) {
                panel->mHistoryPos++;
                if (panel->mHistoryPos >= static_cast<int>(panel->mInputHistory.size()))
                    panel->mHistoryPos = -1;
            }
        }

        const char* text = (panel->mHistoryPos >= 0)
            ? panel->mInputHistory[panel->mHistoryPos].c_str()
            : "";
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, text);
        return 0;
    };

    if (ImGui::InputText("##console_input", mInputBuf, sizeof(mInputBuf), flags,
                         historyCallback, this)) {
        std::string cmd(mInputBuf);
        if (!cmd.empty()) {
            addLog("bbfx> " + cmd);
            executeCommand(cmd);
            mInputHistory.push_back(cmd);
            if (mInputHistory.size() > kMaxHistory) mInputHistory.pop_front();
            mHistoryPos = -1;
            if (!containsSecret(cmd)) appendPersistentHistory(cmd);
        }
        mInputBuf[0] = '\0';
        reclaim = true;
    }

    // Tab autocompletion
    if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Tab)) {
        std::string prefix(mInputBuf);
        auto completions = getCompletions(prefix);
        if (completions.size() == 1) {
            strncpy(mInputBuf, completions[0].c_str(), sizeof(mInputBuf) - 1);
        } else if (!completions.empty()) {
            std::string list;
            for (auto& c : completions) list += c + "  ";
            addLog(list);
        }
    }

    // Keep focus on input
    if (reclaim) ImGui::SetKeyboardFocusHere(-1);

    ImGui::End();
}

void ConsolePanel::addLog(const std::string& text) {
    mOutput.push_back(text);
    if (mOutput.size() > kMaxOutput) {
        mOutput.erase(mOutput.begin());
    }
    mScrollToBottom = true;
}

void ConsolePanel::executeCommand(const std::string& cmd) {
    auto result = mLua.safe_script(cmd, sol::script_pass_on_error);
    if (result.valid()) {
        if (result.get_type() != sol::type::none &&
            result.get_type() != sol::type::lua_nil) {
            std::ostringstream oss;
            sol::object obj = result;
            if (obj.is<std::string>()) {
                oss << obj.as<std::string>();
            } else if (obj.is<double>()) {
                oss << obj.as<double>();
            } else if (obj.is<bool>()) {
                oss << (obj.as<bool>() ? "true" : "false");
            } else {
                oss << "[" << sol::type_name(mLua, result.get_type()) << "]";
            }
            addLog("--> " + oss.str());
        }
    } else {
        sol::error err = result;
        addLog("error: " + std::string(err.what()));
    }
}

std::vector<std::string> ConsolePanel::getCompletions(const std::string& prefix) const {
    std::vector<std::string> matches;
    if (prefix.empty()) return matches;
    auto* animator = Animator::instance();
    if (animator) {
        for (auto& name : animator->getRegisteredNodeNames()) {
            if (name.compare(0, prefix.size(), prefix) == 0) {
                matches.push_back(name);
            }
        }
    }
    return matches;
}

// ── v3.5.2: persistent REPL history (~/.bbfx/repl_history.lua) ──────────
std::string ConsolePanel::getHistoryPath() {
    std::string home;
#ifdef _WIN32
    char path[MAX_PATH];
    if (SHGetFolderPathA(nullptr, CSIDL_PROFILE, nullptr, 0, path) == S_OK) {
        home = path;
    } else if (const char* up = std::getenv("USERPROFILE")) {
        home = up;
    }
#else
    if (const char* h = std::getenv("HOME")) home = h;
#endif
    if (home.empty()) home = ".";
    std::string dir = home + "/.bbfx";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir + "/repl_history.lua";
}

bool ConsolePanel::containsSecret(const std::string& cmd) {
    static const std::regex re(
        "password|secret|token|api_key|api-key|apikey",
        std::regex_constants::icase);
    return std::regex_search(cmd, re);
}

void ConsolePanel::loadPersistentHistory() {
    std::ifstream f(getHistoryPath());
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        mInputHistory.push_back(line);
        if (mInputHistory.size() > kMaxHistory) mInputHistory.pop_front();
    }
}

void ConsolePanel::appendPersistentHistory(const std::string& cmd) {
    std::ofstream f(getHistoryPath(), std::ios::app);
    if (!f.is_open()) return;
    // Single-line storage: replace newlines with spaces for portability.
    std::string flat = cmd;
    std::replace(flat.begin(), flat.end(), '\n', ' ');
    std::replace(flat.begin(), flat.end(), '\r', ' ');
    f << flat << '\n';
}

ConsolePanel::~ConsolePanel() = default;

} // namespace bbfx
