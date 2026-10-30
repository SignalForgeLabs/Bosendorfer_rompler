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
        g.setColour(juce::Colour(0xfff0c43f).withAlpha(0.80f)); // Amber gold
        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.drawText("MIC BLEED & PHASE", headerArea, juce::Justification::centred);

        // 45-degree diagonal L/R reference axes
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawLine(bounds.getX(), bounds.getY(), bounds.getRight(), bounds.getBottom(), 0.5f);
        g.drawLine(bounds.getX(), bounds.getBottom(), bounds.getRight(), bounds.getY(), 0.5f);
        g.drawLine(bounds.getCentreX(), bounds.getY(), bounds.getCentreX(), bounds.getBottom(), 0.4f);

        auto center = bounds.getCentre();
        float maxRadius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.48f;

        // Trace the stereo samples (M/S rotated goniometer: X = (L-R)*0.707, Y = (L+R)*0.707)
        juce::Path vectorPath;
        bool pathStarted = false;

        const int readStart = writeIndex;
        constexpr float kVisualGain = 3.5f;
        for (int i = 0; i < kBufferSize; i += 2)
        {
            int idx = (readStart + i) % kBufferSize;
            auto [leftSample, rightSample] = sampleRingBuffer[idx];

            float side = (leftSample - rightSample) * 0.7071f * kVisualGain;
            float mid  = (leftSample + rightSample) * 0.7071f * kVisualGain;

            float px = center.x + juce::jlimit(-1.0f, 1.0f, side) * maxRadius;
            float py = center.y - juce::jlimit(-1.0f, 1.0f, mid) * maxRadius;

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

        // Draw phosphor-green scope trace
        g.setColour(juce::Colour(0xff2be482).withAlpha(0.80f));
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

