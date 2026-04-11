#pragma once
#include <cstdint>

namespace bbfx {

/// A single MIDI message received from a device.
struct MidiMessage {
    int deviceId = -1;          // which device sent this (-1 = virtual)
    uint8_t status = 0;         // raw status byte (includes channel in lower nibble)
    uint8_t data1 = 0;          // note number or CC number
    uint8_t data2 = 0;          // velocity or CC value
    int channel = 0;            // 1-16 (parsed from status), 0 = unknown
    double timestamp = 0.0;     // rtmidi timestamp (seconds)

    // Convenience: extract message type from status byte (upper nibble)
    uint8_t type() const { return status & 0xF0; }

    // Common MIDI status types
    static constexpr uint8_t NoteOff       = 0x80;
    static constexpr uint8_t NoteOn        = 0x90;
    static constexpr uint8_t PolyAftertouch = 0xA0;
    static constexpr uint8_t ControlChange = 0xB0;
    static constexpr uint8_t ProgramChange = 0xC0;
    static constexpr uint8_t ChannelAftertouch = 0xD0;
    static constexpr uint8_t PitchBend     = 0xE0;
    static constexpr uint8_t System        = 0xF0;

    // MIDI clock
    static constexpr uint8_t Clock    = 0xF8;
    static constexpr uint8_t Start    = 0xFA;
    static constexpr uint8_t Continue = 0xFB;
    static constexpr uint8_t Stop     = 0xFC;
};

} // namespace bbfx
