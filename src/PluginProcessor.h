#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Parameters.h"
#include "SoundFontEngine.h"
#include "PianoAcousticEngine.h"
#include "DamperClunkModel.h"
#include "LicenseManager.h"
#include "MicAssetLoader.h"
#include <atomic>

class BosendorferAudioProcessor : public juce::AudioProcessor
{
public:
    enum class EmbeddedBank
    {
        Standard = 0, // assets/BOSENDRF.SF2
        Bright   = 1  // assets/BOSENBRI.SF2
    };

    BosendorferAudioProcessor();
    ~BosendorferAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // SoundFont management (Embedded BOSENDRF.SF2 / BOSENBRI.SF2 + External file loader)
    bool loadEmbeddedSoundFont(EmbeddedBank bank = EmbeddedBank::Standard);
    bool loadExternalSoundFont(const juce::File& sf2File);
    bool isUsingExternalSoundFont() const noexcept { return usingExternalSf2.load(std::memory_order_relaxed); }
    EmbeddedBank getCurrentEmbeddedBank() const noexcept { return currentEmbeddedBank.load(std::memory_order_relaxed); }

    void selectPreset(int presetIndex);
    int getSelectedPreset() const noexcept { return sfEngine.getPresetIndex(); }
    juce::StringArray getAvailablePresets() const { return sfEngine.getPresetNames(); }
    juce::String getCurrentSoundFontName() const { return sfEngine.getCurrentSourceName(); }
    int getActiveVoiceCount() const noexcept { return sfEngine.getActiveVoiceCount(); }

    void setSustainPedalFromUI(bool down);
    void setSoftPedalFromUI(bool down);
    bool isSustainPedalDown() const noexcept { return sustainPedalDown.load(std::memory_order_relaxed); }
    bool isSoftPedalDown() const noexcept { return softPedalDown.load(std::memory_order_relaxed); }

    float getOutputRmsLeft() const noexcept { return rmsLeft.load(std::memory_order_relaxed); }
    float getOutputRmsRight() const noexcept { return rmsRight.load(std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    void renderSoundFontBlock(juce::AudioBuffer<float>& buffer,
                              juce::MidiBuffer& midiMessages,
                              float velocityCurve,
                              float keyNoiseGain,
                              float pedalNoiseGain,
                              bool micBypassed,
                              float splitCenterNote,
                              float splitCrossfadeWidth);
    void handleSustainPedalCc64(int ccVal);
    void updateImpulses(bool forceReload);

    SoundFontEngine sfEngine;
    PianoAcousticEngine acousticEngine;
    AuthenticDamperClunk damperClunk;
    MicAssetLoader micLoader { acousticEngine };

    juce::AudioBuffer<float> bufferBass;
    juce::AudioBuffer<float> bufferTreble;

    int activeBassMicIndex { -1 };
    int activeTrebleMicIndex { -1 };
    int activeRoomIndex { -1 };

    double currentSampleRate { 44100.0 };
    int64_t trialTimerSamples { 0 };
    bool pedalWasDown { false };
    double pedalDownTimestamp { 0.0 };

    std::atomic<bool> usingExternalSf2 { false };
    std::atomic<EmbeddedBank> currentEmbeddedBank { EmbeddedBank::Standard };
    std::atomic<bool> sustainPedalDown { false };
    std::atomic<bool> softPedalDown { false };
    std::atomic<bool> pendingSustainEvent { false };
    std::atomic<float> rmsLeft { 0.0f };
    std::atomic<float> rmsRight { 0.0f };
    juce::String externalSf2Path;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BosendorferAudioProcessor)
};
