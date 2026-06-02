#include "MidiLearnManager.h"
#include <nlohmann/json.hpp>
#include <iostream>

namespace bbfx {

MidiLearnManager& MidiLearnManager::instance() {
    static MidiLearnManager s;
    return s;
}

void MidiLearnManager::startLearn(const MidiLearnTarget& target) {
    mLearning = true;
    mLearnTarget = target;
    std::cout << "[MidiLearn] Waiting for MIDI input (target: " << target.type << ")" << std::endl;
}

void MidiLearnManager::cancelLearn() {
    mLearning = false;
    std::cout << "[MidiLearn] Cancelled" << std::endl;
}

void MidiLearnManager::processMessages(const std::vector<MidiMessage>& messages) {
    if (!mLearning || messages.empty()) return;

    for (auto& msg : messages) {
        uint8_t type = msg.type();
        if (type == MidiMessage::ControlChange) {
            MidiBinding binding;
            binding.deviceId = msg.deviceId;
            binding.channel = msg.channel;
            binding.midiType = "cc";
            binding.number = msg.data1;
            binding.target = mLearnTarget;
            addBinding(binding);
            mLearning = false;
            std::cout << "[MidiLearn] Bound CC#" << (int)msg.data1 << " ch" << msg.channel
                      << " → " << mLearnTarget.type << std::endl;
            return;
        }
        if (type == MidiMessage::NoteOn && msg.data2 > 0) {
            MidiBinding binding;
            binding.deviceId = msg.deviceId;
            binding.channel = msg.channel;
            binding.midiType = "note";
            binding.number = msg.data1;
            binding.target = mLearnTarget;
            addBinding(binding);
            mLearning = false;
            std::cout << "[MidiLearn] Bound Note " << (int)msg.data1 << " ch" << msg.channel
                      << " → " << mLearnTarget.type << std::endl;
            return;
        }
    }
}

void MidiLearnManager::addBinding(const MidiBinding& binding) {
    // Conflict detection: remove existing binding with same MIDI source
    for (auto it = mBindings.begin(); it != mBindings.end(); ) {
        if (it->midiType == binding.midiType && it->number == binding.number &&
            (it->channel == 0 || binding.channel == 0 || it->channel == binding.channel)) {
            std::cout << "[MidiLearn] Replacing conflicting binding: " << it->midiType
                      << "#" << it->number << " → " << it->target.type << std::endl;
            it = mBindings.erase(it);
        } else {
            ++it;
        }
    }
    mBindings.push_back(binding);
}

void MidiLearnManager::removeBinding(int index) {
    if (index >= 0 && index < static_cast<int>(mBindings.size()))
        mBindings.erase(mBindings.begin() + index);
}

void MidiLearnManager::clearBindings() {
    // C5 — teardown propre : annule un éventuel mode learn en cours (sinon
    // mLearnTarget pointerait une cible dont le binding vient de disparaître),
    // puis vide le store. Passe par cette API plutôt que muter getBindings().
    cancelLearn();
    mBindings.clear();
}

const MidiBinding* MidiLearnManager::findBindingForCC(int cc, int channel) const {
    for (auto& b : mBindings) {
        if (b.midiType == "cc" && b.number == cc && (b.channel == 0 || b.channel == channel))
            return &b;
    }
    return nullptr;
}

const MidiBinding* MidiLearnManager::findBindingForNote(int note, int channel) const {
    for (auto& b : mBindings) {
        if (b.midiType == "note" && b.number == note && (b.channel == 0 || b.channel == channel))
            return &b;
    }
    return nullptr;
}

nlohmann::json MidiLearnManager::toJson() const {
    nlohmann::json arr = nlohmann::json::array();
    for (auto& b : mBindings) {
        nlohmann::json j;
        j["deviceId"] = b.deviceId;
        j["channel"] = b.channel;
        j["midiType"] = b.midiType;
        j["number"] = b.number;
        j["targetType"] = b.target.type;
        j["targetIndex"] = b.target.index;
        j["targetNode"] = b.target.nodeName;
        j["targetPort"] = b.target.portName;
        j["min"] = b.min;
        j["max"] = b.max;
        arr.push_back(j);
    }
    return arr;
}

void MidiLearnManager::fromJson(const nlohmann::json& j) {
    mBindings.clear();
    if (!j.is_array()) return;
    for (size_t idx = 0; idx < j.size(); ++idx) {
        const auto& item = j[idx];
        MidiBinding b;
        b.deviceId = item.value("deviceId", -1);
        b.channel = item.value("channel", 0);
        b.midiType = item.value("midiType", "cc");
        b.number = item.value("number", -1);
        b.target.type = item.value("targetType", "fader");
        b.target.index = item.value("targetIndex", -1);
        b.target.nodeName = item.value("targetNode", "");
        b.target.portName = item.value("targetPort", "");
        b.min = item.value("min", 0.0f);
        b.max = item.value("max", 1.0f);
        mBindings.push_back(b);
    }
}

} // namespace bbfx
