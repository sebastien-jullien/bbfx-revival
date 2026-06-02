#include "MidiDeviceManager.h"
#include <rtmidi/RtMidi.h>
#include <iostream>

namespace bbfx {

MidiDeviceManager* MidiDeviceManager::sInstance = nullptr;

MidiDeviceManager* MidiDeviceManager::instance() {
    return sInstance;
}

MidiDeviceManager::MidiDeviceManager() {
    sInstance = this;

    // v3.5 Lot M — pre-size caches: 16 channels × 128 CCs/notes = 2048 entries.
    mCCCache.assign(16 * 128, 0.0f);
    mNoteVelocity.assign(16 * 128, 0.0f);
    mNoteDown.assign(16 * 128, 0);

    // Enumerate input devices
    try {
        RtMidiIn probe;
        int count = static_cast<int>(probe.getPortCount());
        for (int i = 0; i < count; ++i) {
            InputDevice dev;
            dev.index = i;
            dev.name = probe.getPortName(i);
            mInputDevices.push_back(std::move(dev));
        }
        mLastInputCount = count;
    } catch (RtMidiError& e) {
        std::cerr << "[MIDI] Input enumeration error: " << e.what() << std::endl;
    }

    // Enumerate output devices
    try {
        RtMidiOut probe;
        int count = static_cast<int>(probe.getPortCount());
        for (int i = 0; i < count; ++i) {
            OutputDevice dev;
            dev.index = i;
            dev.name = probe.getPortName(i);
            mOutputDevices.push_back(std::move(dev));
        }
    } catch (RtMidiError& e) {
        std::cerr << "[MIDI] Output enumeration error: " << e.what() << std::endl;
    }

    std::cout << "[MIDI] Found " << mInputDevices.size() << " input, "
              << mOutputDevices.size() << " output devices" << std::endl;
    for (auto& dev : mInputDevices) std::cout << "  IN  " << dev.index << ": " << dev.name << std::endl;
    for (auto& dev : mOutputDevices) std::cout << "  OUT " << dev.index << ": " << dev.name << std::endl;
}

MidiDeviceManager::~MidiDeviceManager() {
    closeAll();
    if (sInstance == this) sInstance = nullptr;
}

// ── Enumeration ─────────────────────────────────────────────────────────────

int MidiDeviceManager::getInputDeviceCount() const { return static_cast<int>(mInputDevices.size()); }
int MidiDeviceManager::getOutputDeviceCount() const { return static_cast<int>(mOutputDevices.size()); }

std::string MidiDeviceManager::getInputDeviceName(int index) const {
    return (index >= 0 && index < static_cast<int>(mInputDevices.size())) ? mInputDevices[index].name : "";
}

std::string MidiDeviceManager::getOutputDeviceName(int index) const {
    return (index >= 0 && index < static_cast<int>(mOutputDevices.size())) ? mOutputDevices[index].name : "";
}

std::vector<std::string> MidiDeviceManager::getInputDeviceNames() const {
    std::vector<std::string> names;
    for (auto& dev : mInputDevices) names.push_back(dev.name);
    return names;
}

std::vector<std::string> MidiDeviceManager::getOutputDeviceNames() const {
    std::vector<std::string> names;
    for (auto& dev : mOutputDevices) names.push_back(dev.name);
    return names;
}

// ── Open / Close ────────────────────────────────────────────────────────────

bool MidiDeviceManager::openInput(int index) {
    if (index < 0 || index >= static_cast<int>(mInputDevices.size())) return false;
    auto& dev = mInputDevices[index];
    if (dev.open) return true;

    try {
        dev.rtIn = std::make_unique<RtMidiIn>();
        dev.rtIn->openPort(index);
        dev.cbData = std::make_unique<CallbackData>(CallbackData{this, index});
        dev.rtIn->setCallback(&MidiDeviceManager::midiCallback, dev.cbData.get());
        dev.rtIn->ignoreTypes(false, false, false); // receive sysex, timing, active sensing
        dev.open = true;
        std::cout << "[MIDI] Opened input " << index << ": " << dev.name << std::endl;
        return true;
    } catch (RtMidiError& e) {
        std::cerr << "[MIDI] Failed to open input " << index << ": " << e.what() << std::endl;
        return false;
    }
}

void MidiDeviceManager::closeInput(int index) {
    if (index < 0 || index >= static_cast<int>(mInputDevices.size())) return;
    auto& dev = mInputDevices[index];
    if (!dev.open) return;
    dev.rtIn.reset();
    dev.open = false;
    std::cout << "[MIDI] Closed input " << index << ": " << dev.name << std::endl;
}

bool MidiDeviceManager::openOutput(int index) {
    if (index < 0 || index >= static_cast<int>(mOutputDevices.size())) return false;
    auto& dev = mOutputDevices[index];
    if (dev.open) return true;

    try {
        dev.rtOut = std::make_unique<RtMidiOut>();
        dev.rtOut->openPort(index);
        dev.open = true;
        std::cout << "[MIDI] Opened output " << index << ": " << dev.name << std::endl;
        return true;
    } catch (RtMidiError& e) {
        std::cerr << "[MIDI] Failed to open output " << index << ": " << e.what() << std::endl;
        return false;
    }
}

void MidiDeviceManager::closeOutput(int index) {
    if (index < 0 || index >= static_cast<int>(mOutputDevices.size())) return;
    auto& dev = mOutputDevices[index];
    if (!dev.open) return;
    dev.rtOut.reset();
    dev.open = false;
    std::cout << "[MIDI] Closed output " << index << ": " << dev.name << std::endl;
}

void MidiDeviceManager::openAll() {
    for (int i = 0; i < static_cast<int>(mInputDevices.size()); ++i) openInput(i);
}

void MidiDeviceManager::closeAll() {
    for (int i = 0; i < static_cast<int>(mInputDevices.size()); ++i) closeInput(i);
    for (int i = 0; i < static_cast<int>(mOutputDevices.size()); ++i) closeOutput(i);
}

bool MidiDeviceManager::isInputOpen(int index) const {
    return (index >= 0 && index < static_cast<int>(mInputDevices.size())) && mInputDevices[index].open;
}

// ── Polling ─────────────────────────────────────────────────────────────────

std::vector<MidiMessage> MidiDeviceManager::poll() {
    std::lock_guard<std::mutex> lock(mQueueMutex);
    std::vector<MidiMessage> result;
    while (!mMessageQueue.empty()) {
        result.push_back(mMessageQueue.front());
        mMessageQueue.pop();
    }
    return result;
}

void MidiDeviceManager::injectMessage(const MidiMessage& msg) {
    std::lock_guard<std::mutex> lock(mQueueMutex);
    mMessageQueue.push(msg);
    // v3.5 Lot M — mirror into the state cache so getCC / isNoteOn are
    // consistent even if no consumer ever calls poll().
    uint8_t t = msg.type();
    int ch = msg.channel;
    if (ch >= 1 && ch <= 16) {
        int base = (ch - 1) * 128;
        if (t == MidiMessage::ControlChange && msg.data1 < 128) {
            mCCCache[base + msg.data1] = msg.data2 / 127.0f;
        } else if (t == MidiMessage::NoteOn && msg.data1 < 128) {
            if (msg.data2 == 0) {
                // Running-status note-off convention
                mNoteDown[base + msg.data1] = 0;
                mNoteVelocity[base + msg.data1] = 0.0f;
            } else {
                mNoteDown[base + msg.data1] = 1;
                mNoteVelocity[base + msg.data1] = msg.data2 / 127.0f;
            }
        } else if (t == MidiMessage::NoteOff && msg.data1 < 128) {
            mNoteDown[base + msg.data1] = 0;
            mNoteVelocity[base + msg.data1] = 0.0f;
        }
    }
}

// ── v3.5 Lot M — state cache accessors ─────────────────────────────────────

float MidiDeviceManager::getLastCCValue(int channel, int cc) const {
    if (channel < 1 || channel > 16 || cc < 0 || cc > 127) return 0.0f;
    std::lock_guard<std::mutex> lock(mQueueMutex);
    return mCCCache[(channel - 1) * 128 + cc];
}

bool MidiDeviceManager::isNoteDown(int channel, int note) const {
    if (channel < 1 || channel > 16 || note < 0 || note > 127) return false;
    std::lock_guard<std::mutex> lock(mQueueMutex);
    return mNoteDown[(channel - 1) * 128 + note] != 0;
}

float MidiDeviceManager::getNoteVelocity(int channel, int note) const {
    if (channel < 1 || channel > 16 || note < 0 || note > 127) return 0.0f;
    std::lock_guard<std::mutex> lock(mQueueMutex);
    return mNoteVelocity[(channel - 1) * 128 + note];
}

// ── Send ────────────────────────────────────────────────────────────────────

void MidiDeviceManager::sendMessage(int outputDeviceId, uint8_t status, uint8_t data1, uint8_t data2) {
    if (outputDeviceId < 0 || outputDeviceId >= static_cast<int>(mOutputDevices.size())) return;
    auto& dev = mOutputDevices[outputDeviceId];
    if (!dev.open || !dev.rtOut) return;

    std::vector<unsigned char> bytes = {status, data1, data2};
    try {
        dev.rtOut->sendMessage(&bytes);
    } catch (RtMidiError& e) {
        std::cerr << "[MIDI] Send error on output " << outputDeviceId << ": " << e.what() << std::endl;
    }
}

// ── Hot-plug ────────────────────────────────────────────────────────────────

void MidiDeviceManager::checkHotPlug() {
    if (++mHotPlugCounter < 60) return; // check every ~1 second at 60fps
    mHotPlugCounter = 0;

    try {
        RtMidiIn probe;
        int count = static_cast<int>(probe.getPortCount());
        if (count != mLastInputCount) {
            std::cout << "[MIDI] Device change detected: " << mLastInputCount << " → " << count << std::endl;
            // C11 — fermer proprement chaque device ouvert AVANT de vider/réenumérer.
            // closeInput() fait `rtIn.reset()` qui annule le callback rtmidi de façon
            // SYNCHRONE (le thread interne rtmidi ne rappellera plus midiCallback après).
            // Sans ça, mInputDevices.clear() détruit RtMidiIn+CallbackData alors qu'un
            // message peut être en cours de callback sur le thread rtmidi → use-after-free.
            for (int i = 0; i < static_cast<int>(mInputDevices.size()); ++i) closeInput(i);
            // Re-enumerate
            mInputDevices.clear();
            for (int i = 0; i < count; ++i) {
                InputDevice dev;
                dev.index = i;
                dev.name = probe.getPortName(i);
                mInputDevices.push_back(std::move(dev));
            }
            mLastInputCount = count;
        }
    } catch (...) {}
}

// ── Callback (rtmidi thread) ────────────────────────────────────────────────

void MidiDeviceManager::midiCallback(double timestamp, std::vector<unsigned char>* message, void* userData) {
    auto* cbData = static_cast<CallbackData*>(userData);
    if (!cbData || !cbData->manager || !message || message->size() < 1) return;

    MidiMessage msg;
    msg.timestamp = timestamp;
    msg.status = (*message)[0];
    msg.data1 = (message->size() > 1) ? (*message)[1] : 0;
    msg.data2 = (message->size() > 2) ? (*message)[2] : 0;
    msg.channel = (msg.status & 0x0F) + 1; // 1-16
    msg.deviceId = cbData->deviceIndex;

    std::lock_guard<std::mutex> lock(cbData->manager->mQueueMutex);
    cbData->manager->mMessageQueue.push(msg);
    // v3.5 Lot M — mirror into state cache (same lock as queue).
    uint8_t t = msg.type();
    int ch = msg.channel;
    if (ch >= 1 && ch <= 16) {
        int base = (ch - 1) * 128;
        auto& mgr = *cbData->manager;
        if (t == MidiMessage::ControlChange && msg.data1 < 128) {
            mgr.mCCCache[base + msg.data1] = msg.data2 / 127.0f;
        } else if (t == MidiMessage::NoteOn && msg.data1 < 128) {
            if (msg.data2 == 0) {
                mgr.mNoteDown[base + msg.data1] = 0;
                mgr.mNoteVelocity[base + msg.data1] = 0.0f;
            } else {
                mgr.mNoteDown[base + msg.data1] = 1;
                mgr.mNoteVelocity[base + msg.data1] = msg.data2 / 127.0f;
            }
        } else if (t == MidiMessage::NoteOff && msg.data1 < 128) {
            mgr.mNoteDown[base + msg.data1] = 0;
            mgr.mNoteVelocity[base + msg.data1] = 0.0f;
        }
    }
}

} // namespace bbfx
