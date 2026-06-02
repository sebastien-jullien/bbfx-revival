#include "MidiActivityPanel.h"
#include "../../midi/MidiDeviceManager.h"
#include "../../midi/MidiMessage.h"
#include <imgui.h>
#include <cstdio>

namespace bbfx {

static const char* sNoteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

std::string MidiActivityPanel::noteName(int n) {
    if (n < 0 || n > 127) return "?";
    int octave = (n / 12) - 1;
    return std::string(sNoteNames[n % 12]) + std::to_string(octave);
}

std::string MidiActivityPanel::typeName(uint8_t type) {
    switch (type) {
        case MidiMessage::NoteOn:       return "NoteOn";
        case MidiMessage::NoteOff:      return "NoteOff";
        case MidiMessage::ControlChange: return "CC";
        case MidiMessage::ProgramChange: return "PC";
        case MidiMessage::PitchBend:    return "PitchBend";
        case MidiMessage::ChannelAftertouch: return "AfterT";
        case MidiMessage::PolyAftertouch: return "PolyAT";
        default: return "???";
    }
}

void MidiActivityPanel::render() {
    ImGui::Begin("MIDI Activity");

    auto* mgr = MidiDeviceManager::instance();
    if (!mgr) {
        ImGui::TextDisabled("MIDI not available");
        ImGui::End();
        return;
    }

    // Poll new messages
    auto msgs = mgr->poll();
    float dt = ImGui::GetIO().DeltaTime;
    mMpsTimer += dt;
    mMpsCounter += static_cast<int>(msgs.size());
    if (mMpsTimer >= 1.0f) {
        mMessagesPerSecond = static_cast<float>(mMpsCounter) / mMpsTimer;
        mMpsCounter = 0;
        mMpsTimer = 0.0f;
    }

    for (auto& m : msgs) {
        // Apply filters
        if (mFilterDevice >= 0 && m.deviceId != mFilterDevice) continue;
        if (mFilterChannel > 0 && m.channel != mFilterChannel) continue;

        LogEntry entry;
        entry.timestamp = m.timestamp;
        entry.deviceId = m.deviceId;
        entry.channel = m.channel;
        entry.type = m.type();
        entry.data1 = m.data1;
        entry.data2 = m.data2;

        // Decode
        if (entry.type == MidiMessage::NoteOn || entry.type == MidiMessage::NoteOff) {
            entry.decoded = noteName(m.data1) + " vel=" + std::to_string(m.data2);
        } else if (entry.type == MidiMessage::ControlChange) {
            entry.decoded = "CC#" + std::to_string(m.data1) + "=" + std::to_string(m.data2);
        } else if (entry.type == MidiMessage::ProgramChange) {
            entry.decoded = "PC#" + std::to_string(m.data1);
        } else if (entry.type == MidiMessage::PitchBend) {
            int bend = (m.data2 << 7) | m.data1;
            entry.decoded = "Bend=" + std::to_string(bend - 8192);
        }

        mLog.push_back(entry);
        mMessageCount++;
        if (mLog.size() > kMaxEntries) mLog.pop_front();
    }

    // Stats
    ImGui::Text("Messages: %d | Rate: %.0f/s", mMessageCount, mMessagesPerSecond);
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) { mLog.clear(); mMessageCount = 0; }

    // Filters
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Ch Filter", &mFilterChannel);
    if (mFilterChannel < 0) mFilterChannel = 0;
    if (mFilterChannel > 16) mFilterChannel = 16;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Dev Filter", &mFilterDevice);
    if (mFilterDevice < -1) mFilterDevice = -1;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("-1 = all devices");

    // N11 — case Auto-scroll (avant : `mAutoScroll` hardcodé true, sans contrôle UI).
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &mAutoScroll);

    ImGui::Separator();

    // Message log
    ImGui::BeginChild("##midilog", {0, 0}, false, ImGuiWindowFlags_HorizontalScrollbar);
    for (auto& e : mLog) {
        // Color by type
        ImVec4 col = {0.8f, 0.8f, 0.8f, 1.0f};
        if (e.type == MidiMessage::NoteOn || e.type == MidiMessage::NoteOff)
            col = {0.3f, 1.0f, 0.3f, 1.0f}; // green
        else if (e.type == MidiMessage::ControlChange)
            col = {0.3f, 0.6f, 1.0f, 1.0f}; // blue
        else if (e.type == MidiMessage::ProgramChange)
            col = {1.0f, 0.8f, 0.2f, 1.0f}; // yellow

        ImGui::TextColored(col, "Ch%d %s %s", e.channel, typeName(e.type).c_str(), e.decoded.c_str());
    }
    if (mAutoScroll && !mLog.empty()) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    ImGui::End();
}

} // namespace bbfx
