#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PianoHarpViewComponent.h"
#include "CenterMonitorView.h"
#include <array>

//==============================================================================
// Layer 3 Bottom Component: 97-Key Bösendorfer 290 Imperial Vector Keyboard
//==============================================================================
class ImperialKeyboardComponent : public juce::MidiKeyboardComponent
{
public:
    ImperialKeyboardComponent(juce::MidiKeyboardState& kbState, Orientation kbOrientation)
        : juce::MidiKeyboardComponent(kbState, kbOrientation)
    {
        // Bösendorfer 290 Imperial 97-key range: C0 (MIDI 12) to C8 (MIDI 108)
        setAvailableRange(12, 108);
        setOctaveForMiddleC(4);
    }

    void setSplitZoneOverlay(int centerNote, float widthSemitones, bool showSplit)
    {
        if (splitCenterNote != centerNote || std::abs(splitWidthSemitones - widthSemitones) > 0.05f || showSplitZone != showSplit)
        {
            splitCenterNote = centerNote;
            splitWidthSemitones = widthSemitones;
            showSplitZone = showSplit;
            repaint();
        }
    }

protected:
    void drawWhiteNote(int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                       bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour) override
    {
        const bool isSubContra = (midiNoteNumber < 21);

        juce::Colour baseCol;
        if (isSubContra)
        {
            baseCol = isDown ? juce::Colour(0xff181a1e) : juce::Colour(0xff22252a);
            if (isOver && !isDown)
                baseCol = baseCol.brighter(0.14f);
        }
        else
        {
            baseCol = isDown ? juce::Colour(0xffdcd8cf) : juce::Colour(0xfff5f3eb);
            if (isOver && !isDown)
                baseCol = baseCol.brighter(0.08f);
        }

        juce::ColourGradient grad(baseCol.brighter(0.05f), area.getX(), area.getY(),
                                  baseCol.darker(0.10f), area.getX(), area.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRect(area);

        if (isDown)
        {
            g.setColour(juce::Colour(0xffd4af37).withAlpha(0.38f));
            g.fillRect(area);
        }

        if (showSplitZone)
        {
            const float halfW = splitWidthSemitones * 0.5f;
            const float lowNote = static_cast<float>(splitCenterNote) - halfW;
            const float highNote = static_cast<float>(splitCenterNote) + halfW;
            if (static_cast<float>(midiNoteNumber) >= lowNote && static_cast<float>(midiNoteNumber) <= highNote)
            {
                const float norm = (splitWidthSemitones > 0.001f)
                                       ? ((static_cast<float>(midiNoteNumber) - lowNote) / splitWidthSemitones)
                                       : 0.5f;
                const auto zoneCol = juce::Colour(0xffd4af37).interpolatedWith(juce::Colour(0xff5ca8ff), norm);
                g.setColour(zoneCol.withAlpha(0.30f));
                g.fillRect(area.withHeight(8.0f));
            }
        }

        if (isSubContra && !isDown)
        {
            g.setColour(juce::Colour(0xff8b1e24));
            g.fillRect(area.getX(), area.getY(), area.getWidth(), 3.5f);
        }

        g.setColour(lineColour.withAlpha(0.65f));
        g.drawRect(area, 0.6f);

        if (midiNoteNumber == 12 || midiNoteNumber == 21 || midiNoteNumber == 53 || midiNoteNumber == 69 || (midiNoteNumber % 12 == 0))
        {
            juce::String label;
            if (midiNoteNumber == 12)
                label = "C0";
            else if (midiNoteNumber == 21)
                label = "A0";
            else if (midiNoteNumber == 53)
                label = "F3";
            else if (midiNoteNumber == 69)
                label = "A4";
            else
                label = "C" + juce::String((midiNoteNumber / 12) - 1);

            g.setColour(isSubContra ? juce::Colour(0xffd4af37) : textColour.withAlpha(0.75f));
            g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            g.drawText(label, area.withTrimmedTop(area.getHeight() - 16.0f),
                       juce::Justification::centredBottom, false);
        }
    }

    void drawBlackNote(int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                       bool isDown, bool isOver, juce::Colour noteFillColour) override
    {
        const bool isSubContra = (midiNoteNumber < 21);

        juce::Colour col = isSubContra
                               ? (isDown ? juce::Colour(0xffc8c8c8) : juce::Colour(0xffe8e8e8))
                               : (isDown ? juce::Colour(0xff2c2519) : noteFillColour);

        if (isOver && !isDown)
            col = col.brighter(0.18f);

        g.setColour(col);
        g.fillRect(area);

        if (isDown)
        {
            g.setColour(juce::Colour(0xffd4af37).withAlpha(0.45f));
            g.fillRect(area);
        }

        g.setColour(juce::Colour(0xff101010));
        g.drawRect(area, 1.0f);

        auto inner = area.reduced(1.8f, 1.8f).withTrimmedBottom(3.5f);
        g.setColour(col.brighter(0.15f));
        g.drawRect(inner, 0.7f);
    }

private:
    int splitCenterNote { 61 };
    float splitWidthSemitones { 16.0f };
    bool showSplitZone { false };
};

//==============================================================================
// Layer 2 Middle Component: Split Left/Right Floating Microphone Overlays
//==============================================================================
class MicOverlayLayer : public juce::Component
{
public:
    MicOverlayLayer();

    void setMicSelections(int bassMicIndex, int trebleMicIndex);
    void setMicGainsAndBleed(float bassGainDb, float trebleGainDb, float crossfeedNorm);
    void setSplitParameters(int splitCenterNote, float splitWidthSemitones, bool bypassed);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Image getLeftMicImage(int index) const;
    juce::Image getRightMicImage(int index) const;
    static juce::String getMicDisplayName(int index);

    std::array<juce::Image, 4> cachedLeftMicImages;
    std::array<juce::Image, 4> cachedRightMicImages;

    // Split Left (Bass) and Right (Treble) floating microphone overlays
    FloatingMicOverlay bassMicOverlay   { "Neuman TLM 103", juce::Colours::goldenrod, false };
    FloatingMicOverlay trebleMicOverlay { "Neumann KM 184", juce::Colours::cyan,      true  };
    bool initialPositionsSet { false };

    int currentBassMic { -1 };
    int currentTrebleMic { -1 };
    float bassDb { 0.0f };
    float trebleDb { 0.0f };
    float crossfeed { 0.15f };
    int splitCenter { 61 };
    float splitWidth { 16.0f };
    bool isBypassed { false };

    // Left and Right split regions within the center harp viewport (654 x 383)
    const juce::Rectangle<int> bassMicBounds   { 4,   34, 316, 278 };
    const juce::Rectangle<int> trebleMicBounds { 334, 34, 316, 278 };
    const juce::Rectangle<int> bannerBounds    { 72, 318, 510, 56  };
};

//==============================================================================
// LookAndFeel Matching Prototype_look_and_feel.png (Metallic Faders & Dark Knobs)
//==============================================================================
class BosendorferLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BosendorferLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff121110));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1c1b1a));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff5a554c));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xffebe6dc));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffd4af37));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1b1917));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xfff2e8c9));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff5c481c));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff23211f));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff7a6020));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe0dad0));
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xfffff9e6));
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe8d8a8));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, const float rotaryStartAngle,
                          const float rotaryEndAngle, juce::Slider&) override
    {
        const float radius = static_cast<float>(juce::jmin(width / 2, height / 2)) - 1.5f;
        const float centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Outer metallic bevel ring matching Prototype_look_and_feel.png
        g.setColour(juce::Colour(0xff6b6863));
        g.drawEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 1.3f);

        const float capRadius = radius * 0.86f;
        juce::ColourGradient capGrad(juce::Colour(0xff383634), centreX, centreY - capRadius,
                                     juce::Colour(0xff161514), centreX, centreY + capRadius, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(centreX - capRadius, centreY - capRadius, capRadius * 2.0f, capRadius * 2.0f);

        // Subtle gold value arc
        juce::Path valArc;
        valArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(juce::Colour(0xffd4af37).withAlpha(0.85f));
        g.strokePath(valArc, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Crisp white/silver indicator tick
        juce::Path pointer;
        const float pointerLen = capRadius * 0.82f;
        pointer.addRoundedRectangle(-1.1f, -pointerLen, 2.2f, pointerLen * 0.55f, 0.8f);
        g.setColour(juce::Colour(0xffefefef));
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                          const juce::Slider::SliderStyle style, juce::Slider&) override
    {
        if (style == juce::Slider::LinearVertical)
        {
            const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
            const float topY = static_cast<float>(y) + 6.0f;
            const float botY = static_cast<float>(y + height) - 6.0f;

            // Recessed vertical slot track
            g.setColour(juce::Colour(0xff141312));
            g.fillRoundedRectangle(cx - 2.0f, topY, 4.0f, botY - topY, 2.0f);
            g.setColour(juce::Colour(0xff45423e));
            g.drawRoundedRectangle(cx - 2.0f, topY, 4.0f, botY - topY, 2.0f, 0.8f);

            // Active fill from bottom up to thumb
            const float thumbY = juce::jlimit(topY, botY, sliderPos);
            g.setColour(juce::Colour(0xff9a8248));
            g.fillRoundedRectangle(cx - 1.5f, thumbY, 3.0f, botY - thumbY, 1.5f);

            // Brushed silver fader cap matching Prototype_look_and_feel.png
            const float capW = 12.0f;
            const float capH = 18.0f;
            juce::Rectangle<float> capRect(cx - capW * 0.5f, thumbY - capH * 0.5f, capW, capH);
            juce::ColourGradient capGrad(juce::Colour(0xffdcdad6), capRect.getX(), capRect.getY(),
                                         juce::Colour(0xff7c7974), capRect.getX(), capRect.getBottom(), false);
            g.setGradientFill(capGrad);
            g.fillRoundedRectangle(capRect, 2.0f);
            g.setColour(juce::Colour(0xff22201e));
            g.drawRoundedRectangle(capRect, 2.0f, 1.0f);
            g.drawHorizontalLine(static_cast<int>(thumbY), capRect.getX() + 2.0f, capRect.getRight() - 2.0f);
        }
    }
};

//==============================================================================
// License Activation Modal Overlay (SignalForge Bosendorfer 290)
//==============================================================================
class LicenseActivationOverlay : public juce::Component
{
public:
    LicenseActivationOverlay()
    {
        emailEditor.setMultiLine(false);
        emailEditor.setTextToShowWhenEmpty("Enter registered email (e.g. user@domain.com)", juce::Colour(0xff807b72));
        emailEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff141312));
        emailEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff5a554c));
        emailEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xfff2e8c9));
        addAndMakeVisible(emailEditor);

        serialEditor.setMultiLine(false);
        serialEditor.setTextToShowWhenEmpty("XXXX-XXXX-XXXX-XXXX", juce::Colour(0xff807b72));
        serialEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff141312));
        serialEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff5a554c));
        serialEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xfff5d76e));
        addAndMakeVisible(serialEditor);

        activateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff7a6020));
        activateButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfffff9e6));
        addAndMakeVisible(activateButton);
        activateButton.onClick = [this]()
        {
            if (LicenseManager::getInstance().activateLicense(emailEditor.getText(), serialEditor.getText()))
            {
                statusMessage = "License activated successfully!";
                statusIsError = false;
                if (onLicenseChanged)
                    onLicenseChanged();
                setVisible(false);
            }
            else
            {
                statusMessage = "Invalid email or serial key. Expected format: XXXX-XXXX-XXXX-XXXX";
                statusIsError = true;
                repaint();
            }
        };

        closeButton.onClick = [this]()
        {
            setVisible(false);
            if (onLicenseChanged)
                onLicenseChanged();
        };
        addAndMakeVisible(closeButton);
    }

    void refreshFromManager()
    {
        if (LicenseManager::getInstance().isLicensed())
        {
            emailEditor.setText(LicenseManager::getInstance().getLicensedEmail(), juce::dontSendNotification);
            statusMessage = "Product is licensed and activated.";
            statusIsError = false;
        }
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xc80a0908));

        auto card = getLocalBounds().toFloat().withSizeKeepingCentre(440.0f, 250.0f);
        g.setColour(juce::Colour(0xff1e1c1a));
        g.fillRoundedRectangle(card, 8.0f);
        g.setColour(juce::Colour(0xffd4af37));
        g.drawRoundedRectangle(card, 8.0f, 1.5f);

        g.setColour(juce::Colour(0xfff5d76e));
        g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        g.drawText("SIGNALFORGE LICENSE ACTIVATION",
                   card.withTop(card.getY() + 14.0f).withHeight(24.0f),
                   juce::Justification::centred);

        g.setColour(juce::Colour(0xffb8b2a6));
        g.setFont(juce::FontOptions(11.5f));
        g.drawText("Bosendorfer Imperial 290  -  Enter Email & Serial Key",
                   card.withTop(card.getY() + 38.0f).withHeight(18.0f),
                   juce::Justification::centred);

        g.setColour(juce::Colour(0xffdcd8d0));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("Email:", static_cast<int>(card.getX() + 26.0f), static_cast<int>(card.getY() + 72.0f), 80, 24, juce::Justification::centredLeft);
        g.drawText("Serial Key:", static_cast<int>(card.getX() + 26.0f), static_cast<int>(card.getY() + 112.0f), 80, 24, juce::Justification::centredLeft);

        if (statusMessage.isNotEmpty())
        {
            g.setColour(statusIsError ? juce::Colour(0xffff6b5b) : juce::Colour(0xff5ce63c));
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.drawText(statusMessage,
                       card.withTop(card.getY() + 150.0f).withHeight(20.0f),
                       juce::Justification::centred);
        }
    }

    void resized() override
    {
        auto card = getLocalBounds().withSizeKeepingCentre(440, 250);
        emailEditor.setBounds(card.getX() + 110, card.getY() + 70, 300, 26);
        serialEditor.setBounds(card.getX() + 110, card.getY() + 110, 300, 26);
        activateButton.setBounds(card.getX() + 110, card.getY() + 188, 144, 30);
        closeButton.setBounds(card.getX() + 266, card.getY() + 188, 144, 30);
    }

    std::function<void()> onLicenseChanged;

private:
    juce::TextEditor emailEditor;
    juce::TextEditor serialEditor;
    juce::TextButton activateButton { "Activate License" };
    juce::TextButton closeButton    { "Close / Demo" };
    juce::String statusMessage;
    bool statusIsError { false };
};

//==============================================================================
// Main Plugin Editor (3-Layer Composited Layout Matching Prototype_look_and_feel.png)
//==============================================================================
class BosendorferAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit BosendorferAudioProcessorEditor(BosendorferAudioProcessor&);
    ~BosendorferAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateLayerState();
    void refreshPresetComboBox();
    void updateBankButtonStates();
    void setupCircleKnob(juce::Slider& slider, const juce::String& tooltipText);
    void setupVerticalFader(juce::Slider& slider, juce::Label& topLbl, juce::Label& botLbl, const juce::String& text);

    BosendorferAudioProcessor& audioProcessor;
    BosendorferLookAndFeel lookAndFeel;

    // Layer 1: Cached Background Plates (1000 x 580)
    juce::Image stageGrandBg;
    juce::Image pianoHarpBg;
    bool isMicSetupView { false };

    // Layer 2: Split Left/Right Floating Microphone Overlay Layer (active in MIC SETUP view)
    MicOverlayLayer micOverlayLayer;

    // Layer 3: Header Bar Controls (Matching Prototype_look_and_feel.png top right)
    juce::TextButton licenseButton   { "License" };
    juce::ComboBox presetSelector;
    juce::TextButton stdSf2Button    { "Standard" };
    juce::TextButton brightSf2Button { "Brite" };
    juce::TextButton viewToggleBtn   { "FX / Mics" };
    LicenseActivationOverlay licenseOverlay;

    // Pedal Buttons
    juce::TextButton unaCordaButton { "Una Corda (CC 67)" };
    juce::TextButton sustainButton  { "Sustain (CC 64)" };

    // Left Panel ("Sound Control"):
    // 1. Upper Box: Reverb Vertical Fader, Lid Open Vertical Fader, and Stereo LED Meter
    juce::Slider reverbFader;
    juce::Label reverbFaderTopLabel;
    juce::Label reverbFaderBotLabel;

    juce::Slider lidFader;
    juce::Label lidFaderTopLabel;
    juce::Label lidFaderBotLabel;

    // 2. 4 Circular Cutout Rotary Knobs (above printed "Reverb", "Lid Open", "Key Noise", "Pedal Noise")
    juce::Slider roomSlider;
    juce::Slider lidPositionSlider;
    juce::Slider keyNoiseSlider;
    juce::Slider pedalNoiseSlider;

    // 3. Bottom Dual-Mode Monitor View (Touch Response Velocity Curve / Mic Bleed & Phase Goniometer)
    CenterMonitorView monitorView;
    juce::Slider velCurveBoxSlider;

    // Center Viewport Top Overlay Controls (Left Mic, Bypass, Right Mic, Room IR)
    juce::ComboBox roomIrSelector;
    juce::ComboBox bassMicSelector;
    juce::ComboBox trebleMicSelector;
    juce::TextButton micBypassButton { "Mics Active" };

    // Right Panel ("Mic & Effects"):
    // 1. Upper Box: 3 Vertical Faders (Left/Close Gain, Mid/Bleed, Right/Far Gain)
    juce::Slider bassMicGainSlider;
    juce::Label bassFaderTopLabel;
    juce::Label bassFaderBotLabel;

    juce::Slider micCrossfeedSlider;
    juce::Label bleedFaderTopLabel;
    juce::Label bleedFaderBotLabel;

    juce::Slider trebleMicGainSlider;
    juce::Label trebleFaderTopLabel;
    juce::Label trebleFaderBotLabel;

    // 2. 3 Circular Cutout Rotary Knobs (above printed "EQ", "Dynamic Range", "Saturation")
    juce::Slider splitCenterSlider;
    juce::Slider splitWidthSlider;
    juce::Slider masterGainSlider;

    // APVTS Attachments
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboBoxAttachment> bassMicAttachment;
    std::unique_ptr<ComboBoxAttachment> trebleMicAttachment;
    std::unique_ptr<ComboBoxAttachment> roomIrAttachment;
    std::unique_ptr<ButtonAttachment>   micBypassAttachment;

    std::unique_ptr<SliderAttachment> reverbFaderAttachment;
    std::unique_ptr<SliderAttachment> lidFaderAttachment;
    std::unique_ptr<SliderAttachment> roomAttachment;
    std::unique_ptr<SliderAttachment> lidPositionAttachment;
    std::unique_ptr<SliderAttachment> keyNoiseAttachment;
    std::unique_ptr<SliderAttachment> pedalNoiseAttachment;
    std::unique_ptr<SliderAttachment> velCurveBoxAttachment;

    std::unique_ptr<SliderAttachment> bassMicGainAttachment;
    std::unique_ptr<SliderAttachment> micCrossfeedAttachment;
    std::unique_ptr<SliderAttachment> trebleMicGainAttachment;
    std::unique_ptr<SliderAttachment> splitCenterAttachment;
    std::unique_ptr<SliderAttachment> splitWidthAttachment;
    std::unique_ptr<SliderAttachment> masterGainAttachment;

    // Layer 3 Bottom: 97-Key Sub-Bass Keyboard (C0 to C8)
    ImperialKeyboardComponent keyboardComponent;
    float smoothedMeterL { 0.0f };
    float smoothedMeterR { 0.0f };
    uint32_t lastSeenHitCounter { 0 };
    int lastScopeReadIndex { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BosendorferAudioProcessorEditor)
};
