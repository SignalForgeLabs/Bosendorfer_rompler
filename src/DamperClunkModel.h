#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

class AuthenticDamperClunk
{
public:
    AuthenticDamperClunk() = default;

    void prepare(double sampleRate)
    {
        currentSampleRate = sampleRate;
        
        // Low-pass filter for the felt impact transient
        feltFilter.setCoefficients(juce::IIRCoefficients::makeLowPass(sampleRate, 2000.0, 0.707));
        
        // Band-pass filter for the wooden guide-rail knock
        woodFilter.setCoefficients(juce::IIRCoefficients::makeBandPass(sampleRate, 400.0, 1.8));

        reset();
    }

    void reset()
    {
        phaseMode1 = 0.0;
        phaseMode2 = 0.0;
        bodyEnv = 0.0f;
        woodEnv = 0.0f;
        feltEnv = 0.0f;
        feltFilter.reset();
        woodFilter.reset();
    }

    void trigger(float releaseVelocity, float masterGain)
    {
        // Scale with release velocity and UI pedal noise knob
        float baseAmp = releaseVelocity * masterGain;

        bodyEnv = 0.70f * baseAmp;
        woodEnv = 0.45f * baseAmp;
        feltEnv = 0.25f * baseAmp;

        // Soundboard modal frequencies (two coupled modes that produce authentic wood beating)
        float mode1Freq = 72.0f + (random.nextFloat() * 4.0f);
        float mode2Freq = 84.0f + (random.nextFloat() * 4.0f);

        phaseIncMode1 = (juce::MathConstants<double>::twoPi * mode1Freq) / currentSampleRate;
        phaseIncMode2 = (juce::MathConstants<double>::twoPi * mode2Freq) / currentSampleRate;

        // Exponential decay constants calibrated to real mechanical materials
        bodyDecay = std::exp(-1.0f / (float)(currentSampleRate * 0.120)); // ~120ms soundboard ring
        woodDecay = std::exp(-1.0f / (float)(currentSampleRate * 0.028)); // ~28ms wood frame knock
        feltDecay = std::exp(-1.0f / (float)(currentSampleRate * 0.010)); // ~10ms dull felt slap
    }

    bool isActive() const { return bodyEnv > 0.0001f; }

    void processAndAdd(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        if (!isActive()) return;

        float* left = buffer.getWritePointer(0);
        float* right = buffer.getWritePointer(1);

        for (int i = 0; i < numSamples; ++i)
        {
            if (bodyEnv <= 0.0001f)
            {
                bodyEnv = 0.0f;
                break;
            }

            // 1. Felt impact slap
            float rawNoise = random.nextFloat() * 2.0f - 1.0f;
            float feltSound = feltFilter.processSingleSampleRaw(rawNoise * feltEnv);

            // 2. Hard maple guide-rail knock
            float woodSound = woodFilter.processSingleSampleRaw(rawNoise * woodEnv);

            // 3. Soundboard dual-mode resonant thump
            float bodySound = (std::sin(phaseMode1) * 0.6f + std::sin(phaseMode2) * 0.4f) * bodyEnv;

            // Advance phases
            phaseMode1 += phaseIncMode1;
            if (phaseMode1 >= juce::MathConstants<double>::twoPi) phaseMode1 -= juce::MathConstants<double>::twoPi;

            phaseMode2 += phaseIncMode2;
            if (phaseMode2 >= juce::MathConstants<double>::twoPi) phaseMode2 -= juce::MathConstants<double>::twoPi;

            // Advance envelope decays
            bodyEnv *= bodyDecay;
            woodEnv *= woodDecay;
            feltEnv *= feltDecay;

            // Sum mechanical layers
            float clunkSample = (bodySound + woodSound + feltSound);

            // Inject into stereo spread before convolution
            left[i]  += clunkSample * 0.65f;
            right[i] += clunkSample * 0.50f;
        }
    }

private:
    double currentSampleRate { 44100.0 };
    double phaseMode1 { 0.0 };
    double phaseMode2 { 0.0 };
    double phaseIncMode1 { 0.0 };
    double phaseIncMode2 { 0.0 };

    float bodyEnv { 0.0f };
    float woodEnv { 0.0f };
    float feltEnv { 0.0f };

    float bodyDecay { 0.999f };
    float woodDecay { 0.985f };
    float feltDecay { 0.970f };

    juce::IIRFilter feltFilter;
    juce::IIRFilter woodFilter;
    juce::Random random;
};

using DamperClunkModel = AuthenticDamperClunk;
