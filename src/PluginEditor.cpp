#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "AssetShield.h"
#include "BinaryData.h"

#ifndef ASSETS_DIR_PATH
#define ASSETS_DIR_PATH "/mnt/Storage/Projects/Bosendoter_mperial_Grand/assets"
#endif

//==============================================================================
// Layer 2: MicOverlayLayer Implementation (Split Left / Right Floating Mics)
//==============================================================================
MicOverlayLayer::MicOverlayLayer()
{
    setInterceptsMouseClicks(false, true);

    auto loadShieldedPng = [](const void* binData, int binSize) -> juce::Image
    {
        return AssetShield::loadImage(binData, static_cast<size_t>(binSize));
    };

    // 0: Neumann KM 184 (Left: Neumann_KM184_L.bin, Right: Neuman_KM184_R.bin)
    cachedLeftMicImages[0]  = loadShieldedPng(BinaryData::Neumann_KM184_L_bin, BinaryData::Neumann_KM184_L_binSize);
    cachedRightMicImages[0] = loadShieldedPng(BinaryData::Neuman_KM184_R_bin,  BinaryData::Neuman_KM184_R_binSize);

    // 1: Neuman TLM 103 (Left: Neuman_TLM_L.bin, Right: Neuman_TLM_R.bin)
    cachedLeftMicImages[1]  = loadShieldedPng(BinaryData::Neuman_TLM_L_bin,    BinaryData::Neuman_TLM_L_binSize);
    cachedRightMicImages[1] = loadShieldedPng(BinaryData::Neuman_TLM_R_bin,    BinaryData::Neuman_TLM_R_binSize);

    // 2: Neumann U87 (Left: Neuman_U87-L.bin, Right: Neuman_U87-R.bin)
    cachedLeftMicImages[2]  = loadShieldedPng(BinaryData::Neuman_U87L_bin,     BinaryData::Neuman_U87L_binSize);
    cachedRightMicImages[2] = loadShieldedPng(BinaryData::Neuman_U87R_bin,     BinaryData::Neuman_U87R_binSize);

    // 3: RCA 44BX NEVE 1073 (Left: RCA_Ribbon_L.bin, Right: RCA_Ribbon_R.bin)
    cachedLeftMicImages[3]  = loadShieldedPng(BinaryData::RCA_Ribbon_L_bin,    BinaryData::RCA_Ribbon_L_binSize);
    cachedRightMicImages[3] = loadShieldedPng(BinaryData::RCA_Ribbon_R_bin,    BinaryData::RCA_Ribbon_R_binSize);

    addAndMakeVisible(bassMicOverlay);
    addAndMakeVisible(trebleMicOverlay);

    bassMicOverlay.onPositionMoved = [this]() { repaint(); };
    trebleMicOverlay.onPositionMoved = [this]() { repaint(); };
}

juce::Image MicOverlayLayer::getLeftMicImage(int index) const
{
    return cachedLeftMicImages[static_cast<size_t>(juce::jlimit(0, 3, index))];
}

juce::Image MicOverlayLayer::getRightMicImage(int index) const
{
    return cachedRightMicImages[static_cast<size_t>(juce::jlimit(0, 3, index))];
}

juce::String MicOverlayLayer::getMicDisplayName(int index)
{
    return getMicModelAssets(index).displayName;
}

void MicOverlayLayer::setMicSelections(int bassMicIndex, int trebleMicIndex)
{
    if (currentBassMic != bassMicIndex)
    {
        currentBassMic = juce::jlimit(0, 3, bassMicIndex);
        bassMicOverlay.setMicImage(getLeftMicImage(currentBassMic), getMicDisplayName(currentBassMic));
        repaint(bannerBounds);
    }

    if (currentTrebleMic != trebleMicIndex)
    {
        currentTrebleMic = juce::jlimit(0, 3, trebleMicIndex);
        trebleMicOverlay.setMicImage(getRightMicImage(currentTrebleMic), getMicDisplayName(currentTrebleMic));
        repaint(bannerBounds);
    }
}

void MicOverlayLayer::setMicGainsAndBleed(float bassGainDb, float trebleGainDb, float crossfeedNorm)
{
    bassDb = bassGainDb;
    trebleDb = trebleGainDb;
    bassMicOverlay.setGainAndRole("LEFT (BASS <= A4)", bassDb, isBypassed);
    trebleMicOverlay.setGainAndRole("RIGHT (F3-F5+)", trebleDb, isBypassed);

    if (std::abs(crossfeed - crossfeedNorm) > 0.005f)
    {
        crossfeed = crossfeedNorm;
        repaint();
    }
}

void MicOverlayLayer::setSplitParameters(int splitCenterNote, float splitWidthSemitones, bool bypassed)
{
    if (splitCenter != splitCenterNote || std::abs(splitWidth - splitWidthSemitones) > 0.05f || isBypassed != bypassed)
    {
        splitCenter = splitCenterNote;
        splitWidth = splitWidthSemitones;
        isBypassed = bypassed;
        bassMicOverlay.setGainAndRole("LEFT (BASS <= A4)", bassDb, isBypassed);
        trebleMicOverlay.setGainAndRole("RIGHT (F3-F5+)", trebleDb, isBypassed);
        repaint();
    }
}

void MicOverlayLayer::resized()
{
    const float scaleX = static_cast<float>(getWidth()) / 654.0f;
    const float scaleY = static_cast<float>(getHeight()) / 383.0f;

    if (!initialPositionsSet && getWidth() > 0 && getHeight() > 0)
    {
        bassMicOverlay.setBounds(
            static_cast<int>(static_cast<float>(bassMicBounds.getX()) * scaleX),
            static_cast<int>(static_cast<float>(bassMicBounds.getY()) * scaleY),
            static_cast<int>(static_cast<float>(bassMicBounds.getWidth()) * scaleX),
            static_cast<int>(static_cast<float>(bassMicBounds.getHeight()) * scaleY));

        trebleMicOverlay.setBounds(
            static_cast<int>(static_cast<float>(trebleMicBounds.getX()) * scaleX),
            static_cast<int>(static_cast<float>(trebleMicBounds.getY()) * scaleY),
            static_cast<int>(static_cast<float>(trebleMicBounds.getWidth()) * scaleX),
            static_cast<int>(static_cast<float>(trebleMicBounds.getHeight()) * scaleY));

        initialPositionsSet = true;
    }
}

void MicOverlayLayer::paint(juce::Graphics& g)
{
    const float w = static_cast<float>(getWidth());
    const float h = static_cast<float>(getHeight());
    const float scaleX = w / 654.0f;
    const float scaleY = h / 383.0f;

    auto scaleRect = [scaleX, scaleY](juce::Rectangle<int> r) -> juce::Rectangle<float>
    {
        return {
            static_cast<float>(r.getX()) * scaleX,
            static_cast<float>(r.getY()) * scaleY,
            static_cast<float>(r.getWidth()) * scaleX,
            static_cast<float>(r.getHeight()) * scaleY
        };
    };

    const auto bassBox   = bassMicOverlay.getBounds().toFloat();
    const auto trebleBox = trebleMicOverlay.getBounds().toFloat();
    const auto infoBox   = scaleRect(bannerBounds);

    const float alphaMul = isBypassed ? 0.35f : 1.0f;

    // 1. Draw Acoustic Split Crossover Divider Line across the Soundboard / Harp
    const float splitNorm = juce::jlimit(0.15f, 0.85f, (static_cast<float>(splitCenter) - 24.0f) / 72.0f);
    const float splitX = w * splitNorm;
    const float zoneHalfPx = (splitWidth / 48.0f) * (w * 0.45f);

    if (!isBypassed && zoneHalfPx > 1.0f)
    {
        juce::ColourGradient splitGrad(
            juce::Colour(0xffd4af37).withAlpha(0.0f), splitX - zoneHalfPx, 0.0f,
            juce::Colour(0xff5ca8ff).withAlpha(0.0f), splitX + zoneHalfPx, 0.0f, false);
        splitGrad.addColour(0.5, juce::Colour(0xfff5d76e).withAlpha(0.20f));
        g.setGradientFill(splitGrad);
        g.fillRect(splitX - zoneHalfPx, 32.0f * scaleY, zoneHalfPx * 2.0f, h - 98.0f * scaleY);
    }

    // Dashed split center line between Left (Bass) and Right (Treble) halves
    g.setColour(juce::Colour(0xfff5d76e).withAlpha(0.55f * alphaMul));
    const float dashLengths[] = { 6.0f, 4.0f };
    g.drawDashedLine(juce::Line<float>(splitX, 34.0f * scaleY, splitX, h - 72.0f * scaleY),
                     dashLengths, 2, 1.5f);

    // 2. Draw Stereo Off-Axis Crossfeed / Bleed Arc between Left and Right Microphones
    if (!isBypassed && crossfeed > 0.005f)
    {
        juce::Path bleedArc;
        bleedArc.startNewSubPath(bassBox.getCentreX() + 36.0f * scaleX, bassBox.getCentreY());
        bleedArc.quadraticTo(splitX, 44.0f * scaleY,
                             trebleBox.getCentreX() - 36.0f * scaleX, trebleBox.getCentreY());

        g.setColour(juce::Colour(0xffd4af37).withAlpha(juce::jlimit(0.18f, 0.85f, crossfeed * 1.6f)));
        g.strokePath(bleedArc, juce::PathStrokeType(2.2f));
    }

    // 3. Draw Split & Convolution Status Banner at bottom of harp viewport
    g.setColour(juce::Colour(0xff12100e).withAlpha(0.86f));
    g.fillRoundedRectangle(infoBox, 6.0f);
    g.setColour(isBypassed ? juce::Colour(0xffb84a39) : juce::Colour(0xff8c6f2d));
    g.drawRoundedRectangle(infoBox, 6.0f, 1.2f);

    if (isBypassed)
    {
        g.setColour(juce::Colour(0xffff8e7a));
        g.setFont(juce::FontOptions(11.5f * scaleY, juce::Font::bold));
        g.drawText("MICROPHONE FIR CONVOLUTION BYPASSED - DIRECT SOUNDFONT OUTPUT",
                   infoBox, juce::Justification::centred);
    }
    else
    {
        const int lowNote  = juce::jlimit(12, 108, static_cast<int>(std::round(static_cast<float>(splitCenter) - splitWidth * 0.5f)));
        const int highNote = juce::jlimit(12, 108, static_cast<int>(std::round(static_cast<float>(splitCenter) + splitWidth * 0.5f)));
        const juce::String lowName  = juce::MidiMessage::getMidiNoteName(lowNote, true, true, 4);
        const juce::String highName = juce::MidiMessage::getMidiNoteName(highNote, true, true, 4);

        const auto& leftAssets  = getMicModelAssets(juce::jlimit(0, 3, currentBassMic));
        const auto& rightAssets = getMicModelAssets(juce::jlimit(0, 3, currentTrebleMic));

        g.setColour(juce::Colour(0xfff5d76e));
        g.setFont(juce::FontOptions(10.5f * scaleY, juce::Font::bold));
        g.drawText("BASS (LEFT <= " + highName + ") + TREBLE (RIGHT >= " + lowName + " to F5+)   |   CROSSOVER: "
                   + lowName + "-" + highName + " (" + juce::String(splitWidth, 1) + " ST)   |   BLEED: "
                   + juce::String(static_cast<int>(crossfeed * 100.0f)) + "%",
                   infoBox.withHeight(infoBox.getHeight() * 0.54f), juce::Justification::centred);

        g.setColour(juce::Colour(0xffcbb98b));
        g.setFont(juce::FontOptions(9.5f * scaleY));
        g.drawText(juce::String("Left IR: ") + leftAssets.wavFileName + " (" + leftAssets.leftPngFileName + ")   |   Right IR: "
                   + rightAssets.wavFileName + " (" + rightAssets.rightPngFileName + ")",
                   infoBox.withTrimmedTop(infoBox.getHeight() * 0.50f), juce::Justification::centred);
    }
}

//==============================================================================
// Main Editor Implementation (Matching Prototype_look_and_feel.png)
//==============================================================================
BosendorferAudioProcessorEditor::BosendorferAudioProcessorEditor(BosendorferAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      monitorView(p.apvts, IDs::velCurve.getParamID()),
      keyboardComponent(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&lookAndFeel);

    // --- Layer 1: Descrambled Background Plates from Shielded BinaryData ---
    stageGrandBg = AssetShield::loadImage(BinaryData::stage_grand_bin, static_cast<size_t>(BinaryData::stage_grand_binSize));
    pianoHarpBg  = AssetShield::loadImage(BinaryData::piano_harp_bin, static_cast<size_t>(BinaryData::piano_harp_binSize));

    // --- Layer 2: Add Split Left/Right Floating Microphone Overlay Layer ---
    addChildComponent(micOverlayLayer);

    // --- Layer 3: Header Bar Controls ---
    addAndMakeVisible(licenseButton);
    licenseButton.onClick = [this]()
    {
        licenseOverlay.refreshFromManager();
        licenseOverlay.setVisible(!licenseOverlay.isVisible());
        licenseOverlay.toFront(true);
    };

    addAndMakeVisible(presetSelector);
    presetSelector.onChange = [this]()
    {
        const int selectedId = presetSelector.getSelectedId();
        if (selectedId > 0)
            audioProcessor.selectPreset(selectedId - 1);
    };

    addAndMakeVisible(stdSf2Button);
    stdSf2Button.onClick = [this]()
    {
        if (audioProcessor.loadEmbeddedSoundFont(BosendorferAudioProcessor::EmbeddedBank::Standard))
        {
            refreshPresetComboBox();
            updateBankButtonStates();
            repaint();
        }
    };

    addAndMakeVisible(brightSf2Button);
    brightSf2Button.onClick = [this]()
    {
        if (audioProcessor.loadEmbeddedSoundFont(BosendorferAudioProcessor::EmbeddedBank::Bright))
        {
            refreshPresetComboBox();
            updateBankButtonStates();
            repaint();
        }
    };

    // View Switch Button: [FX / Mics] vs [Stage View]
    viewToggleBtn.setClickingTogglesState(true);
    addAndMakeVisible(viewToggleBtn);
    viewToggleBtn.onClick = [this]()
    {
        isMicSetupView = viewToggleBtn.getToggleState();
        viewToggleBtn.setButtonText(isMicSetupView ? "Stage View" : "FX / Mics");
        monitorView.setMicViewMode(isMicSetupView);
        updateLayerState();
        repaint();
    };

    // Pedals
    unaCordaButton.setClickingTogglesState(true);
    addAndMakeVisible(unaCordaButton);
    unaCordaButton.onClick = [this]()
    {
        audioProcessor.setSoftPedalFromUI(unaCordaButton.getToggleState());
    };

    sustainButton.setClickingTogglesState(true);
    addAndMakeVisible(sustainButton);
    sustainButton.onClick = [this]()
    {
        audioProcessor.setSustainPedalFromUI(sustainButton.getToggleState());
    };

    // --- Left Panel ("Sound Control"): Vertical Faders + 4 Circular Cutout Knobs + Velocity Curve Box ---
    setupVerticalFader(reverbFader, reverbFaderTopLabel, reverbFaderBotLabel, "Reverb");
    setupVerticalFader(lidFader,    lidFaderTopLabel,    lidFaderBotLabel,    "Lid Open");

    reverbFaderAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, IDs::roomMix.getParamID(), reverbFader);
    lidFaderAttachment    = std::make_unique<SliderAttachment>(audioProcessor.apvts, IDs::lidPosition.getParamID(), lidFader);

    // 4 Rotary Knobs aligned to the 4 black circles on stage_grand.png ("Reverb", "Lid Open", "Key Noise", "Pedal Noise")
    setupCircleKnob(roomSlider,        "Reverb Mix");
    setupCircleKnob(lidPositionSlider, "Lid Open Position");
    setupCircleKnob(keyNoiseSlider,    "Keybed Action Noise");
    setupCircleKnob(pedalNoiseSlider,  "Damper Pedal Noise (Lift Whoosh & Fast-Release Mechanical Thump)");

    roomAttachment        = std::make_unique<SliderAttachment>(audioProcessor.apvts, IDs::roomMix.getParamID(), roomSlider);
    lidPositionAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, IDs::lidPosition.getParamID(), lidPositionSlider);
    keyNoiseAttachment    = std::make_unique<SliderAttachment>(audioProcessor.apvts, IDs::keyNoise.getParamID(), keyNoiseSlider);
    pedalNoiseAttachment  = std::make_unique<SliderAttachment>(audioProcessor.apvts, IDs::pedalNoise.getParamID(), pedalNoiseSlider);

    // Dual-mode display window (Touch Response Curve in Stage View, Stereo Mic Vector Scope in Mic View)
    addAndMakeVisible(monitorView);

    // Interactive drag slider placed over the Velocity Curve display box (X=32..151, Y=374..418)
    velCurveBoxSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    velCurveBoxSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    velCurveBoxSlider.setAlpha(0.0f);
    velCurveBoxSlider.setTooltip("Drag up/down to bend the Velocity Curve");
    addAndMakeVisible(velCurveBoxSlider);
    velCurveBoxAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, IDs::velCurve.getParamID(), velCurveBoxSlider);

    // --- Center Viewport Overlay Selectors (4 Selectable Mics for Left & Right + Room IR) ---
    roomIrSelector.addItemList({ "Dry", "Vienna Konzerthaus", "Maple Stage 40ft", "Naumburg Band Shell", "Scoring Studio Hall", "Art Gallery" }, 1);
    addAndMakeVisible(roomIrSelector);
    roomIrAttachment = std::make_unique<ComboBoxAttachment>(
        audioProcessor.apvts, IDs::roomIrType.getParamID(), roomIrSelector);

    const juce::StringArray micModels {
        "1. Neumann KM 184",
        "2. Neuman TLM 103",
        "3. Neumann U87",
        "4. RCA 44BX NEVE 1073"
    };

    bassMicSelector.addItemList(micModels, 1);
    bassMicSelector.setTooltip("Select Left (Bass) Microphone Model & IR");
    addAndMakeVisible(bassMicSelector);
    bassMicAttachment = std::make_unique<ComboBoxAttachment>(
        audioProcessor.apvts, IDs::bassMicType.getParamID(), bassMicSelector);
    bassMicSelector.onChange = [this]()
    {
        if (!isMicSetupView)
        {
            isMicSetupView = true;
            viewToggleBtn.setToggleState(true, juce::dontSendNotification);
            viewToggleBtn.setButtonText("Stage View");
        }
        updateLayerState();
    };

    trebleMicSelector.addItemList(micModels, 1);
    trebleMicSelector.setTooltip("Select Right (Treble) Microphone Model & IR");
    addAndMakeVisible(trebleMicSelector);
    trebleMicAttachment = std::make_unique<ComboBoxAttachment>(
        audioProcessor.apvts, IDs::trebleMicType.getParamID(), trebleMicSelector);
    trebleMicSelector.onChange = [this]()
    {
        if (!isMicSetupView)
        {
            isMicSetupView = true;
            viewToggleBtn.setToggleState(true, juce::dontSendNotification);
            viewToggleBtn.setButtonText("Stage View");
        }
        updateLayerState();
    };

    micBypassButton.setClickingTogglesState(true);
    micBypassButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff8b261d));
    addAndMakeVisible(micBypassButton);
    micBypassAttachment = std::make_unique<ButtonAttachment>(
        audioProcessor.apvts, IDs::micSectionBypassed.getParamID(), micBypassButton);
    micBypassButton.onClick = [this]() { updateLayerState(); };

    // --- Right Panel ("Mic & Effects"): 3 Vertical Faders + 3 Circular Cutout Knobs ---
    setupVerticalFader(bassMicGainSlider,  bassFaderTopLabel,   bassFaderBotLabel,   "Left");
    setupVerticalFader(micCrossfeedSlider, bleedFaderTopLabel,  bleedFaderBotLabel,  "Bleed");
    setupVerticalFader(trebleMicGainSlider, trebleFaderTopLabel, trebleFaderBotLabel, "Right");

    bassMicGainAttachment   = std::make_unique<SliderAttachment>(audioProcessor.apvts, IDs::bassMicGain.getParamID(), bassMicGainSlider);
    micCrossfeedAttachment  = std::make_unique<SliderAttachment>(audioProcessor.apvts, IDs::micCrossfeed.getParamID(), micCrossfeedSlider);
    trebleMicGainAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, IDs::trebleMicGain.getParamID(), trebleMicGainSlider);

    // 3 Rotary Knobs aligned to the 3 black circles on stage_grand.png ("EQ", "Dynamic Range", "Saturation")
    setupCircleKnob(splitCenterSlider, "Crossover Center Note (F3-A4)");
    setupCircleKnob(splitWidthSlider,  "Crossover Width (Semitones)");
    setupCircleKnob(masterGainSlider,  "Master Output Gain (dB)");

    splitCenterAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, IDs::splitCenter.getParamID(), splitCenterSlider);
    splitWidthAttachment  = std::make_unique<SliderAttachment>(audioProcessor.apvts, IDs::splitWidth.getParamID(), splitWidthSlider);
    masterGainAttachment  = std::make_unique<SliderAttachment>(audioProcessor.apvts, IDs::masterGain.getParamID(), masterGainSlider);

    // --- Layer 3 Bottom: 97-Key Bösendorfer Imperial Keyboard ---
    keyboardComponent.setScrollButtonsVisible(false);
    addAndMakeVisible(keyboardComponent);

    addChildComponent(licenseOverlay);
    licenseOverlay.onLicenseChanged = [this]()
    {
        updateBankButtonStates();
        repaint();
    };
    if (!LicenseManager::getInstance().isLicensed())
    {
        licenseOverlay.refreshFromManager();
        licenseOverlay.setVisible(true);
    }

    refreshPresetComboBox();
    updateBankButtonStates();
    updateLayerState();

    setResizable(true, true);
    setResizeLimits(1000, 580, 1600, 928);
    setSize(1000, 580);

    startTimerHz(30);
}

BosendorferAudioProcessorEditor::~BosendorferAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void BosendorferAudioProcessorEditor::setupCircleKnob(juce::Slider& slider, const juce::String& tooltipText)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled(true, true, this);
    slider.setTooltip(tooltipText);
    addAndMakeVisible(slider);
}

void BosendorferAudioProcessorEditor::setupVerticalFader(juce::Slider& slider, juce::Label& topLbl, juce::Label& botLbl, const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled(true, true, this);
    addAndMakeVisible(slider);

    topLbl.setText(text, juce::dontSendNotification);
    topLbl.setJustificationType(juce::Justification::centred);
    topLbl.setFont(juce::FontOptions(9.5f));
    topLbl.setColour(juce::Label::textColourId, juce::Colour(0xffdedad2));
    addAndMakeVisible(topLbl);

    botLbl.setText(text, juce::dontSendNotification);
    botLbl.setJustificationType(juce::Justification::centred);
    botLbl.setFont(juce::FontOptions(9.5f));
    botLbl.setColour(juce::Label::textColourId, juce::Colour(0xffdedad2));
    addAndMakeVisible(botLbl);
}

void BosendorferAudioProcessorEditor::updateLayerState()
{
    const bool bypassed = micBypassButton.getToggleState();
    micBypassButton.setButtonText(bypassed ? "Mics Bypassed" : "Mics Active");

    const float activeAlpha = bypassed ? 0.45f : 1.0f;
    bassMicSelector.setAlpha(activeAlpha);
    trebleMicSelector.setAlpha(activeAlpha);
    bassMicGainSlider.setAlpha(activeAlpha);
    trebleMicGainSlider.setAlpha(activeAlpha);
    micCrossfeedSlider.setAlpha(activeAlpha);
    splitCenterSlider.setAlpha(activeAlpha);
    splitWidthSlider.setAlpha(activeAlpha);

    micOverlayLayer.setVisible(isMicSetupView);
    roomIrSelector.setVisible(!isMicSetupView);
    monitorView.setMicViewMode(isMicSetupView);
    velCurveBoxSlider.setInterceptsMouseClicks(!isMicSetupView, false);

    const int bassIdx   = juce::jlimit(0, 3, bassMicSelector.getSelectedId() - 1);
    const int trebleIdx = juce::jlimit(0, 3, trebleMicSelector.getSelectedId() - 1);
    const int splitNote = static_cast<int>(std::round(splitCenterSlider.getValue()));
    const float splitW  = static_cast<float>(splitWidthSlider.getValue());

    micOverlayLayer.setMicSelections(bassIdx, trebleIdx);
    micOverlayLayer.setMicGainsAndBleed(
        static_cast<float>(bassMicGainSlider.getValue()),
        static_cast<float>(trebleMicGainSlider.getValue()),
        static_cast<float>(micCrossfeedSlider.getValue()));
    micOverlayLayer.setSplitParameters(splitNote, splitW, bypassed);

    keyboardComponent.setSplitZoneOverlay(splitNote, splitW, isMicSetupView && !bypassed);
}

void BosendorferAudioProcessorEditor::refreshPresetComboBox()
{
    presetSelector.clear(juce::dontSendNotification);
    const auto presets = audioProcessor.getAvailablePresets();
    for (int i = 0; i < presets.size(); ++i)
        presetSelector.addItem(juce::String(i + 1) + ". " + presets[i], i + 1);

    const int current = audioProcessor.getSelectedPreset();
    if (presets.size() > 0)
        presetSelector.setSelectedId(juce::jlimit(1, presets.size(), current + 1), juce::dontSendNotification);
}

void BosendorferAudioProcessorEditor::updateBankButtonStates()
{
    const auto bank = audioProcessor.getCurrentEmbeddedBank();
    stdSf2Button.setToggleState(bank == BosendorferAudioProcessor::EmbeddedBank::Standard,
                                juce::dontSendNotification);
    brightSf2Button.setToggleState(bank == BosendorferAudioProcessor::EmbeddedBank::Bright,
                                   juce::dontSendNotification);

    const bool licensed = LicenseManager::getInstance().isLicensed();
    licenseButton.setButtonText(licensed ? "Licensed" : "Activate");
    licenseButton.setToggleState(licensed, juce::dontSendNotification);
}

void BosendorferAudioProcessorEditor::timerCallback()
{
    const bool susDown  = audioProcessor.isSustainPedalDown();
    const bool softDown = audioProcessor.isSoftPedalDown();

    if (sustainButton.getToggleState() != susDown)
        sustainButton.setToggleState(susDown, juce::dontSendNotification);

    if (unaCordaButton.getToggleState() != softDown)
        unaCordaButton.setToggleState(softDown, juce::dontSendNotification);

    const int currentPreset = audioProcessor.getSelectedPreset();
    if (presetSelector.getSelectedId() != currentPreset + 1 && presetSelector.getNumItems() > currentPreset)
        presetSelector.setSelectedId(currentPreset + 1, juce::dontSendNotification);

    const uint32_t currentHits = audioProcessor.getNoteHitCounter();
    if (currentHits != lastSeenHitCounter)
    {
        lastSeenHitCounter = currentHits;
        monitorView.setLastHitVelocity(audioProcessor.getLastNoteVelocity());
    }

    const int currentScopeWrite = audioProcessor.getScopeWriteIndex();
    if (currentScopeWrite != lastScopeReadIndex)
    {
        const int available = juce::jmin(BosendorferAudioProcessor::kScopeBufferSize,
                                         currentScopeWrite - lastScopeReadIndex);
        const int startIdx = currentScopeWrite - available;
        for (int i = 0; i < available; ++i)
        {
            const auto [l, r] = audioProcessor.getScopeSample(startIdx + i);
            monitorView.pushStereoSample(l, r);
        }
        lastScopeReadIndex = currentScopeWrite;
    }

    updateLayerState();

    const float scaleX = static_cast<float>(getWidth()) / 1000.0f;
    const float scaleY = static_cast<float>(getHeight()) / 580.0f;

    // Repaint top header status & Left Panel Stereo Meter
    repaint(static_cast<int>(700.0f * scaleX), static_cast<int>(38.0f * scaleY),
            static_cast<int>(290.0f * scaleX), static_cast<int>(24.0f * scaleY));
    repaint(static_cast<int>(108.0f * scaleX), static_cast<int>(112.0f * scaleY),
            static_cast<int>(48.0f * scaleX),  static_cast<int>(128.0f * scaleY));
}

void BosendorferAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float scaleX = bounds.getWidth() / 1000.0f;
    const float scaleY = bounds.getHeight() / 580.0f;

    //==========================================================================
    // LAYER 1: Base Background Plate (stage_grand.png vs piano_harp.png)
    //==========================================================================
    const juce::Image& activePlate = isMicSetupView ? pianoHarpBg : stageGrandBg;
    if (activePlate.isValid())
    {
        g.drawImage(activePlate, bounds, juce::RectanglePlacement::stretchToFit);
    }
    else
    {
        g.fillAll(juce::Colour(0xff12100e));
    }

    //==========================================================================
    // 1. Fallboard / Top Header Title ("CLASSIC VIENNA 290")
    //==========================================================================
    g.setFont(juce::FontOptions("Didot", 24.0f * scaleY, juce::Font::bold));
    g.setColour(juce::Colour(0xFFE5C158)); // Polished brass lettering
    g.drawText("CLASSIC VIENNA 290",
               static_cast<int>(18.0f * scaleX), static_cast<int>(6.0f * scaleY),
               static_cast<int>(420.0f * scaleX), static_cast<int>(26.0f * scaleY),
               juce::Justification::centredLeft, true);

    // 2. Maker's Badge & Mason's Mark ("SIGNAL FORGE AUDIO LABS")
    {
        const float mx = 20.0f * scaleX;
        const float my = 35.5f * scaleY;
        const float ms = 13.0f * scaleY;
        const float cx = mx + ms * 0.5f;
        const float cy = my + ms * 0.5f;

        juce::Path masonsMark;
        // Outer diamond / square-and-compass frame
        masonsMark.startNewSubPath(cx, my);
        masonsMark.lineTo(mx + ms, cy);
        masonsMark.lineTo(cx, my + ms);
        masonsMark.lineTo(mx, cy);
        masonsMark.closeSubPath();

        // Inner mason chisel vertical axis & Signal Forge chevron crossbar
        masonsMark.startNewSubPath(cx, my + ms * 0.18f);
        masonsMark.lineTo(cx, my + ms * 0.82f);
        masonsMark.startNewSubPath(mx + ms * 0.24f, cy - ms * 0.14f);
        masonsMark.lineTo(cx, cy + ms * 0.16f);
        masonsMark.lineTo(mx + ms * 0.76f, cy - ms * 0.14f);

        g.setColour(juce::Colour(0xFFE5C158));
        g.strokePath(masonsMark, juce::PathStrokeType(1.35f * scaleY));
    }

    g.setFont(juce::FontOptions("Helvetica Neue", 10.5f * scaleY, juce::Font::bold));
    g.setColour(juce::Colour(0xFFC0A060)); // Soft gold silkscreen
    g.drawText("SIGNAL FORGE AUDIO LABS",
               static_cast<int>(38.0f * scaleX), static_cast<int>(34.0f * scaleY),
               static_cast<int>(175.0f * scaleX), static_cast<int>(16.0f * scaleY),
               juce::Justification::centredLeft, true);

    g.setColour(juce::Colour(0xff9e9a92));
    g.setFont(juce::FontOptions(10.0f * scaleY, juce::Font::bold));
    g.drawText("|   97-KEY CONCERT GRAND  -  " + audioProcessor.getCurrentSoundFontName().toUpperCase(),
               static_cast<int>(214.0f * scaleX), static_cast<int>(34.0f * scaleY),
               static_cast<int>(420.0f * scaleX), static_cast<int>(16.0f * scaleY),
               juce::Justification::centredLeft);

    // Top-Right MIDI & Voices LED Indicator (Matching "MIDI [green]" in Prototype_look_and_feel.png)
    const int voices = audioProcessor.getActiveVoiceCount();
    g.setColour(juce::Colour(0xffb5b0a8));
    g.setFont(juce::FontOptions(10.0f * scaleY, juce::Font::bold));
    g.drawText("MIDI",
               static_cast<int>(824.0f * scaleX), static_cast<int>(42.0f * scaleY),
               static_cast<int>(34.0f * scaleX), static_cast<int>(14.0f * scaleY),
               juce::Justification::centredRight);

    juce::Rectangle<float> midiLed(862.0f * scaleX, 45.0f * scaleY, 18.0f * scaleX, 7.0f * scaleY);
    g.setColour(voices > 0 ? juce::Colour(0xff5ce63c) : juce::Colour(0xff2b4a24));
    g.fillRoundedRectangle(midiLed, 3.5f * scaleY);

    g.setColour(juce::Colour(0xffb5b0a8));
    g.drawText("VOICES: " + juce::String(voices),
               static_cast<int>(890.0f * scaleX), static_cast<int>(42.0f * scaleY),
               static_cast<int>(90.0f * scaleX), static_cast<int>(14.0f * scaleY),
               juce::Justification::centredRight);

    //==========================================================================
    // Left Panel Upper Box: Stereo Segmented LED Meter (Matching Prototype_look_and_feel.png)
    //==========================================================================
    // Cover the solid black placeholder box in stage_grand.png with the brushed panel gradient
    juce::Rectangle<float> leftUpperBox(24.0f * scaleX, 110.0f * scaleY, 132.0f * scaleX, 130.0f * scaleY);
    juce::ColourGradient leftPanelGrad(juce::Colour(0xff4c4a47), leftUpperBox.getX(), leftUpperBox.getY(),
                                       juce::Colour(0xff2c2a28), leftUpperBox.getRight(), leftUpperBox.getBottom(), false);
    g.setGradientFill(leftPanelGrad);
    g.fillRoundedRectangle(leftUpperBox, 3.0f);

    juce::Rectangle<float> rightUpperBox(844.0f * scaleX, 112.0f * scaleY, 130.0f * scaleX, 130.0f * scaleY);
    juce::ColourGradient rightPanelGrad(juce::Colour(0xff4a4540), rightUpperBox.getX(), rightUpperBox.getY(),
                                        juce::Colour(0xff2b2724), rightUpperBox.getRight(), rightUpperBox.getBottom(), false);
    g.setGradientFill(rightPanelGrad);
    g.fillRoundedRectangle(rightUpperBox, 3.0f);

    // Draw Tri-Color Segmented Vertical Stereo Meter at X = 118..142, Y = 122..218
    const float targetL = std::pow(juce::jlimit(0.0f, 1.0f, audioProcessor.getOutputRmsLeft() * 3.6f), 0.62f);
    const float targetR = std::pow(juce::jlimit(0.0f, 1.0f, audioProcessor.getOutputRmsRight() * 3.6f), 0.62f);
    smoothedMeterL = (targetL > smoothedMeterL) ? (smoothedMeterL * 0.55f + targetL * 0.45f)
                                                : (smoothedMeterL * 0.88f + targetL * 0.12f);
    smoothedMeterR = (targetR > smoothedMeterR) ? (smoothedMeterR * 0.55f + targetR * 0.45f)
                                                : (smoothedMeterR * 0.88f + targetR * 0.12f);

    const int numSegments = 24;
    const float meterTop = 122.0f * scaleY;
    const float meterH   = 96.0f * scaleY;
    const float segStep  = meterH / static_cast<float>(numSegments);
    const float barW     = 7.0f * scaleX;
    const float leftBarX = 120.0f * scaleX;
    const float rightBarX = 131.0f * scaleX;

    for (int s = 0; s < numSegments; ++s)
    {
        const float segNorm = 1.0f - (static_cast<float>(s) / static_cast<float>(numSegments));
        const float sy = meterTop + static_cast<float>(s) * segStep;

        const bool litL = (smoothedMeterL >= segNorm);
        const bool litR = (smoothedMeterR >= segNorm);

        juce::Colour onCol;
        juce::Colour offCol;
        if (s < 4) // Top 4 segments: Red Peak Zone
        {
            onCol  = juce::Colour(0xffff3b30);
            offCol = juce::Colour(0xff2e1716);
        }
        else if (s < 10) // Next 6 segments: Amber / Yellow Zone
        {
            onCol  = juce::Colour(0xffffcc00);
            offCol = juce::Colour(0xff2d2815);
        }
        else // Lower 14 segments: Emerald Green Zone
        {
            onCol  = juce::Colour(0xff38e54d);
            offCol = juce::Colour(0xff17291a);
        }

        g.setColour(litL ? onCol : offCol);
        g.fillRect(leftBarX, sy, barW, segStep * 0.68f);

        g.setColour(litR ? onCol : offCol);
        g.fillRect(rightBarX, sy, barW, segStep * 0.68f);
    }

    g.setColour(juce::Colour(0xffdedad2));
    g.setFont(juce::FontOptions(9.5f * scaleY));
    g.drawText("Meter",
               static_cast<int>(112.0f * scaleX), static_cast<int>(222.0f * scaleY),
               static_cast<int>(36.0f * scaleX), static_cast<int>(14.0f * scaleY),
               juce::Justification::centred);
}

void BosendorferAudioProcessorEditor::resized()
{
    const float scaleX = static_cast<float>(getWidth()) / 1000.0f;
    const float scaleY = static_cast<float>(getHeight()) / 580.0f;

    auto scaledBounds = [scaleX, scaleY](int x, int y, int w, int h) -> juce::Rectangle<int>
    {
        return {
            static_cast<int>(std::round(static_cast<float>(x) * scaleX)),
            static_cast<int>(std::round(static_cast<float>(y) * scaleY)),
            static_cast<int>(std::round(static_cast<float>(w) * scaleX)),
            static_cast<int>(std::round(static_cast<float>(h) * scaleY))
        };
    };

    //==========================================================================
    // Layer 2: Split Left/Right Floating Microphone Overlay Layer over Harp
    //==========================================================================
    micOverlayLayer.setBounds(scaledBounds(177, 70, 654, 383));

    //==========================================================================
    // Layer 3 Top Header Bar Controls (Matching Prototype_look_and_feel.png)
    //==========================================================================
    licenseButton.setBounds(scaledBounds(446, 14, 76, 24));
    presetSelector.setBounds(scaledBounds(530, 14, 180, 24));
    stdSf2Button.setBounds(scaledBounds(718, 14, 78, 24));
    brightSf2Button.setBounds(scaledBounds(802, 14, 74, 24));
    viewToggleBtn.setBounds(scaledBounds(884, 14, 96, 24));
    licenseOverlay.setBounds(scaledBounds(177, 70, 654, 383));

    //==========================================================================
    // Center Viewport Top Overlay Bar (Left Mic, Bypass, Right Mic, Room Space)
    //==========================================================================
    bassMicSelector.setBounds(scaledBounds(184, 74, 196, 24));
    micBypassButton.setBounds(scaledBounds(444, 74, 120, 24));
    trebleMicSelector.setBounds(scaledBounds(628, 74, 196, 24));
    roomIrSelector.setBounds(scaledBounds(184, 398, 196, 24));

    //==========================================================================
    // Left Panel ("Sound Control"):
    // 1. Upper Box Vertical Faders (Reverb, Lid Open) + Meter
    //==========================================================================
    reverbFaderTopLabel.setVisible(false);
    reverbFader.setBounds(scaledBounds(30, 118, 32, 102));
    reverbFaderBotLabel.setBounds(scaledBounds(22, 222, 48, 14));

    lidFaderTopLabel.setVisible(false);
    lidFader.setBounds(scaledBounds(70, 118, 32, 102));
    lidFaderBotLabel.setBounds(scaledBounds(62, 222, 50, 14));

    // 2. 4 Circular Cutout Rotary Knobs on Left Panel
    roomSlider.setBounds(scaledBounds(39, 254, 36, 36));
    lidPositionSlider.setBounds(scaledBounds(110, 254, 36, 36));
    keyNoiseSlider.setBounds(scaledBounds(39, 314, 36, 36));
    pedalNoiseSlider.setBounds(scaledBounds(110, 314, 36, 36));

    // 3. Bottom Dual-Mode Monitor View & Interactive Velocity Drag Overlay (full left-sidebar width)
    monitorView.setBounds(scaledBounds(18, 370, 144, 52));
    velCurveBoxSlider.setBounds(scaledBounds(18, 370, 144, 52));

    //==========================================================================
    // Right Panel ("Mic & Effects"):
    // 1. Upper Box 3 Vertical Faders (Left/Close, Mid/Bleed, Right/Far)
    //==========================================================================
    bassFaderTopLabel.setBounds(scaledBounds(846, 114, 40, 13));
    bassMicGainSlider.setBounds(scaledBounds(850, 128, 32, 92));
    bassFaderBotLabel.setBounds(scaledBounds(846, 222, 40, 13));

    bleedFaderTopLabel.setBounds(scaledBounds(888, 114, 42, 13));
    micCrossfeedSlider.setBounds(scaledBounds(893, 128, 32, 92));
    bleedFaderBotLabel.setBounds(scaledBounds(888, 222, 42, 13));

    trebleFaderTopLabel.setBounds(scaledBounds(930, 114, 42, 13));
    trebleMicGainSlider.setBounds(scaledBounds(935, 128, 32, 92));
    trebleFaderBotLabel.setBounds(scaledBounds(930, 222, 42, 13));

    // 2. 3 Circular Cutout Rotary Knobs on Right Panel
    splitCenterSlider.setBounds(scaledBounds(889, 256, 36, 36));
    splitWidthSlider.setBounds(scaledBounds(889, 318, 36, 36));
    masterGainSlider.setBounds(scaledBounds(889, 378, 36, 36));

    //==========================================================================
    // Pedals Bar at bottom edge of center viewport
    //==========================================================================
    unaCordaButton.setBounds(scaledBounds(388, 430, 122, 18));
    sustainButton.setBounds(scaledBounds(518, 430, 122, 18));

    //==========================================================================
    // Layer 3 Bottom: 97-Key Sub-Contra Keyboard (Exact Slot: X=16..981, Y=467..579)
    //==========================================================================
    const auto kbRect = scaledBounds(16, 467, 966, 112);
    keyboardComponent.setBounds(kbRect);
    keyboardComponent.setKeyWidth(static_cast<float>(kbRect.getWidth()) / 57.0f);
}
