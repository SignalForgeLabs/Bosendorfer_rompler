#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

class PianoAcousticEngine
{
public:
    PianoAcousticEngine() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
        maxBlockSize = juce::jmax(1u, spec.maximumBlockSize);

        juce::dsp::ProcessSpec stereoSpec { sampleRate, maxBlockSize, 2 };

        bassMicConv.prepare(stereoSpec);
        trebleMicConv.prepare(stereoSpec);
        roomConv.prepare(stereoSpec);

        lidFilter.prepare(stereoSpec);
        lidFilter.reset();

        roomSendBuffer.setSize(2, static_cast<int>(maxBlockSize));
        roomSendBuffer.clear();

        updateLidFilter();
        reset();
    }

    void reset()
    {
        bassMicConv.reset();
        trebleMicConv.reset();
        roomConv.reset();
        lidFilter.reset();
    }

    static float applyVelocityCurve(float velocityIn, float curveExponent) noexcept
    {
        const float clampedVel = juce::jlimit(0.0f, 1.0f, velocityIn);
        const float clampedCurve = juce::jlimit(0.25f, 3.0f, curveExponent);
        return juce::jlimit(0.005f, 1.0f, std::pow(clampedVel, clampedCurve));
    }

    void setMicSectionBypassed(bool bypassed) noexcept
    {
        micSectionBypassed.store(bypassed, std::memory_order_relaxed);
    }

    bool isMicSectionBypassed() const noexcept
    {
        return micSectionBypassed.load(std::memory_order_relaxed);
    }

    void setLidPosition(float normalizedLid, bool softPedal = false)
    {
        const float clamped = juce::jlimit(0.0f, 1.0f, normalizedLid);
        if (std::abs(clamped - lidPosition) > 1.0e-4f || softPedal != softPedalActive)
        {
            lidPosition = clamped;
            softPedalActive = softPedal;
            updateLidFilter();
        }
    }

    // Load 1024-tap zero-latency FIR WAV from embedded binary memory (or file)
    // juce::dsp::Convolution loads asynchronously on its internal background thread
    void loadBassMicImpulse(const void* data, size_t size)
    {
        bassMicConv.loadImpulseResponse(
            data, size,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::no,
            1024,
            juce::dsp::Convolution::Normalise::yes);
    }

    void loadTrebleMicImpulse(const void* data, size_t size)
    {
        trebleMicConv.loadImpulseResponse(
            data, size,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::no,
            1024,
            juce::dsp::Convolution::Normalise::yes);
    }

    void loadRoomImpulse(const void* data, size_t size)
    {
        roomConv.loadImpulseResponse(
            data, size,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::yes,
            0,
            juce::dsp::Convolution::Normalise::yes);
    }

    // Process split Bass (Mic A) and Treble (Mic B) buffers, or bypass directly
    void processSplit(juce::AudioBuffer<float>& outputBuffer,
                      juce::AudioBuffer<float>& bufferBass,
                      juce::AudioBuffer<float>& bufferTreble,
                      float bassGainDb,
                      float trebleGainDb,
                      float crossfeed,
                      float roomWet)
    {
        const int numSamples = outputBuffer.getNumSamples();
        if (numSamples <= 0 || outputBuffer.getNumChannels() < 2)
            return;

        if (!micSectionBypassed.load(std::memory_order_relaxed))
        {
            // 1. Convolve Bass Zone (bufferBass) through Mic A FIR
            {
                juce::dsp::AudioBlock<float> bassBlock(bufferBass);
                auto subBlock = bassBlock.getSubBlock(0, static_cast<size_t>(numSamples));
                juce::dsp::ProcessContextReplacing<float> ctx(subBlock);
                bassMicConv.process(ctx);
            }

            // 2. Convolve Treble Zone (bufferTreble) through Mic B FIR
            {
                juce::dsp::AudioBlock<float> trebleBlock(bufferTreble);
                auto subBlock = trebleBlock.getSubBlock(0, static_cast<size_t>(numSamples));
                juce::dsp::ProcessContextReplacing<float> ctx(subBlock);
                trebleMicConv.process(ctx);
            }

            const float gBass = juce::Decibels::decibelsToGain(bassGainDb);
            const float gTreble = juce::Decibels::decibelsToGain(trebleGainDb);

            const float* bassL = bufferBass.getReadPointer(0);
            const float* bassR = bufferBass.getReadPointer(1);
            const float* trebL = bufferTreble.getReadPointer(0);
            const float* trebR = bufferTreble.getReadPointer(1);

            float* outL = outputBuffer.getWritePointer(0);
            float* outR = outputBuffer.getWritePointer(1);

            // 3. Sum bufferBass + bufferTreble and apply adjustable stereo crossfeed bleed matrix
            const float directGain = 1.0f - (crossfeed * 0.5f);
            for (int i = 0; i < numSamples; ++i)
            {
                const float dryL = bassL[i] * gBass + trebL[i] * gTreble;
                const float dryR = bassR[i] * gBass + trebR[i] * gTreble;

                outL[i] = (dryL * directGain) + (dryR * crossfeed);
                outR[i] = (dryR * directGain) + (dryL * crossfeed);
            }
        }

        // 4. Apply Lid Position & Una Corda acoustic contour
        {
            juce::dsp::AudioBlock<float> lidBlock(outputBuffer);
            juce::dsp::ProcessContextReplacing<float> lidCtx(lidBlock);
            lidFilter.process(lidCtx);
        }

        // 5. Global Room Stage Convolution Send
        if (roomWet > 0.001f)
        {
            if (roomSendBuffer.getNumSamples() < numSamples)
                roomSendBuffer.setSize(2, numSamples, false, false, true);

            roomSendBuffer.copyFrom(0, 0, outputBuffer, 0, 0, numSamples);
            roomSendBuffer.copyFrom(1, 0, outputBuffer, 1, 0, numSamples);

            juce::dsp::AudioBlock<float> roomBlock(roomSendBuffer);
            auto roomSubBlock = roomBlock.getSubBlock(0, static_cast<size_t>(numSamples));
            juce::dsp::ProcessContextReplacing<float> roomContext(roomSubBlock);
            roomConv.process(roomContext);

            for (int ch = 0; ch < 2; ++ch)
                outputBuffer.addFrom(ch, 0, roomSendBuffer, ch, 0, numSamples, roomWet);
        }
    }

private:
    void updateLidFilter()
    {
        float effectiveLid = lidPosition;
        if (softPedalActive)
            effectiveLid *= 0.65f;

        const float minCutoff = 1600.0f;
        const float maxCutoff = static_cast<float>(juce::jmin(20000.0, sampleRate * 0.46));
        const float cutoffHz = minCutoff * std::pow(maxCutoff / minCutoff, effectiveLid);

        *lidFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(
            sampleRate, juce::jlimit(200.0f, maxCutoff, cutoffHz), 0.7071f);
    }

    double sampleRate { 44100.0 };
    juce::uint32 maxBlockSize { 512 };
    float lidPosition { 1.0f };
    bool softPedalActive { false };
    std::atomic<bool> micSectionBypassed { false };

    juce::dsp::Convolution bassMicConv;
    juce::dsp::Convolution trebleMicConv;
    juce::dsp::Convolution roomConv;

    using StereoIIR = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;
    StereoIIR lidFilter;

    juce::AudioBuffer<float> roomSendBuffer;
};
