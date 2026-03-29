#include "ConsolePanel.h"
#include "../../core/Animator.h"
#include <imgui.h>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif

namespace bbfx {

ConsolePanel::ConsolePanel(sol::state& lua) : mLua(lua) {
    addLog("BBFx Studio Console v3.1");
    addLog("Type Lua expressions. Tab for autocompletion. help() for commands.");

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

    // Input field
    ImGui::Separator();
    bool reclaim = false;
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                ImGuiInputTextFlags_EscapeClearsAll;
    if (ImGui::InputText("##console_input", mInputBuf, sizeof(mInputBuf), flags)) {
        std::string cmd(mInputBuf);
        if (!cmd.empty()) {
            addLog("bbfx> " + cmd);
            executeCommand(cmd);
            mInputHistory.push_back(cmd);
            if (mInputHistory.size() > kMaxHistory) mInputHistory.pop_front();
            mHistoryPos = -1;
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

} // namespace bbfx
