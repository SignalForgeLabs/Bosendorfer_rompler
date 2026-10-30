#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "tsf.h"
#include "AssetShield.h"
#include <vector>
#include <atomic>
#include <algorithm>
#include <cmath>

class SoundFontEngine
{
public:
    SoundFontEngine() = default;
    ~SoundFontEngine()
    {
        close();
    }

    bool loadFromMemory(const void* data, int sizeBytes, const juce::String& sourceLabel = "Embedded Bosendorfer Imperial")
    {
        tsf* newFont = AssetShield::loadSoundFont(data, sizeBytes);
        if (!newFont)
            newFont = tsf_load_memory(data, sizeBytes);
        if (!newFont)
            return false;

        configureNewInstance(newFont);
        swapSoundFont(newFont, sourceLabel, juce::File());
        return true;
    }

    bool loadFromFile(const juce::File& sf2File)
    {
        if (!sf2File.existsAsFile())
            return false;

        tsf* newFont = tsf_load_filename(sf2File.getFullPathName().toRawUTF8());
        if (!newFont)
            return false;

        configureNewInstance(newFont);
        swapSoundFont(newFont, sf2File.getFileName(), sf2File);
        return true;
    }

    void prepare(double sampleRate, int maxBlockSize)
    {
        const juce::SpinLock::ScopedLockType lock(renderLock);
        currentSampleRate = static_cast<float>(sampleRate > 0.0 ? sampleRate : 44100.0);
        if (soundFont)
        {
            tsf_set_output(soundFont, TSF_STEREO_INTERLEAVED, static_cast<int>(currentSampleRate), 0.0f);
        }
        const size_t cap = static_cast<size_t>(juce::jmax(512, maxBlockSize) * 2);
        interleavedBuffer.resize(cap, 0.0f);
        voiceScratchBuffer.resize(cap, 0.0f);
    }

    void noteOn(int channel, int note, float velocity)
    {
        if (note < 0 || note > 127)
            return;

        const juce::SpinLock::ScopedTryLockType lock(renderLock);
        if (lock.isLocked() && soundFont)
        {
            const int safeChannel = juce::jlimit(0, 15, channel);
            tsf_channel_note_on(soundFont, safeChannel, note, juce::jlimit(0.0f, 1.0f, velocity));
        }
    }

    void noteOff(int channel, int note)
    {
        if (note < 0 || note > 127)
            return;

        const juce::SpinLock::ScopedTryLockType lock(renderLock);
        if (lock.isLocked() && soundFont)
        {
            const int safeChannel = juce::jlimit(0, 15, channel);
            tsf_channel_note_off(soundFont, safeChannel, note);
        }
    }

    void allNotesOff(int channel = -1)
    {
        const juce::SpinLock::ScopedTryLockType lock(renderLock);
        if (lock.isLocked() && soundFont)
        {
            if (channel < 0)
                tsf_note_off_all(soundFont);
            else
                tsf_channel_sounds_off_all(soundFont, juce::jlimit(0, 15, channel));
        }
    }

    void pitchBend(int channel, int midiPitchBendValue)
    {
        const juce::SpinLock::ScopedTryLockType lock(renderLock);
        if (lock.isLocked() && soundFont)
        {
            const int safeChannel = juce::jlimit(0, 15, channel);
            tsf_channel_set_pitchwheel(soundFont, safeChannel, juce::jlimit(0, 16383, midiPitchBendValue));
        }
    }

    void controlChange(int channel, int controller, int value)
    {
        const juce::SpinLock::ScopedTryLockType lock(renderLock);
        if (lock.isLocked() && soundFont)
        {
            const int safeChannel = juce::jlimit(0, 15, channel);
            tsf_channel_midi_control(soundFont, safeChannel, controller, juce::jlimit(0, 127, value));
        }
    }

    // Standard single-buffer stereo render (used when Mic Section is bypassed)
    void renderBlock(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0)
            return;

        const juce::SpinLock::ScopedTryLockType lock(renderLock);
        if (!lock.isLocked() || !soundFont)
            return;

        if (interleavedBuffer.size() < static_cast<size_t>(numSamples * 2))
            interleavedBuffer.resize(static_cast<size_t>(numSamples * 2), 0.0f);

        tsf_render_float(soundFont, interleavedBuffer.data(), numSamples, 0);

        auto* leftOut = buffer.getWritePointer(0);
        auto* rightOut = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            leftOut[i] += interleavedBuffer[static_cast<size_t>(i * 2)];
            if (rightOut)
                rightOut[i] += interleavedBuffer[static_cast<size_t>(i * 2 + 1)];
        }

        activeVoiceCount.store(tsf_active_voice_count(soundFont), std::memory_order_relaxed);
    }

    // Per-voice Equal-Power Keyboard Split render into bufferBass (Mic A) and bufferTreble (Mic B)
    void renderSplitBlock(juce::AudioBuffer<float>& bufferBass,
                          juce::AudioBuffer<float>& bufferTreble,
                          int startSample,
                          int numSamples,
                          float splitCenterNote = 60.0f,
                          float crossfadeRange = 7.0f);

    void setPolyphony(int maxVoices)
    {
        maxVoicesLimit = juce::jlimit(16, 512, maxVoices);
        const juce::SpinLock::ScopedLockType lock(renderLock);
        if (soundFont)
            tsf_set_max_voices(soundFont, maxVoicesLimit);
    }

    void setPresetIndex(int presetIndex)
    {
        const juce::SpinLock::ScopedLockType lock(renderLock);
        if (!soundFont)
            return;

        const int count = tsf_get_presetcount(soundFont);
        if (count <= 0)
            return;

        currentPresetIndex = juce::jlimit(0, count - 1, presetIndex);
        for (int ch = 0; ch < 16; ++ch)
            tsf_channel_set_presetindex(soundFont, ch, currentPresetIndex);
    }

    int getPresetIndex() const noexcept { return currentPresetIndex; }

    int getPresetCount() const
    {
        const juce::SpinLock::ScopedLockType lock(renderLock);
        return soundFont ? tsf_get_presetcount(soundFont) : 0;
    }

    juce::StringArray getPresetNames() const
    {
        juce::StringArray names;
        const juce::SpinLock::ScopedLockType lock(renderLock);
        if (soundFont)
        {
            const int count = tsf_get_presetcount(soundFont);
            for (int i = 0; i < count; ++i)
            {
                const char* rawName = tsf_get_presetname(soundFont, i);
                juce::String name = rawName ? juce::String::fromUTF8(rawName).trim() : juce::String();
                if (name.isEmpty())
                    name = "Preset " + juce::String(i + 1);
                names.add(name);
            }
        }
        return names;
    }

    void setTuningSemitones(float semitones)
    {
        if (std::abs(semitones - currentTuningSemitones) < 1.0e-4f)
            return;

        currentTuningSemitones = semitones;
        const juce::SpinLock::ScopedTryLockType lock(renderLock);
        if (lock.isLocked() && soundFont)
        {
            for (int ch = 0; ch < 16; ++ch)
                tsf_channel_set_tuning(soundFont, ch, currentTuningSemitones);
        }
    }

    int getActiveVoiceCount() const noexcept
    {
        return activeVoiceCount.load(std::memory_order_relaxed);
    }

    bool isLoaded() const noexcept
    {
        return soundFont != nullptr;
    }

    juce::String getCurrentSourceName() const
    {
        const juce::ScopedLock sl(metadataLock);
        return loadedSourceName;
    }

    juce::File getCurrentFile() const
    {
        const juce::ScopedLock sl(metadataLock);
        return loadedFile;
    }

    void close()
    {
        tsf* oldFont = nullptr;
        {
            const juce::SpinLock::ScopedLockType lock(renderLock);
            oldFont = soundFont;
            soundFont = nullptr;
            activeVoiceCount.store(0, std::memory_order_relaxed);
        }
        if (oldFont)
            tsf_close(oldFont);
    }

private:
    void configureNewInstance(tsf* instance);
    static void extendSubContraRegister(tsf* instance);

    void swapSoundFont(tsf* newFont, const juce::String& sourceLabel, const juce::File& sourceFile)
    {
        tsf* oldFont = nullptr;
        {
            const juce::SpinLock::ScopedLockType lock(renderLock);
            oldFont = soundFont;
            soundFont = newFont;
            activeVoiceCount.store(0, std::memory_order_relaxed);
        }
        {
            const juce::ScopedLock sl(metadataLock);
            loadedSourceName = sourceLabel;
            loadedFile = sourceFile;
        }
        if (oldFont)
            tsf_close(oldFont);
    }

    mutable juce::SpinLock renderLock;
    mutable juce::CriticalSection metadataLock;
    tsf* soundFont { nullptr };
    float currentSampleRate { 44100.0f };
    int maxVoicesLimit { 256 };
    int currentPresetIndex { 0 };
    float currentTuningSemitones { 0.0f };
    std::atomic<int> activeVoiceCount { 0 };
    std::vector<float> interleavedBuffer;
    std::vector<float> voiceScratchBuffer;
    juce::String loadedSourceName { "None" };
    juce::File loadedFile;
};
