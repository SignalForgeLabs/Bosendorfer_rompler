#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "DamperClunkModel.h"
#include "AssetShield.h"
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

        damperClunkModel.prepare(sampleRate);
        synthesizeMechanicalSamples();
        updateLidFilter();
        reset();
    }

    void reset()
    {
        bassMicConv.reset();
        trebleMicConv.reset();
        roomConv.reset();
        lidFilter.reset();

        damperClunkModel.reset();
        liftPlaybackPos.store(-1, std::memory_order_relaxed);
        keyNoisePlaybackPos.store(-1, std::memory_order_relaxed);
    }

    static float applyVelocityCurve(float velocityIn, float curveExponent) noexcept
    {
        const float clampedVel = juce::jlimit(0.0f, 1.0f, velocityIn);
        const float clampedCurve = juce::jlimit(0.25f, 3.0f, curveExponent);
        return juce::jlimit(0.005f, 1.0f, std::pow(clampedVel, clampedCurve));
    }

    // Triggered on fast sustain pedal release (CC64 drops >=64 to <64 in <100 ms)
    void triggerMechanicalClunkSample(float velocityScalar, float masterPedalNoiseGain) noexcept
    {
        damperClunkModel.trigger(velocityScalar, masterPedalNoiseGain);
    }

    void triggerMechanicalClunkSample(float gain) noexcept
    {
        damperClunkModel.trigger(1.0f, gain);
    }

    // Triggered on sustain pedal press (CC64 rises <64 to >=64):
    // Soft 97-damper felt lift whoosh + faint sympathetic string harmonic ring
    void triggerPedalLiftSample(float gain) noexcept
    {
        liftPlaybackGain.store(juce::jlimit(0.0f, 2.0f, gain), std::memory_order_relaxed);
        liftPlaybackPos.store(0, std::memory_order_relaxed);
    }

    // Triggered on key strike when Key Noise knob > 0.05
    void triggerKeyNoiseSample(float gain) noexcept
    {
        keyNoisePlaybackGain.store(juce::jlimit(0.0f, 1.5f, gain), std::memory_order_relaxed);
        keyNoisePlaybackPos.store(0, std::memory_order_relaxed);
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
    void loadBassMicImpulse(juce::AudioBuffer<float>&& irBuffer, double irSampleRate)
    {
        bassMicConv.loadImpulseResponse(
            std::move(irBuffer),
            irSampleRate,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::no,
            juce::dsp::Convolution::Normalise::yes);
    }

    void loadTrebleMicImpulse(juce::AudioBuffer<float>&& irBuffer, double irSampleRate)
    {
        trebleMicConv.loadImpulseResponse(
            std::move(irBuffer),
            irSampleRate,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::no,
            juce::dsp::Convolution::Normalise::yes);
    }

    void loadRoomImpulse(const void* data, size_t size)
    {
        AssetShield::loadImpulseResponse(
            roomConv,
            data,
            size,
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::yes,
            0);
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
            // Feed any pre-convolution mechanical signal in outputBuffer (e.g. DamperClunkModel)
            // into the Bass (Left/Center rail) and Treble mic channels so it reflects through both Mic IRs and Hall IR
            bufferBass.addFrom(0, 0, outputBuffer, 0, 0, numSamples, 0.75f);
            bufferBass.addFrom(1, 0, outputBuffer, 1, 0, numSamples, 0.75f);
            bufferTreble.addFrom(0, 0, outputBuffer, 0, 0, numSamples, 0.25f);
            bufferTreble.addFrom(1, 0, outputBuffer, 1, 0, numSamples, 0.25f);

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

        // 3.5 Synthesize & Mix Physical Mechanical Damper Clunk, Damper Lift Whoosh, and Keybed Thump
        // Added directly into the stereo buffer prior to the lid filter & room convolution stage
        damperClunkModel.processAndAdd(outputBuffer, numSamples);
        mixActiveSample(outputBuffer, damperLiftWhooshSample, liftPlaybackPos, liftPlaybackGain, numSamples);
        mixActiveSample(outputBuffer, keybedThumpSample, keyNoisePlaybackPos, keyNoisePlaybackGain, numSamples);

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
    static void mixActiveSample(juce::AudioBuffer<float>& dest,
                                const juce::AudioBuffer<float>& sampleBuf,
                                std::atomic<int>& posAtomic,
                                const std::atomic<float>& gainAtomic,
                                int numSamples) noexcept
    {
        int pos = posAtomic.load(std::memory_order_relaxed);
        if (pos < 0)
            return;

        const int sampleLen = sampleBuf.getNumSamples();
        if (pos >= sampleLen)
        {
            posAtomic.store(-1, std::memory_order_relaxed);
            return;
        }

        const float gain = gainAtomic.load(std::memory_order_relaxed);
        const int toCopy = juce::jmin(numSamples, sampleLen - pos);

        dest.addFrom(0, 0, sampleBuf, 0, pos, toCopy, gain);
        dest.addFrom(1, 0, sampleBuf, 1, pos, toCopy, gain);

        pos += toCopy;
        posAtomic.store(pos < sampleLen ? pos : -1, std::memory_order_relaxed);
    }

    void synthesizeMechanicalSamples()
    {
        const double sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        const double twoPi = juce::MathConstants<double>::twoPi;
        uint32_t rng = 0x9E3779B9u;

        // 1. Damper Lift Whoosh + Sympathetic String Resonance Wash (~520 ms stereo)
        const int liftSamples = static_cast<int>(sr * 0.52);
        damperLiftWhooshSample.setSize(2, liftSamples);
        damperLiftWhooshSample.clear();

        float* liftL = damperLiftWhooshSample.getWritePointer(0);
        float* liftR = damperLiftWhooshSample.getWritePointer(1);
        float whooshLpL = 0.0f;
        float whooshLpR = 0.0f;

        for (int i = 0; i < liftSamples; ++i)
        {
            const double t = static_cast<double>(i) / sr;
            const double whooshEnv = (1.0 - std::exp(-t / 0.032)) * std::exp(-t / 0.130);
            const double ringEnv   = (1.0 - std::exp(-t / 0.055)) * std::exp(-t / 0.240);

            rng = rng * 1664525u + 1013904223u;
            const float nL = (static_cast<float>(rng & 0xFFFFu) / 32768.0f) - 1.0f;
            rng = rng * 1664525u + 1013904223u;
            const float nR = (static_cast<float>(rng & 0xFFFFu) / 32768.0f) - 1.0f;

            whooshLpL += 0.035f * (nL - whooshLpL);
            whooshLpR += 0.035f * (nR - whooshLpR);

            // Faint open-string sympathetic harmonic wash
            const double sympL = 0.35 * std::sin(twoPi * 55.0 * t)
                               + 0.28 * std::sin(twoPi * 110.0 * t + 0.6)
                               + 0.20 * std::sin(twoPi * 165.0 * t + 1.3)
                               + 0.14 * std::sin(twoPi * 220.0 * t + 2.1);
            const double sympR = 0.32 * std::sin(twoPi * 55.0 * t + 0.3)
                               + 0.28 * std::sin(twoPi * 110.2 * t + 1.1)
                               + 0.22 * std::sin(twoPi * 220.0 * t + 0.4)
                               + 0.15 * std::sin(twoPi * 330.0 * t + 1.8);

            liftL[i] = static_cast<float>(0.18 * static_cast<double>(whooshLpL) * whooshEnv + 0.09 * sympL * ringEnv);
            liftR[i] = static_cast<float>(0.18 * static_cast<double>(whooshLpR) * whooshEnv + 0.09 * sympR * ringEnv);
        }

        // 3. Keybed Action Felt Thump (~85 ms stereo)
        const int keySamples = static_cast<int>(sr * 0.085);
        keybedThumpSample.setSize(2, keySamples);
        keybedThumpSample.clear();

        float* keyL = keybedThumpSample.getWritePointer(0);
        float* keyR = keybedThumpSample.getWritePointer(1);
        for (int i = 0; i < keySamples; ++i)
        {
            const double t = static_cast<double>(i) / sr;
            const double env = (1.0 - std::exp(-t / 0.0012)) * std::exp(-t / 0.018);
            const double thump = 0.65 * std::sin(twoPi * 88.0 * t) + 0.35 * std::sin(twoPi * 154.0 * t + 0.5);
            const float sampleVal = static_cast<float>(thump * env * 0.16);
            keyL[i] = sampleVal;
            keyR[i] = sampleVal * 0.95f;
        }
    }

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

    // Real-time physical DamperClunkModel + pre-synthesized lift whoosh & keybed samples
    DamperClunkModel damperClunkModel;
    juce::AudioBuffer<float> damperLiftWhooshSample;
    juce::AudioBuffer<float> keybedThumpSample;

    std::atomic<int>   liftPlaybackPos { -1 };
    std::atomic<float> liftPlaybackGain { 0.0f };
    std::atomic<int>   keyNoisePlaybackPos { -1 };
    std::atomic<float> keyNoisePlaybackGain { 0.0f };
};
