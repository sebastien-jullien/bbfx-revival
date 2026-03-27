#include "AudioCapture.h"
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace bbfx {

// ── AudioCapture ────────────────────────────────────────────────────────────

AudioCapture::AudioCapture(int sampleRate, int bufferSize)
    : mSampleRate(sampleRate), mBufferSize(bufferSize)
{
    mRingBuffer.resize(bufferSize, 0.0f);
}

AudioCapture::~AudioCapture() {
    stop();
}

void SDLCALL AudioCapture::audioCallback(void* userdata, SDL_AudioStream* stream,
                                          int additional_amount, int /*total_amount*/) {
    // This is called by SDL when the recording device has data available.
    // For recording streams, additional_amount tells us how much data is available to get.
    if (additional_amount <= 0) return;

    auto* self = static_cast<AudioCapture*>(userdata);

    if (!self->mCallbackFired) {
        self->mCallbackFired = true;
        std::cout << "[AudioCapture] First audio data received" << std::endl;
    }

    // Read available data from the stream
    std::vector<float> temp(additional_amount / sizeof(float));
    int got = SDL_GetAudioStreamData(stream, temp.data(), additional_amount);
    if (got <= 0) return;

    int samplesGot = got / static_cast<int>(sizeof(float));

    // Write to ring buffer
    std::lock_guard<std::mutex> lock(self->mMutex);
    for (int i = 0; i < samplesGot; ++i) {
        self->mRingBuffer[self->mWritePos] = temp[i];
        self->mWritePos = (self->mWritePos + 1) % self->mBufferSize;
    }
    self->mNewData = true;
}

bool AudioCapture::start() {
    if (mRunning.load()) return true;

    // Check if SDL audio subsystem is initialized
    if (!(SDL_WasInit(0) & SDL_INIT_AUDIO)) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            std::cerr << "[AudioCapture] Failed to init SDL audio: " << SDL_GetError() << std::endl;
            return false;
        }
    }

    // Check for recording devices
    int count = 0;
    SDL_AudioDeviceID* devices = SDL_GetAudioRecordingDevices(&count);
    if (!devices || count == 0) {
        std::cerr << "[AudioCapture] No recording devices found" << std::endl;
        if (devices) SDL_free(devices);
        return false;
    }
    SDL_free(devices);

    // Open default recording device with our spec
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_F32;
    spec.channels = 1;
    spec.freq = mSampleRate;

    mStream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_RECORDING,
        &spec,
        audioCallback,
        this
    );

    if (!mStream) {
        std::cerr << "[AudioCapture] Failed to open recording device: " << SDL_GetError() << std::endl;
        return false;
    }

    // Log the actual device name
    SDL_AudioDeviceID devId = SDL_GetAudioStreamDevice(mStream);
    const char* devName = SDL_GetAudioDeviceName(devId);
    std::cout << "[AudioCapture] Device: " << (devName ? devName : "unknown") << std::endl;

    // Resume the device (SDL3 opens paused)
    SDL_ResumeAudioStreamDevice(mStream);

    mRunning.store(true);
    std::cout << "[AudioCapture] Started (" << mSampleRate << " Hz, buffer " << mBufferSize << ")" << std::endl;
    return true;
}

void AudioCapture::stop() {
    if (!mRunning.load()) return;
    mRunning.store(false);

    if (mStream) {
        SDL_DestroyAudioStream(mStream);
        mStream = nullptr;
    }
    std::cout << "[AudioCapture] Stopped" << std::endl;
}

bool AudioCapture::poll(std::vector<float>& out) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mNewData) return false;

    out.resize(mBufferSize);
    // Copy ring buffer in order (from oldest to newest)
    int readPos = mWritePos; // writePos points to the oldest sample
    for (int i = 0; i < mBufferSize; ++i) {
        out[i] = mRingBuffer[(readPos + i) % mBufferSize];
    }
    mNewData = false;
    return true;
}

// ── AudioCaptureNode ────────────────────────────────────────────────────────

AudioCaptureNode::AudioCaptureNode(const string& name, AudioCapture* capture)
    : AnimationNode(name), mCapture(capture)
{
    mSamples.resize(capture->getBufferSize(), 0.0f);
    addOutput(new AnimationPort("samples_ready"));
}

void AudioCaptureNode::update() {
    mFreshData = mCapture->poll(mSamples);
    auto* port = mOutputs["samples_ready"];
    if (port) {
        port->setValue(mFreshData ? 1.0f : 0.0f);
    }
    if (mFreshData) {
        fireUpdate();
    }
}

} // namespace bbfx
