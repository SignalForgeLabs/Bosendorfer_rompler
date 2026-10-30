#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace IDs
{
    inline const juce::ParameterID bassMicType        { "bass_mic_type", 1 };
    inline const juce::ParameterID bassMicGain        { "bass_mic_gain", 1 };
    inline const juce::ParameterID trebleMicType      { "treble_mic_type", 1 };
    inline const juce::ParameterID trebleMicGain      { "treble_mic_gain", 1 };
    inline const juce::ParameterID micSectionBypassed { "mic_bypass", 1 };
    inline const juce::ParameterID splitCenter        { "split_center", 1 };
    inline const juce::ParameterID splitWidth         { "split_width", 1 };
    inline const juce::ParameterID micCrossfeed       { "mic_crossfeed", 1 };
    inline const juce::ParameterID roomIrType         { "room_ir_type", 1 };
    inline const juce::ParameterID roomMix            { "room_mix", 1 };
    inline const juce::ParameterID masterGain         { "master_gain", 1 };
    inline const juce::ParameterID velCurve           { "vel_curve", 1 };
    inline const juce::ParameterID lidPosition        { "lid_pos", 1 };
    inline const juce::ParameterID tuningSt           { "tuning_st", 1 };
    inline const juce::ParameterID keyNoise           { "key_noise", 1 };
    inline const juce::ParameterID pedalNoise         { "pedal_noise", 1 };
}

struct MicModelAssets
{
    const char* displayName;
    const char* wavFileName;
    const char* leftPngFileName;
    const char* rightPngFileName;
};

inline const MicModelAssets& getMicModelAssets(int micIndex)
{
    static const MicModelAssets kModels[4] = {
        { "Neumann KM 184",     "Mic_Neumann_KM184.wav",  "Neumann_KM184_L.png", "Neuman_KM184_R.png" },
        { "Neuman TLM 103",     "Mic_Neumann_TLM103.wav", "Neuman_TLM_L.png",    "Neuman_TLM_R.png"   },
        { "Neumann U87",        "Mic_Neumann_U87.wav",    "Neuman_U87-L.png",    "Neuman_U87-R.png"   },
        { "RCA 44BX NEVE 1073", "RCA 44BX NEVE.wav",      "RCA_Ribbon_L.png",    "RCA_Ribbon_R.png"   }
    };
    return kModels[juce::jlimit(0, 3, micIndex)];
}

inline juce::String getMicAssetBaseName(int micIndex)
{
    return getMicModelAssets(micIndex).wavFileName;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 4 Selectable Microphones for either Left (Bass) or Right (Treble)
    juce::StringArray micChoices {
        "Neumann KM 184",
        "Neuman TLM 103",
        "Neumann U87",
        "RCA 44BX NEVE 1073"
    };
    juce::StringArray roomChoices { "Vienna Konzerthaus", "Maple Stage 40ft", "Scoring Studio Hall" };

    // Default Left (Bass) Mic: Neuman TLM 103 (1), Default Right (Treble) Mic: Neumann KM 184 (0)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        IDs::bassMicType, "Left Bass Mic Model", micChoices, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::bassMicGain, "Left Bass Mic Gain", juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        IDs::trebleMicType, "Right Treble Mic Model", micChoices, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::trebleMicGain, "Right Treble Mic Gain", juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        IDs::micSectionBypassed, "Mic Section Bypass", false));

    // Crossover spans F below Middle C (F3 = MIDI 53) to A over Middle C (A4 = MIDI 69), centered at MIDI 61
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        IDs::splitCenter, "Split Center Note", 53, 69, 61,
        juce::AudioParameterIntAttributes().withStringFromValueFunction(
            [](int value, int) { return juce::MidiMessage::getMidiNoteName(value, true, true, 4) + " (" + juce::String(value) + ")"; })));

    // Overlap Width defaulted to 16 semitones (F3 [53] to A4 [69])
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::splitWidth, "Split Overlap (st)", juce::NormalisableRange<float>(3.0f, 24.0f, 0.5f), 16.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::micCrossfeed, "Mic Bleed / Spill", juce::NormalisableRange<float>(0.0f, 0.5f, 0.01f), 0.15f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        IDs::roomIrType, "Acoustic Space", roomChoices, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::roomMix, "Room IR Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.25f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::masterGain, "Master Output", juce::NormalisableRange<float>(-36.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::velCurve, "Velocity Curve", juce::NormalisableRange<float>(0.5f, 2.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::lidPosition, "Lid Position", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::tuningSt, "Fine Tune (st)", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::keyNoise, "Key Noise", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::pedalNoise, "Pedal Noise", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.65f));

    return { params.begin(), params.end() };
}
