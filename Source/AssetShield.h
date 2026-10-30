#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "tsf.h"
#include <cstring>

class AssetShield
{
public:
    static inline const char* getCipherKey() noexcept
    {
        return "SFALabs_AudioShield_2026_AJ_Key";
    }

    // In-memory XOR stream cipher descrambler
    static juce::MemoryBlock descramble(const void* rawData, size_t dataSize)
    {
        if (rawData == nullptr || dataSize == 0)
            return {};

        juce::MemoryBlock decrypted(dataSize);
        auto* dest = static_cast<uint8_t*>(decrypted.getData());
        const auto* src = static_cast<const uint8_t*>(rawData);
        const auto* key = reinterpret_cast<const uint8_t*>(getCipherKey());
        const size_t keyLen = std::strlen(getCipherKey());

        for (size_t i = 0; i < dataSize; ++i)
        {
            dest[i] = src[i] ^ key[i % keyLen];
        }

        return decrypted;
    }

    // Direct TSF loader from scrambled BinaryData
    static tsf* loadSoundFont(const void* rawData, int dataSize)
    {
        if (rawData == nullptr || dataSize <= 0)
            return nullptr;

        juce::MemoryBlock decrypted = descramble(rawData, static_cast<size_t>(dataSize));

        // tsf_load_memory parses the unencrypted byte array directly in RAM
        tsf* font = tsf_load_memory(decrypted.getData(), static_cast<int>(decrypted.getSize()));

        // Zero and release temporary buffer immediately
        decrypted.fillWith(0);
        return font;
    }

    // Direct JUCE Convolution Loader from scrambled BinaryData (moves AudioBuffer ownership safely)
    static void loadImpulseResponse(juce::dsp::Convolution& convolution,
                                    const void* rawData,
                                    size_t dataSize,
                                    juce::dsp::Convolution::Stereo isStereo = juce::dsp::Convolution::Stereo::yes,
                                    juce::dsp::Convolution::Trim trim = juce::dsp::Convolution::Trim::yes,
                                    size_t sizeToUse = 0)
    {
        if (rawData == nullptr || dataSize == 0)
            return;

        juce::MemoryBlock decrypted = descramble(rawData, dataSize);

        auto stream = std::make_unique<juce::MemoryInputStream>(decrypted.getData(), decrypted.getSize(), false);
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatReader> reader(wavFormat.createReaderFor(stream.release(), true));

        if (reader != nullptr && reader->lengthInSamples > 0)
        {
            int numSamples = static_cast<int>(reader->lengthInSamples);
            if (sizeToUse > 0 && static_cast<int>(sizeToUse) < numSamples)
                numSamples = static_cast<int>(sizeToUse);

            const int numCh = juce::jmax(1, static_cast<int>(reader->numChannels));
            juce::AudioBuffer<float> irBuffer(numCh, numSamples);
            reader->read(&irBuffer, 0, numSamples, 0, true, true);
            const double sr = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
            reader.reset();

            convolution.loadImpulseResponse(
                std::move(irBuffer),
                sr,
                isStereo,
                trim,
                juce::dsp::Convolution::Normalise::yes
            );
        }

        decrypted.fillWith(0);
    }

    // Direct JUCE Image loader from scrambled BinaryData
    static juce::Image loadImage(const void* rawData, size_t dataSize)
    {
        if (rawData == nullptr || dataSize == 0)
            return {};

        juce::MemoryBlock decrypted = descramble(rawData, dataSize);
        juce::Image img = juce::ImageFileFormat::loadFrom(decrypted.getData(), decrypted.getSize());
        decrypted.fillWith(0);
        return img;
    }
};
