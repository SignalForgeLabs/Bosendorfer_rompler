#include "BsendorferMicDSP.h"

extern "C"
{
    int tsf_get_voice_count_ext(const tsf* f);
    int tsf_voice_is_playing_ext(const tsf* f, int voiceIndex);
    int tsf_voice_get_midi_key_ext(const tsf* f, int voiceIndex);
    void tsf_voice_render_float_ext(tsf* f, int voiceIndex, float* outputBuffer, int numSamples);
}

constexpr float PI_OVER_2 = 1.5707963267948966f;

BsendorferMicDSP::BsendorferMicDSP() = default;

BsendorferMicDSP::~BsendorferMicDSP()
{
    if (gSoundFont != nullptr)
    {
        tsf_close(gSoundFont);
        gSoundFont = nullptr;
    }
}

void BsendorferMicDSP::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate   = spec.sampleRate;
    maxBlockSize = static_cast<int>(spec.maximumBlockSize);

    if (gSoundFont != nullptr)
        tsf_set_output(gSoundFont, TSF_STEREO_INTERLEAVED, static_cast<int>(sampleRate), 0.0f);

    // Prepare convolution engines
    bassConvolution.prepare(spec);
    trebleConvolution.prepare(spec);

    // Allocate scratch processing buffers (2 channels each)
    bassBuffer.setSize(2, maxBlockSize);
    trebleBuffer.setSize(2, maxBlockSize);
    voiceBuffer.setSize(2, maxBlockSize);
    interleavedVoiceBuffer.resize(static_cast<size_t>(maxBlockSize) * 2, 0.0f);

    reset();
}

void BsendorferMicDSP::reset()
{
    bassConvolution.reset();
    trebleConvolution.reset();
    bassBuffer.clear();
    trebleBuffer.clear();
    voiceBuffer.clear();
    std::fill(interleavedVoiceBuffer.begin(), interleavedVoiceBuffer.end(), 0.0f);
}

void BsendorferMicDSP::setSplitParameters(float centerMidiNote, float overlapSemitones) noexcept
{
    splitCenterNote.store(centerMidiNote, std::memory_order_relaxed);
    overlapWidth.store(std::max(1.0f, overlapSemitones), std::memory_order_relaxed);
}

bool BsendorferMicDSP::loadSoundfont(const void* data, int sizeInBytes)
{
    if (gSoundFont != nullptr)
    {
        tsf_close(gSoundFont);
        gSoundFont = nullptr;
    }

    gSoundFont = tsf_load_memory(data, sizeInBytes);
    if (!gSoundFont)
        return false;

    tsf_set_output(gSoundFont, TSF_STEREO_INTERLEAVED, static_cast<int>(sampleRate), 0.0f);
    return true;
}

void BsendorferMicDSP::loadBassIR(const void* wavData, size_t dataSize)
{
    bassConvolution.loadImpulseResponse(
        wavData,
        dataSize,
        juce::dsp::Convolution::Stereo::yes,
        juce::dsp::Convolution::Trim::yes,
        1024, // Keep kernel short to prevent latency/smear
        juce::dsp::Convolution::Normalise::yes
    );
}

void BsendorferMicDSP::loadTrebleIR(const void* wavData, size_t dataSize)
{
    trebleConvolution.loadImpulseResponse(
        wavData,
        dataSize,
        juce::dsp::Convolution::Stereo::yes,
        juce::dsp::Convolution::Trim::yes,
        1024,
        juce::dsp::Convolution::Normalise::yes
    );
}

inline void BsendorferMicDSP::calculateGainsForNote(int midiNote, float& outBassGain, float& outTrebleGain) const noexcept
{
    const float center  = splitCenterNote.load(std::memory_order_relaxed);
    const float overlap = overlapWidth.load(std::memory_order_relaxed);
    const float half    = overlap * 0.5f;

    const float note = static_cast<float>(midiNote);

    // Normalized progress across the crossover band [0.0 = pure bass, 1.0 = pure treble]
    const float t = std::clamp((note - (center - half)) / overlap, 0.0f, 1.0f);

    // Equal-power crossfade (cos^2 + sin^2 = 1.0 energy preservation)
    outBassGain   = std::cos(t * PI_OVER_2);
    outTrebleGain = std::sin(t * PI_OVER_2);
}

void BsendorferMicDSP::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || gSoundFont == nullptr)
        return;

    const size_t requiredInterleaved = static_cast<size_t>(numSamples) * 2;
    if (interleavedVoiceBuffer.size() < requiredInterleaved)
        interleavedVoiceBuffer.resize(requiredInterleaved, 0.0f);

    // 1. Process MIDI Events into tsf
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        const int ch = juce::jlimit(0, 15, msg.getChannel() - 1);
        if (msg.isNoteOn())
        {
            tsf_channel_note_on(gSoundFont, ch, msg.getNoteNumber(), msg.getFloatVelocity());
        }
        else if (msg.isNoteOff())
        {
            tsf_channel_note_off(gSoundFont, ch, msg.getNoteNumber());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            tsf_note_off_all(gSoundFont);
        }
        else if (msg.isPitchWheel())
        {
            tsf_channel_set_pitchwheel(gSoundFont, ch, msg.getPitchWheelValue());
        }
        else if (msg.isController())
        {
            tsf_channel_midi_control(gSoundFont, ch, msg.getControllerNumber(), msg.getControllerValue());
        }
    }

    // 2. Fast Path: If Mic Section is Bypassed, render tsf directly into destination buffer
    if (micSectionBypassed.load(std::memory_order_relaxed))
    {
        tsf_render_float(gSoundFont, interleavedVoiceBuffer.data(), numSamples, 0);

        float* leftChannel  = buffer.getWritePointer(0);
        float* rightChannel = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            leftChannel[i] = interleavedVoiceBuffer[static_cast<size_t>(i * 2)];
            if (rightChannel)
                rightChannel[i] = interleavedVoiceBuffer[static_cast<size_t>(i * 2 + 1)];
        }
        return;
    }

    // 3. Clear accumulation buffers for the crossover
    if (bassBuffer.getNumSamples() < numSamples)
        bassBuffer.setSize(2, numSamples, false, false, true);
    if (trebleBuffer.getNumSamples() < numSamples)
        trebleBuffer.setSize(2, numSamples, false, false, true);

    bassBuffer.clear(0, numSamples);
    trebleBuffer.clear(0, numSamples);

    // Iterate through active tsf voices to route per-note into Bass / Treble sub-mixes
    const int totalVoices = tsf_get_voice_count_ext(gSoundFont);

    for (int v = 0; v < totalVoices; ++v)
    {
        if (!tsf_voice_is_playing_ext(gSoundFont, v))
            continue;

        const int noteNumber = tsf_voice_get_midi_key_ext(gSoundFont, v); // Bösendorfer range: 12 (C0) to 108 (C8)

        float bassGain = 0.0f;
        float trebleGain = 0.0f;
        calculateGainsForNote(noteNumber, bassGain, trebleGain);

        // Render only this active voice into temporary interleaved storage
        std::fill(interleavedVoiceBuffer.begin(),
                  interleavedVoiceBuffer.begin() + static_cast<ptrdiff_t>(requiredInterleaved),
                  0.0f);
        tsf_voice_render_float_ext(gSoundFont, v, interleavedVoiceBuffer.data(), numSamples);

        // De-interleave and accumulate directly into Bass and Treble buffers scaled by equal-power gains
        float* bassL   = bassBuffer.getWritePointer(0);
        float* bassR   = bassBuffer.getWritePointer(1);
        float* trebleL = trebleBuffer.getWritePointer(0);
        float* trebleR = trebleBuffer.getWritePointer(1);

        for (int i = 0; i < numSamples; ++i)
        {
            const float sampleL = interleavedVoiceBuffer[static_cast<size_t>(i * 2)];
            const float sampleR = interleavedVoiceBuffer[static_cast<size_t>(i * 2 + 1)];

            if (bassGain > 0.0001f)
            {
                bassL[i] += sampleL * bassGain;
                bassR[i] += sampleR * bassGain;
            }

            if (trebleGain > 0.0001f)
            {
                trebleL[i] += sampleL * trebleGain;
                trebleR[i] += sampleR * trebleGain;
            }
        }
    }

    // 4. Run through Convolution Blocks
    juce::dsp::AudioBlock<float> bassBlock(bassBuffer);
    juce::dsp::AudioBlock<float> trebleBlock(trebleBuffer);

    auto bassSubBlock   = bassBlock.getSubBlock(0, static_cast<size_t>(numSamples));
    auto trebleSubBlock = trebleBlock.getSubBlock(0, static_cast<size_t>(numSamples));

    juce::dsp::ProcessContextReplacing<float> bassContext(bassSubBlock);
    juce::dsp::ProcessContextReplacing<float> trebleContext(trebleSubBlock);

    bassConvolution.process(bassContext);
    trebleConvolution.process(trebleContext);

    // 5. Sum Both Engines into the Output Buffer with Stereo Crossfeed Bleed
    const float xfeed = crossfeedAmount.load(std::memory_order_relaxed);
    const float directGain = 1.0f - (xfeed * 0.5f);

    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : outL;

    const float* bL = bassBuffer.getReadPointer(0);
    const float* bR = bassBuffer.getReadPointer(1);
    const float* tL = trebleBuffer.getReadPointer(0);
    const float* tR = trebleBuffer.getReadPointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        // Direct sum of both microphone zones
        const float sumL = bL[i] + tL[i];
        const float sumR = bR[i] + tR[i];

        // Apply acoustic crossfeed between left/right soundboard zones
        outL[i] = (sumL * directGain) + (sumR * xfeed);
        outR[i] = (sumR * directGain) + (sumL * xfeed);
    }
}

