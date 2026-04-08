#include "UndoHistoryPanel.h"
#include "../commands/CommandManager.h"
#include <imgui.h>

namespace bbfx {

void UndoHistoryPanel::render() {
    ImGui::Begin("Undo History");

    auto& cm = CommandManager::instance();
    auto& undoStack = cm.getUndoStack();
    auto& redoStack = cm.getRedoStack();

    int undoSize = static_cast<int>(undoStack.size());
    int redoSize = static_cast<int>(redoStack.size());

    if (undoSize == 0 && redoSize == 0) {
        ImGui::TextDisabled("No history");
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("Undo stack: %d | Redo stack: %d", undoSize, redoSize);
    ImGui::Separator();

    // Show redo items (grayed out, above current position)
    for (int i = redoSize - 1; i >= 0; --i) {
        ImGui::PushStyleColor(ImGuiCol_Text, {0.5f, 0.5f, 0.5f, 0.6f});
        ImGui::Selectable(redoStack[i]->description().c_str());
        if (ImGui::IsItemClicked()) {
            // Redo to this point
            for (int j = 0; j <= i; ++j) cm.redo();
        }
        ImGui::PopStyleColor();
    }

    // Current position marker
    ImGui::TextColored({0.0f, 1.0f, 0.5f, 1.0f}, ">>> Current <<<");

    // Show undo items (most recent first)
    for (int i = undoSize - 1; i >= 0; --i) {
        bool isMostRecent = (i == undoSize - 1);
        if (isMostRecent) {
            ImGui::PushStyleColor(ImGuiCol_Header, {0.2f, 0.4f, 0.6f, 0.6f});
        }
        if (ImGui::Selectable(undoStack[i]->description().c_str(), isMostRecent)) {
            // Undo to this point: undo (undoSize - 1 - i) times
            int count = undoSize - 1 - i;
            for (int j = 0; j < count; ++j) cm.undo();
        }
        if (isMostRecent) ImGui::PopStyleColor();
    }

    ImGui::End();
}

} // namespace bbfx
