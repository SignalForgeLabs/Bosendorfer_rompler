#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "MicAssetLoader.h"
#include "BsendorferMicDSP.h"
#include "Parameters.h"

// ============================================================================
// Layer 2: Draggable Left / Right Floating Microphone Overlay Component
// ============================================================================
class FloatingMicOverlay : public juce::Component
{
public:
    FloatingMicOverlay(const juce::String& initialName, juce::Colour glowColour, bool isRightSide = false)
        : micName(initialName), accentColour(glowColour), rightAligned(isRightSide)
    {
        setSize(310, 270);
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }

    void setMicImage(const juce::Image& newImage, const juce::String& newName)
    {
        micImage = newImage;
        micName = newName;
        repaint();
    }

    void setGainAndRole(const juce::String& roleLabel, float gainDbValue, bool bypassed)
    {
        if (roleText != roleLabel || std::abs(gainDb - gainDbValue) > 0.05f || isBypassed != bypassed)
        {
            roleText = roleLabel;
            gainDb = gainDbValue;
            isBypassed = bypassed;
            repaint();
        }
    }

    std::function<void()> onPositionMoved;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const float alphaMul = isBypassed ? 0.35f : 1.0f;

        auto imgArea = bounds.withTrimmedBottom(26.0f);

        if (micImage.isValid())
        {
            g.setOpacity(alphaMul);
            const auto placement = (rightAligned ? juce::RectanglePlacement::xRight : juce::RectanglePlacement::xLeft)
                                 | juce::RectanglePlacement::yMid
                                 | juce::RectanglePlacement::onlyReduceInSize;
            g.drawImage(micImage, imgArea, placement);
            g.setOpacity(1.0f);
        }
        else
        {
            g.setColour(juce::Colours::darkgrey.withAlpha(alphaMul));
            g.fillRoundedRectangle(imgArea.reduced(16.0f), 8.0f);
            g.setColour(juce::Colours::white.withAlpha(alphaMul));
            g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
            g.drawFittedText(micName, imgArea.toNearestInt(), juce::Justification::centred, 2);
        }

        // Compact status badge at bottom of the Left / Right microphone half
        auto pill = bounds.withTrimmedTop(bounds.getHeight() - 22.0f).reduced(18.0f, 1.0f);
        g.setColour(juce::Colour(0xff100e0c).withAlpha(0.84f * alphaMul));
        g.fillRoundedRectangle(pill, 4.0f);
        g.setColour(accentColour.withAlpha(0.80f * alphaMul));
        g.drawRoundedRectangle(pill, 4.0f, 1.2f);

        g.setColour(juce::Colour(0xfff5d76e).withAlpha(alphaMul));
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        const juce::String dbStr = (gainDb >= 0.0f ? "+" : "") + juce::String(gainDb, 1) + " dB";
        const juce::String title = (roleText.isNotEmpty() ? (roleText + ": ") : "") + micName + " (" + dbStr + ")";
        g.drawText(title, pill.reduced(6.0f, 0.0f), juce::Justification::centred, false);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        constrainer.setMinimumOnscreenAmounts(getHeight() / 2, getWidth() / 2, getHeight() / 2, getWidth() / 2);
        dragger.startDraggingComponent(this, e);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        dragger.dragComponent(this, e, &constrainer);
        if (onPositionMoved)
            onPositionMoved();
    }

private:
    juce::ComponentDragger dragger;
    juce::ComponentBoundsConstrainer constrainer;
    juce::Image micImage;
    juce::String micName;
    juce::String roleText;
    float gainDb { 0.0f };
    bool isBypassed { false };
    juce::Colour accentColour;
    bool rightAligned { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FloatingMicOverlay)
};

// ============================================================================
// Layer 3: 97-Key Vector Sub-Bass Keyboard (C0 to C8)
// ============================================================================
class Bosendorfer97KeyComponent : public juce::Component
{
public:
    Bosendorfer97KeyComponent() = default;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const int totalWhiteKeys = 57;
        const float keyWidth = bounds.getWidth() / static_cast<float>(totalWhiteKeys);
        const float blackKeyHeight = bounds.getHeight() * 0.62f;

        g.fillAll(juce::Colours::black);

        for (int i = 0; i < totalWhiteKeys; ++i)
        {
            auto keyRect = juce::Rectangle<float>(static_cast<float>(i) * keyWidth, 0.0f, keyWidth - 1.0f, bounds.getHeight());

            if (i < 5)
            {
                g.setColour(juce::Colour(0xff22252a));
                g.fillRect(keyRect);
                g.setColour(juce::Colour(0xff3a3f47));
                g.drawRect(keyRect, 1.0f);
            }
            else
            {
                g.setColour(juce::Colour(0xfff5f3eb));
                g.fillRect(keyRect);
                g.setColour(juce::Colours::grey);
                g.drawRect(keyRect, 1.0f);
            }
        }

        int whiteIndex = 0;
        for (int midiNote = 12; midiNote <= 108; ++midiNote)
        {
            const int noteInOctave = midiNote % 12;
            const bool isBlack = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10);

            if (isBlack)
            {
                auto blackRect = juce::Rectangle<float>((static_cast<float>(whiteIndex) * keyWidth) - (keyWidth * 0.32f),
                                                        0.0f, keyWidth * 0.64f, blackKeyHeight);

                if (midiNote < 21)
                {
                    g.setColour(juce::Colour(0xffe8e8e8));
                    g.fillRect(blackRect);
                    g.setColour(juce::Colours::darkgrey);
                    g.drawRect(blackRect, 1.0f);
                }
                else
                {
                    g.setColour(juce::Colour(0xff121212));
                    g.fillRect(blackRect);
                }
            }
            else
            {
                whiteIndex++;
            }
        }
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Bosendorfer97KeyComponent)
};

// ============================================================================
// Main Layered Container
// ============================================================================
class PianoHarpViewComponent : public juce::Component
{
public:
    PianoHarpViewComponent(BsendorferMicDSP& dspEngine, MicAssetLoader& loaderEngine, const juce::File& assetFolder)
        : dsp(dspEngine), loader(loaderEngine), assetsDir(assetFolder),
          bassMicOverlay("Neuman TLM 103", juce::Colours::goldenrod, false),
          trebleMicOverlay("Neumann KM 184", juce::Colours::cyan, true)
    {
        stageGrandImage = juce::ImageFileFormat::loadFrom(assetsDir.getChildFile("stage_grand.png"));
        pianoHarpImage  = juce::ImageFileFormat::loadFrom(assetsDir.getChildFile("piano_harp.png"));

        const juce::StringArray micList = {
            "Neumann KM 184",
            "Neuman TLM 103",
            "Neumann U87",
            "RCA 44BX NEVE 1073"
        };

        bassMicSelector.addItemList(micList, 1);
        bassMicSelector.setSelectedId(2, juce::dontSendNotification);
        bassMicSelector.onChange = [this] { onMicSelectionChanged(MicSlot::Bass, bassMicSelector.getSelectedId() - 1); };
        addAndMakeVisible(bassMicSelector);

        trebleMicSelector.addItemList(micList, 1);
        trebleMicSelector.setSelectedId(1, juce::dontSendNotification);
        trebleMicSelector.onChange = [this] { onMicSelectionChanged(MicSlot::Treble, trebleMicSelector.getSelectedId() - 1); };
        addAndMakeVisible(trebleMicSelector);

        viewToggleButton.setButtonText("Switch to Harp / Mic View");
        viewToggleButton.setClickingTogglesState(true);
        viewToggleButton.onClick = [this] { updateViewMode(viewToggleButton.getToggleState()); };
        addAndMakeVisible(viewToggleButton);

        bypassToggle.setButtonText("Bypass Mic Section");
        bypassToggle.onClick = [this] { dsp.setBypassMicSection(bypassToggle.getToggleState()); };
        addAndMakeVisible(bypassToggle);

        addChildComponent(bassMicOverlay);
        addChildComponent(trebleMicOverlay);
        addAndMakeVisible(subBassKeyboard);

        updateViewMode(false);
        onMicSelectionChanged(MicSlot::Bass, 1);
        onMicSelectionChanged(MicSlot::Treble, 0);
    }

    void updateViewMode(bool isHarpViewActive)
    {
        showHarpView = isHarpViewActive;
        viewToggleButton.setButtonText(showHarpView ? "View: Open Soundboard (Harp)" : "View: Stage Grand");

        bassMicOverlay.setVisible(showHarpView);
        trebleMicOverlay.setVisible(showHarpView);

        if (showHarpView)
        {
            const int halfW = getWidth() / 2;
            const int overlayH = juce::jmax(180, getHeight() - 190);
            bassMicOverlay.setBounds(0, 46, halfW, overlayH);
            trebleMicOverlay.setBounds(halfW, 46, halfW, overlayH);
        }

        repaint();
    }

    void onMicSelectionChanged(MicSlot slot, int micIndex)
    {
        const auto& info = getMicModelAssets(micIndex);
        const char* pngName = (slot == MicSlot::Bass) ? info.leftPngFileName : info.rightPngFileName;

        juce::File pngFile = assetsDir.getChildFile(pngName);
        juce::Image img = juce::ImageFileFormat::loadFrom(pngFile);

        juce::File wavFile = assetsDir.getChildFile("IRs").getChildFile(info.wavFileName);
        if (!wavFile.existsAsFile())
            wavFile = assetsDir.getChildFile(info.wavFileName);
        loader.queueLoad(slot, wavFile);

        if (slot == MicSlot::Bass)
            bassMicOverlay.setMicImage(img, info.displayName);
        else
            trebleMicOverlay.setMicImage(img, info.displayName);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().removeFromTop(getHeight() - 110);

        if (showHarpView && pianoHarpImage.isValid())
        {
            g.drawImage(pianoHarpImage, area.toFloat(), juce::RectanglePlacement::fillDestination);
        }
        else if (!showHarpView && stageGrandImage.isValid())
        {
            g.drawImage(stageGrandImage, area.toFloat(), juce::RectanglePlacement::fillDestination);
        }
        else
        {
            g.fillAll(juce::Colour(0xff181b1f));
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        auto topBar = bounds.removeFromTop(42).reduced(6, 4);
        viewToggleButton.setBounds(topBar.removeFromLeft(210));
        topBar.removeFromLeft(12);
        bypassToggle.setBounds(topBar.removeFromLeft(160));
        topBar.removeFromLeft(12);
        bassMicSelector.setBounds(topBar.removeFromLeft(160));
        topBar.removeFromLeft(8);
        trebleMicSelector.setBounds(topBar.removeFromLeft(160));

        subBassKeyboard.setBounds(bounds.removeFromBottom(110));
    }

private:
    BsendorferMicDSP& dsp;
    MicAssetLoader& loader;
    juce::File assetsDir;

    bool showHarpView = false;
    juce::Image stageGrandImage;
    juce::Image pianoHarpImage;

    juce::ToggleButton viewToggleButton;
    juce::ToggleButton bypassToggle;
    juce::ComboBox bassMicSelector;
    juce::ComboBox trebleMicSelector;

    FloatingMicOverlay bassMicOverlay;
    FloatingMicOverlay trebleMicOverlay;

    Bosendorfer97KeyComponent subBassKeyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoHarpViewComponent)
};
