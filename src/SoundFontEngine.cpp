#define TSF_IMPLEMENTATION
#include "SoundFontEngine.h"

void SoundFontEngine::extendSubContraRegister(tsf* instance)
{
    if (!instance || !instance->presets)
        return;

    // Bösendorfer 290 Imperial extends down to C0 (MIDI 12).
    // Extend the lowest key zone of each preset down to MIDI 0 (covering C0 = 12)
    // so sub-contra keys C0–G#0 (MIDI 12–20) always sound seamlessly.
    for (int p = 0; p < instance->presetNum; ++p)
    {
        auto& preset = instance->presets[p];
        if (!preset.regions || preset.regionNum <= 0)
            continue;

        unsigned char minLoKey = 127;
        for (int r = 0; r < preset.regionNum; ++r)
        {
            const auto& reg = preset.regions[r];
            if (reg.lokey <= reg.hikey && reg.lokey < minLoKey)
                minLoKey = reg.lokey;
        }

        if (minLoKey > 12 && minLoKey <= 36)
        {
            for (int r = 0; r < preset.regionNum; ++r)
            {
                if (preset.regions[r].lokey == minLoKey)
                    preset.regions[r].lokey = 0;
            }
        }
    }
}

void SoundFontEngine::configureNewInstance(tsf* instance)
{
    if (!instance)
        return;

    extendSubContraRegister(instance);
    tsf_set_output(instance, TSF_STEREO_INTERLEAVED, static_cast<int>(currentSampleRate), 0.0f);
    tsf_set_max_voices(instance, maxVoicesLimit);

    const int presetCount = tsf_get_presetcount(instance);
    if (currentPresetIndex >= presetCount)
        currentPresetIndex = 0;

    for (int ch = 0; ch < 16; ++ch)
    {
        tsf_channel_set_presetindex(instance, ch, currentPresetIndex);
        tsf_channel_set_pitchrange(instance, ch, 2.0f);
        tsf_channel_set_tuning(instance, ch, currentTuningSemitones);
    }
}

void SoundFontEngine::renderSplitBlock(juce::AudioBuffer<float>& bufferBass,
                                       juce::AudioBuffer<float>& bufferTreble,
                                       int startSample,
                                       int numSamples,
                                       float splitCenterNote,
                                       float crossfadeRange)
{
    if (numSamples <= 0)
        return;

    const juce::SpinLock::ScopedTryLockType lock(renderLock);
    if (!lock.isLocked() || !soundFont)
        return;

    const size_t requiredFloats = static_cast<size_t>(numSamples * 2);
    if (voiceScratchBuffer.size() < requiredFloats)
        voiceScratchBuffer.resize(requiredFloats, 0.0f);

    float* bassL = bufferBass.getWritePointer(0, startSample);
    float* bassR = bufferBass.getNumChannels() > 1 ? bufferBass.getWritePointer(1, startSample) : nullptr;
    float* trebL = bufferTreble.getWritePointer(0, startSample);
    float* trebR = bufferTreble.getNumChannels() > 1 ? bufferTreble.getWritePointer(1, startSample) : nullptr;

    const float overlapSemi = juce::jmax(1.0f, crossfadeRange);
    const float halfOverlap = overlapSemi * 0.5f;

    int activeCount = 0;
    struct tsf_voice* v = soundFont->voices;
    struct tsf_voice* vEnd = v + soundFont->voiceNum;

    for (; v != vEnd; ++v)
    {
        if (v->playingPreset == -1)
            continue;

        const float note = static_cast<float>(v->playingKey);

        // 0.0 at or below (splitCenter - halfOverlap), 1.0 at or above (splitCenter + halfOverlap)
        const float t = std::clamp((note - (splitCenterNote - halfOverlap)) / overlapSemi, 0.0f, 1.0f);

        // Equal-power cosine / sine crossfade (preserves perceived energy across the 7-semitone fifth)
        const float bassGain   = std::cos(t * 1.57079632679f);
        const float trebleGain = std::sin(t * 1.57079632679f);

        std::fill(voiceScratchBuffer.begin(), voiceScratchBuffer.begin() + static_cast<ptrdiff_t>(requiredFloats), 0.0f);
        tsf_voice_render(soundFont, v, voiceScratchBuffer.data(), numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const float sL = voiceScratchBuffer[static_cast<size_t>(i * 2)];
            const float sR = voiceScratchBuffer[static_cast<size_t>(i * 2 + 1)];

            if (bassGain > 0.0001f)
            {
                bassL[i] += sL * bassGain;
                if (bassR)
                    bassR[i] += sR * bassGain;
            }

            if (trebleGain > 0.0001f)
            {
                trebL[i] += sL * trebleGain;
                if (trebR)
                    trebR[i] += sR * trebleGain;
            }
        }

        if (v->playingPreset != -1)
            ++activeCount;
    }

    activeVoiceCount.store(activeCount, std::memory_order_relaxed);
}

extern "C"
{
    int tsf_get_voice_count_ext(const tsf* f)
    {
        return f ? f->voiceNum : 0;
    }

    int tsf_voice_is_playing_ext(const tsf* f, int voiceIndex)
    {
        if (!f || voiceIndex < 0 || voiceIndex >= f->voiceNum)
            return 0;
        return f->voices[voiceIndex].playingPreset != -1 ? 1 : 0;
    }

    int tsf_voice_get_midi_key_ext(const tsf* f, int voiceIndex)
    {
        if (!f || voiceIndex < 0 || voiceIndex >= f->voiceNum)
            return -1;
        return f->voices[voiceIndex].playingKey;
    }

    void tsf_voice_render_float_ext(tsf* f, int voiceIndex, float* outputBuffer, int numSamples)
    {
        if (!f || !outputBuffer || numSamples <= 0 || voiceIndex < 0 || voiceIndex >= f->voiceNum)
            return;
        if (f->voices[voiceIndex].playingPreset != -1)
            tsf_voice_render(f, &f->voices[voiceIndex], outputBuffer, numSamples);
    }
}
