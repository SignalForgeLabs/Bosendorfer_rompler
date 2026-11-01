#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <utility>
#include <cmath>

class CenterMonitorView : public juce::Component, public juce::Timer
{
public:
    CenterMonitorView(juce::AudioProcessorValueTreeState& state, const juce::String& velCurveId)
        : apvts(state), velCurveParamID(velCurveId)
    {
        cachedCurve = apvts.getRawParameterValue(velCurveParamID)->load();
        startTimerHz(30); // 30 FPS repaint rate
    }

    ~CenterMonitorView() override
    {
        stopTimer();
    }

    void setMicViewMode(bool showMics)
    {
        if (isMicMode != showMics)
        {
            isMicMode = showMics;
            repaint();
        }
    }

    bool isMicViewMode() const noexcept { return isMicMode; }

    // Called from the audio thread or FIFO reader to pass post-convolution samples
    void pushStereoSample(float left, float right)
    {
        sampleRingBuffer[writeIndex] = { left, right };
        writeIndex = (writeIndex + 1) % kBufferSize;
    }

    void setLastHitVelocity(float vel)
    {
        lastHitVel = juce::jlimit(0.0f, 1.0f, vel);
        hitDotAlpha = 1.0f;
    }

    void timerCallback() override
    {
        if (!isMicMode)
        {
            // Velocity dot decay
            if (hitDotAlpha > 0.0f)
            {
                hitDotAlpha -= 0.07f;
                if (hitDotAlpha < 0.0f) hitDotAlpha = 0.0f;
            }

            float currentCurve = apvts.getRawParameterValue(velCurveParamID)->load();
            if (std::abs(currentCurve - cachedCurve) > 0.005f || hitDotAlpha > 0.0f)
            {
                cachedCurve = currentCurve;
                repaint();
            }
        }
        else
        {
            // Vector Lissajous scope continually repaints while active
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.5f);

        // Recessed chassis display plate
        g.setColour(juce::Colour(0xff101214));
        g.fillRoundedRectangle(bounds, 3.0f);

        // Subtle dark grid border
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

        if (!isMicMode)
        {
            drawVelocityCurve(g, bounds.reduced(2.5f));
        }
        else
        {
            drawMicVectors(g, bounds.reduced(2.5f));
        }
    }

private:
    void drawVelocityCurve(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        // Grid lines
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawLine(bounds.getX(), bounds.getCentreY(), bounds.getRight(), bounds.getCentreY(), 0.5f);
        g.drawLine(bounds.getCentreX(), bounds.getY(), bounds.getCentreX(), bounds.getBottom(), 0.5f);

        // Mode header label
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.drawText("TOUCH RESPONSE", bounds.removeFromTop(11.0f), juce::Justification::centred);

        // Compute curve (vel_curve parameter spans 0.5 to 2.0, with 1.0 = linear)
        juce::Path curvePath;
        juce::Path fillPath;
        const int steps = 32;
        const float exponent = juce::jlimit(0.25f, 3.0f, cachedCurve);

        fillPath.startNewSubPath(bounds.getX(), bounds.getBottom());

        for (int i = 0; i <= steps; ++i)
        {
            float inX = (float)i / (float)steps;
            float outY = std::pow(inX, exponent);

            float px = bounds.getX() + inX * bounds.getWidth();
            float py = bounds.getBottom() - outY * bounds.getHeight();

            fillPath.lineTo(px, py);
            if (i == 0) curvePath.startNewSubPath(px, py);
            else        curvePath.lineTo(px, py);
        }

        fillPath.lineTo(bounds.getRight(), bounds.getBottom());
        fillPath.closeSubPath();

        g.setColour(juce::Colour(0xff4fa3e3).withAlpha(0.18f));
        g.fillPath(fillPath);

        g.setColour(juce::Colour(0xff4fa3e3)); // Studio blue trace
        g.strokePath(curvePath, juce::PathStrokeType(1.6f));

        if (hitDotAlpha > 0.01f)
        {
            float outY = std::pow(lastHitVel, exponent);
            float dotX = bounds.getX() + lastHitVel * bounds.getWidth();
            float dotY = bounds.getBottom() - outY * bounds.getHeight();

            g.setColour(juce::Colours::orange.withAlpha(hitDotAlpha));
            g.fillEllipse(dotX - 3.0f, dotY - 3.0f, 6.0f, 6.0f);
        }
    }

    void drawMicVectors(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        // Mode header label
        auto headerArea = bounds.removeFromTop(11.0f);
        g.setColour(juce::Colour(0xfff0c43f).withAlpha(0.85f)); // Amber gold
        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.drawText("MIC IR & PHASE SCOPE", headerArea, juce::Justification::centred);

        // Full-width reference grid axes
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawLine(bounds.getX(), bounds.getY(), bounds.getRight(), bounds.getBottom(), 0.5f);
        g.drawLine(bounds.getX(), bounds.getBottom(), bounds.getRight(), bounds.getY(), 0.5f);
        g.drawLine(bounds.getX(), bounds.getCentreY(), bounds.getRight(), bounds.getCentreY(), 0.5f);
        g.drawLine(bounds.getCentreX(), bounds.getY(), bounds.getCentreX(), bounds.getBottom(), 0.4f);

        const auto center = bounds.getCentre();
        const float radiusX = bounds.getWidth() * 0.48f;
        const float radiusY = bounds.getHeight() * 0.46f;

        // 1. Full-width post-IR stereo waveform / envelope trace across the entire display width
        juce::Path wavePathL, wavePathR, fillPath;
        const int readStart = writeIndex;
        constexpr float kWaveGain = 3.2f;
        fillPath.startNewSubPath(bounds.getX(), center.y);

        for (int i = 0; i < kBufferSize; ++i)
        {
            const int idx = (readStart + i) % kBufferSize;
            const auto [leftSample, rightSample] = sampleRingBuffer[idx];
            const float normX = static_cast<float>(i) / static_cast<float>(kBufferSize - 1);
            const float wx = bounds.getX() + normX * bounds.getWidth();
            const float wyL = center.y - juce::jlimit(-1.0f, 1.0f, leftSample * kWaveGain) * radiusY;
            const float wyR = center.y - juce::jlimit(-1.0f, 1.0f, rightSample * kWaveGain) * radiusY;

            fillPath.lineTo(wx, wyL);
            if (i == 0)
            {
                wavePathL.startNewSubPath(wx, wyL);
                wavePathR.startNewSubPath(wx, wyR);
            }
            else
            {
                wavePathL.lineTo(wx, wyL);
                wavePathR.lineTo(wx, wyR);
            }
        }
        fillPath.lineTo(bounds.getRight(), center.y);
        fillPath.closeSubPath();

        g.setColour(juce::Colour(0xff4fa3e3).withAlpha(0.18f));
        g.fillPath(fillPath);

        g.setColour(juce::Colour(0xff4fa3e3).withAlpha(0.85f)); // Studio blue full-width IR trace
        g.strokePath(wavePathL, juce::PathStrokeType(1.3f));

        // 2. Wide M/S stereo phase vector scope spanning the full horizontal radiusX
        juce::Path vectorPath;
        bool pathStarted = false;
        constexpr float kSideGain = 6.5f;
        constexpr float kMidGain  = 3.5f;

        for (int i = 0; i < kBufferSize; i += 2)
        {
            const int idx = (readStart + i) % kBufferSize;
            const auto [leftSample, rightSample] = sampleRingBuffer[idx];

            const float side = (leftSample - rightSample) * 0.7071f * kSideGain;
            const float mid  = (leftSample + rightSample) * 0.7071f * kMidGain;

            const float px = center.x + juce::jlimit(-1.0f, 1.0f, side) * radiusX;
            const float py = center.y - juce::jlimit(-1.0f, 1.0f, mid) * radiusY;

            if (!pathStarted)
            {
                vectorPath.startNewSubPath(px, py);
                pathStarted = true;
            }
            else
            {
                vectorPath.lineTo(px, py);
            }
        }

        // Draw cyan-green stereo phase trace over the full-width blue IR wave
        g.setColour(juce::Colour(0xff2be4c8).withAlpha(0.82f));
        g.strokePath(vectorPath, juce::PathStrokeType(1.2f));
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String velCurveParamID;

    bool isMicMode { false };
    float cachedCurve { 1.0f };
    float lastHitVel { 0.0f };
    float hitDotAlpha { 0.0f };

    static constexpr int kBufferSize = 128;
    std::pair<float, float> sampleRingBuffer[kBufferSize] {};
    int writeIndex { 0 };
};

