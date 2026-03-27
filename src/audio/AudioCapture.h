#pragma once

#include "../bbfx.h"
#include "../core/AnimationNode.h"
#include <vector>
#include <mutex>
#include <atomic>

struct SDL_AudioStream;

// Forward declare SDLCALL for the callback signature
#ifndef SDLCALL
#ifdef _WIN32
#define SDLCALL __cdecl
#else
#define SDLCALL
#endif
#endif

namespace bbfx {

/// Audio capture via SDL3_audio.
/// Opens a recording device (microphone), mono float32, and stores samples
/// in a thread-safe ring buffer accessible from the main thread.
class AudioCapture {
public:
    AudioCapture(int sampleRate = 44100, int bufferSize = 2048);
    ~AudioCapture();

    bool start();
    void stop();
    bool isRunning() const { return mRunning.load(); }

    /// Copy the latest bufferSize samples into out. Returns true if new data available.
    bool poll(std::vector<float>& out);

    int getSampleRate() const { return mSampleRate; }
    int getBufferSize() const { return mBufferSize; }

private:
    static void SDLCALL audioCallback(void* userdata, SDL_AudioStream* stream,
                                       int additional_amount, int total_amount);

    int mSampleRate;
    int mBufferSize;
    SDL_AudioStream* mStream = nullptr;
    std::atomic<bool> mRunning{false};

    // Ring buffer
    std::mutex mMutex;
    std::vector<float> mRingBuffer;
    int mWritePos = 0;
    bool mNewData = false;
    bool mCallbackFired = false;
};

/// AnimationNode wrapper for AudioCapture.
/// Polls the capture each frame and exposes a samples_ready output port.
class AudioCaptureNode : public AnimationNode {
public:
    AudioCaptureNode(const string& name, AudioCapture* capture);
    ~AudioCaptureNode() override = default;

    void update() override;

    /// Access the latest sample buffer (for downstream nodes like AudioAnalyzer)
    const std::vector<float>& getSamples() const { return mSamples; }
    bool hasFreshData() const { return mFreshData; }

private:
    AudioCapture* mCapture;
    std::vector<float> mSamples;
    bool mFreshData = false;
};

} // namespace bbfx
