#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <vector>

// Define tsf implementation header guard in your main .cpp, not here
#include "tsf.h"

class BsendorferMicDSP
{
public:
    BsendorferMicDSP();
    ~BsendorferMicDSP();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    // Soundfont loading
    bool loadSoundfont(const void* data, int sizeInBytes);

    // Dynamic IR loaders (called on background thread or when UI changes dropdown)
    void loadBassIR(const void* wavData, size_t dataSize);
    void loadTrebleIR(const void* wavData, size_t dataSize);

    // Parameter controls
    void setBypassMicSection(bool bypassed) noexcept { micSectionBypassed.store(bypassed, std::memory_order_relaxed); }
    void setSplitParameters(float centerMidiNote, float overlapSemitones) noexcept;
    void setCrossfeed(float amount) noexcept { crossfeedAmount.store(amount, std::memory_order_relaxed); }

    // Main processing entry point
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

private:
    // Equal-power split calculation
    inline void calculateGainsForNote(int midiNote, float& outBassGain, float& outTrebleGain) const noexcept;

    // Direct render helper for tsf per-channel
    tsf* gSoundFont = nullptr;

    // Dual JUCE Convolution Engines
    juce::dsp::Convolution bassConvolution;
    juce::dsp::Convolution trebleConvolution;

    // Intermediate stereo scratch buffers
    juce::AudioBuffer<float> bassBuffer;
    juce::AudioBuffer<float> trebleBuffer;
    juce::AudioBuffer<float> voiceBuffer;
    std::vector<float> interleavedVoiceBuffer;

    // Atomic / thread-safe state parameters
    std::atomic<bool> micSectionBypassed { false };
    std::atomic<float> splitCenterNote   { 60.0f }; // C4
    std::atomic<float> overlapWidth      { 7.0f };  // 7 semitones (Perfect 5th)
    std::atomic<float> crossfeedAmount   { 0.15f }; // subtle soundboard bleed

    double sampleRate = 44100.0;
    int maxBlockSize  = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BsendorferMicDSP)
};
