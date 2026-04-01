#include "InputRecorder.h"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace bbfx {

InputRecorder::~InputRecorder() {
    stop();
}

void InputRecorder::start(const std::string& filename) {
    if (mRecording.load()) stop();
    mFile.open(filename, std::ios::out | std::ios::trunc);
    if (!mFile.is_open()) {
        std::cerr << "[InputRecorder] Cannot open: " << filename << std::endl;
        return;
    }
    mTime = 0.0f;
    mRecording.store(true);
    std::cout << "[InputRecorder] Recording to " << filename << std::endl;
}

void InputRecorder::stop() {
    if (!mRecording.load()) return;
    mRecording.store(false);
    mFile.close();
    std::cout << "[InputRecorder] Stopped" << std::endl;
}

void InputRecorder::recordKey(int code, const std::string& state) {
    if (!mRecording.load()) return;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4);
    ss << R"({"t":)" << mTime << R"(,"type":"key","code":)" << code
       << R"(,"state":")" << state << R"("})";
    mFile << ss.str() << "\n";
    mFile.flush();
}

void InputRecorder::recordAxis(int axis, float value) {
    if (!mRecording.load()) return;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4);
    ss << R"({"t":)" << mTime << R"(,"type":"axis","axis":)" << axis
       << R"(,"value":)" << value << "}";
    mFile << ss.str() << "\n";
    mFile.flush();
}

void InputRecorder::recordBeat() {
    if (!mRecording.load()) return;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4);
    ss << R"({"t":)" << mTime << R"(,"type":"beat"})";
    mFile << ss.str() << "\n";
    mFile.flush();
}

// v3.2 Studio events

void InputRecorder::recordChordActivate(const std::string& name) {
    if (!mRecording.load()) return;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4);
    ss << R"({"t":)" << mTime << R"(,"type":"chord_activate","name":")" << name << R"("})";
    mFile << ss.str() << "\n";
    mFile.flush();
}

void InputRecorder::recordChordDeactivate(const std::string& name) {
    if (!mRecording.load()) return;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4);
    ss << R"({"t":)" << mTime << R"(,"type":"chord_deactivate","name":")" << name << R"("})";
    mFile << ss.str() << "\n";
    mFile.flush();
}

void InputRecorder::recordFaderChange(int index, const std::string& node, const std::string& port, float value) {
    if (!mRecording.load()) return;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4);
    ss << R"({"t":)" << mTime << R"(,"type":"fader","index":)" << index
       << R"(,"node":")" << node << R"(","port":")" << port
       << R"(","value":)" << value << "}";
    mFile << ss.str() << "\n";
    mFile.flush();
}

void InputRecorder::recordBPMChange(float bpm) {
    if (!mRecording.load()) return;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4);
    ss << R"({"t":)" << mTime << R"(,"type":"bpm","bpm":)" << bpm << "}";
    mFile << ss.str() << "\n";
    mFile.flush();
}

void InputRecorder::recordPresetInstantiate(const std::string& presetName) {
    if (!mRecording.load()) return;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4);
    ss << R"({"t":)" << mTime << R"(,"type":"preset","name":")" << presetName << R"("})";
    mFile << ss.str() << "\n";
    mFile.flush();
}

} // namespace bbfx
