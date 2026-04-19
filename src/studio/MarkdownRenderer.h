#pragma once

#include <string>

namespace bbfx {

/// v3.5 Lot H — minimal Markdown renderer for ImGui.
///
/// Supports the subset needed by plugin README / changelog displays:
///   - Headers  `#`, `##`, `###`            (up to h3)
///   - Bold     `**text**`
///   - Italic   `*text*` or `_text_`
///   - Inline code `` `code` ``
///   - Horizontal rules `---`
///   - Bullet lists `- item` / `* item`
///   - Numbered lists `1. item`
///   - Blockquotes `> quote`
///   - Links `[label](url)`  -> rendered as blue text; click opens in browser
///   - Code fences ``` ``` (rendered verbatim in a child region)
///
/// Anything else is passed through as plain text with word-wrap.
///
/// The renderer is intentionally stateless — call `draw(markdown)` inside
/// whatever ImGui window you are building. It does not push its own
/// ImGui::Begin/End.
class MarkdownRenderer {
public:
    /// Draw the given markdown text into the current ImGui region.
    static void draw(const std::string& markdown);
};

} // namespace bbfx
