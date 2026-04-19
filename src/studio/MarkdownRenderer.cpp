#include "MarkdownRenderer.h"

#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include <imgui.h>

namespace bbfx {

namespace {

void openUrl(const std::string& url) {
    if (url.empty()) return;
#ifdef _WIN32
    std::string cmd = "start \"\" \"" + url + "\"";
#else
    std::string cmd = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
#endif
    std::system(cmd.c_str());
}

// Split a markdown line into inline tokens + render them.
// Tokens recognised, in priority order:
//   `code`      -> monospace
//   [label](url)-> clickable link
//   **bold**
//   *italic*    (also _italic_)
// Anything else is plain text. Implementation is deliberately forgiving —
// unmatched backticks/asterisks are passed through as literals.
void renderInline(const std::string& line) {
    const char* p = line.c_str();
    const char* end = p + line.size();

    std::string buf;
    auto flushText = [&]() {
        if (!buf.empty()) {
            ImGui::TextUnformatted(buf.c_str());
            ImGui::SameLine(0.0f, 0.0f);
            buf.clear();
        }
    };

    while (p < end) {
        char c = *p;

        // Inline code `code`
        if (c == '`') {
            const char* close = strchr(p + 1, '`');
            if (close && close < end) {
                flushText();
                std::string code(p + 1, close);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.65f, 1.0f));
                ImGui::Text("%s", code.c_str());
                ImGui::PopStyleColor();
                ImGui::SameLine(0.0f, 0.0f);
                p = close + 1;
                continue;
            }
        }

        // Link [label](url)
        if (c == '[') {
            const char* endLabel = strchr(p + 1, ']');
            if (endLabel && endLabel + 1 < end && endLabel[1] == '(') {
                const char* endUrl = strchr(endLabel + 2, ')');
                if (endUrl) {
                    flushText();
                    std::string label(p + 1, endLabel);
                    std::string url(endLabel + 2, endUrl);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.75f, 1.0f, 1.0f));
                    ImGui::TextUnformatted(label.c_str());
                    ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", url.c_str());
                    if (ImGui::IsItemClicked()) openUrl(url);
                    ImGui::SameLine(0.0f, 0.0f);
                    p = endUrl + 1;
                    continue;
                }
            }
        }

        // Bold **text**
        if (c == '*' && p + 1 < end && p[1] == '*') {
            const char* close = strstr(p + 2, "**");
            if (close && close < end) {
                flushText();
                std::string boldTxt(p + 2, close);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                // ImGui doesn't have native bold fonts by default — use
                // brighter white as an approximation.
                ImGui::TextUnformatted(boldTxt.c_str());
                ImGui::PopStyleColor();
                ImGui::SameLine(0.0f, 0.0f);
                p = close + 2;
                continue;
            }
        }

        // Italic *text* or _text_
        if ((c == '*' || c == '_') &&
            p + 1 < end && p[1] != c && p[1] != ' ') {
            const char* close = nullptr;
            for (const char* q = p + 1; q < end; ++q) {
                if (*q == c) { close = q; break; }
            }
            if (close && close < end) {
                flushText();
                std::string italicTxt(p + 1, close);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                ImGui::TextUnformatted(italicTxt.c_str());
                ImGui::PopStyleColor();
                ImGui::SameLine(0.0f, 0.0f);
                p = close + 1;
                continue;
            }
        }

        buf += c;
        ++p;
    }
    flushText();
    // Finish with a newline (SameLine was accumulating).
    ImGui::NewLine();
}

} // anonymous

void MarkdownRenderer::draw(const std::string& markdown) {
    std::istringstream iss(markdown);
    std::string line;
    bool inCode = false;
    std::string codeBuf;

    while (std::getline(iss, line)) {
        // Strip trailing CR from Windows line endings.
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Code fence toggle
        if (line.rfind("```", 0) == 0) {
            if (inCode) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
                ImGui::BeginChild(ImGui::GetID(("##md_code_" + std::to_string(codeBuf.size())).c_str()),
                                  ImVec2(0, 0), true);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.65f, 1.0f));
                ImGui::TextUnformatted(codeBuf.c_str());
                ImGui::PopStyleColor();
                ImGui::EndChild();
                ImGui::PopStyleColor();
                inCode = false;
                codeBuf.clear();
            } else {
                inCode = true;
                codeBuf.clear();
            }
            continue;
        }
        if (inCode) {
            codeBuf += line;
            codeBuf.push_back('\n');
            continue;
        }

        // Horizontal rule
        if (line == "---" || line == "***" || line == "___") {
            ImGui::Separator();
            continue;
        }

        // Headers
        if (line.rfind("### ", 0) == 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.80f, 1.0f));
            ImGui::Text("%s", line.substr(4).c_str());
            ImGui::PopStyleColor();
            continue;
        }
        if (line.rfind("## ", 0) == 0) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.5f, 1.0f));
            ImGui::Text("%s", line.substr(3).c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();
            continue;
        }
        if (line.rfind("# ", 0) == 0) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.35f, 1.0f));
            ImGui::Text("%s", line.substr(2).c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();
            continue;
        }

        // Blockquote
        if (line.rfind("> ", 0) == 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.9f, 1.0f));
            ImGui::Text("  |  %s", line.substr(2).c_str());
            ImGui::PopStyleColor();
            continue;
        }

        // Bullet list
        if (line.rfind("- ", 0) == 0 || line.rfind("* ", 0) == 0) {
            ImGui::Bullet();
            renderInline(line.substr(2));
            continue;
        }

        // Numbered list `1. item` — detect digit(s) + ". "
        {
            size_t i = 0;
            while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) ++i;
            if (i > 0 && i + 1 < line.size() && line[i] == '.' && line[i+1] == ' ') {
                ImGui::TextDisabled("%s.", line.substr(0, i).c_str());
                ImGui::SameLine();
                renderInline(line.substr(i + 2));
                continue;
            }
        }

        // Empty line -> spacing
        if (line.empty()) { ImGui::Spacing(); continue; }

        // Default: inline render with wrap
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
        renderInline(line);
        ImGui::PopTextWrapPos();
    }

    // Unterminated code fence? dump what we have.
    if (inCode && !codeBuf.empty()) {
        ImGui::TextUnformatted(codeBuf.c_str());
    }
}

} // namespace bbfx
