#include "MidiMappingPanel.h"
#include "../../midi/MidiDeviceManager.h"
#include "../../midi/MidiLearnManager.h"
#include <imgui.h>

namespace bbfx {

void MidiMappingPanel::render() {
    ImGui::Begin("MIDI Mapping");

    auto* mgr = MidiDeviceManager::instance();
    if (!mgr) {
        ImGui::TextDisabled("MIDI not available");
        ImGui::End();
        return;
    }

    auto& mlm = MidiLearnManager::instance();
    auto& bindings = mlm.getBindings();

    // Learn status
    if (mlm.isLearning()) {
        auto& t = mlm.getLearnTarget();
        ImGui::TextColored({1.0f, 0.0f, 1.0f, 1.0f}, "LEARNING: waiting for MIDI input → %s",
                           t.type.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel")) mlm.cancelLearn();
    } else {
        // Quick learn buttons
        if (ImGui::SmallButton("Learn Fader")) {
            MidiLearnTarget target; target.type = "fader"; target.index = 0;
            mlm.startLearn(target);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Learn Trigger")) {
            MidiLearnTarget target; target.type = "trigger"; target.index = 0;
            mlm.startLearn(target);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Learn Port")) {
            MidiLearnTarget target; target.type = "port";
            mlm.startLearn(target);
        }
    }

    ImGui::Separator();
    ImGui::Text("Bindings: %d", static_cast<int>(bindings.size()));

    // Binding table
    if (ImGui::BeginTable("##bindings", 7,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("CC/Note");
        ImGui::TableSetupColumn("Ch");
        ImGui::TableSetupColumn("Target");
        ImGui::TableSetupColumn("Min");
        ImGui::TableSetupColumn("Max");
        ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::TableHeadersRow();

        int toDelete = -1;
        for (int i = 0; i < static_cast<int>(bindings.size()); ++i) {
            auto& b = bindings[i];
            ImGui::PushID(i);
            ImGui::TableNextRow();

            // Type (CC/Note)
            ImGui::TableNextColumn();
            ImVec4 typeCol = (b.midiType == "cc") ?
                ImVec4(0.3f, 0.6f, 1.0f, 1.0f) : ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
            ImGui::TextColored(typeCol, "%s", b.midiType.c_str());

            // Number
            ImGui::TableNextColumn();
            ImGui::Text("%d", b.number);

            // Channel
            ImGui::TableNextColumn();
            ImGui::Text("%d", b.channel);

            // Target
            ImGui::TableNextColumn();
            if (b.target.type == "fader") {
                ImGui::Text("Fader %d", b.target.index);
            } else if (b.target.type == "trigger") {
                ImGui::Text("Trigger %d", b.target.index);
            } else if (b.target.type == "port") {
                ImGui::Text("%s.%s", b.target.nodeName.c_str(), b.target.portName.c_str());
            }

            // Min/Max
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(40);
            ImGui::DragFloat("##min", &b.min, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(40);
            ImGui::DragFloat("##max", &b.max, 0.01f, 0.0f, 1.0f, "%.2f");

            // Delete
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("X")) toDelete = i;

            ImGui::PopID();
        }

        if (toDelete >= 0) mlm.removeBinding(toDelete);

        ImGui::EndTable();
    }

    ImGui::Separator();
    if (ImGui::Button("Clear All Bindings")) {
        bindings.clear();
    }

    ImGui::End();
}

} // namespace bbfx
