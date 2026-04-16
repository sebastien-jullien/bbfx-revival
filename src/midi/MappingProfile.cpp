#include "MappingProfile.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace bbfx {

bool MappingProfile::save(const std::string& path) const {
    try {
        std::ofstream ofs(path);
        if (!ofs.is_open()) return false;
        ofs << toJson().dump(2);
        std::cout << "[MappingProfile] Saved: " << path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[MappingProfile] Save error: " << e.what() << std::endl;
        return false;
    }
}

bool MappingProfile::load(const std::string& path) {
    try {
        std::ifstream ifs(path);
        if (!ifs.is_open()) return false;
        json j = json::parse(ifs);
        fromJson(j);
        std::cout << "[MappingProfile] Loaded: " << path
                  << " (" << midiBindings.size() << " MIDI, "
                  << oscBindings.size() << " OSC)" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[MappingProfile] Load error: " << e.what() << std::endl;
        return false;
    }
}

void MappingProfile::apply() {
    auto& mlm = MidiLearnManager::instance();
    mlm.getBindings().clear();
    for (const auto& b : midiBindings) {
        mlm.addBinding(b);
    }
    std::cout << "[MappingProfile] Applied " << midiBindings.size() << " MIDI bindings" << std::endl;
}

void MappingProfile::captureFromManager() {
    midiBindings = MidiLearnManager::instance().getBindings();
}

json MappingProfile::toJson() const {
    json j;
    j["name"] = name;
    j["version"] = "3.4";

    // MIDI bindings — serialize from this profile's data, not the manager
    json midiArr = json::array();
    for (const auto& b : midiBindings) {
        json m;
        m["deviceId"] = b.deviceId;
        m["channel"] = b.channel;
        m["midiType"] = b.midiType;
        m["number"] = b.number;
        m["targetType"] = b.target.type;
        m["targetIndex"] = b.target.index;
        m["targetNode"] = b.target.nodeName;
        m["targetPort"] = b.target.portName;
        m["min"] = b.min;
        m["max"] = b.max;
        midiArr.push_back(m);
    }
    j["midi"] = midiArr;

    // OSC bindings
    json oscArr = json::array();
    for (const auto& ob : oscBindings) {
        json o;
        o["address"] = ob.address;
        o["targetNode"] = ob.targetNode;
        o["targetPort"] = ob.targetPort;
        o["min"] = ob.min;
        o["max"] = ob.max;
        oscArr.push_back(o);
    }
    j["osc"] = oscArr;

    return j;
}

void MappingProfile::fromJson(const json& j) {
    name = j.value("name", "");

    // MIDI
    midiBindings.clear();
    if (j.contains("midi") && j["midi"].is_array()) {
        for (auto& item : j["midi"]) {
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
            midiBindings.push_back(b);
        }
    }

    // OSC
    oscBindings.clear();
    if (j.contains("osc") && j["osc"].is_array()) {
        for (auto& item : j["osc"]) {
            OscBinding ob;
            ob.address = item.value("address", "");
            ob.targetNode = item.value("targetNode", "");
            ob.targetPort = item.value("targetPort", "");
            ob.min = item.value("min", 0.0f);
            ob.max = item.value("max", 1.0f);
            oscBindings.push_back(ob);
        }
    }
}

} // namespace bbfx
