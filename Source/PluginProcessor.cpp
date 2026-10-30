#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    void getMicImpulseResource(int index, const void*& data, size_t& size)
    {
        switch (juce::jlimit(0, 3, index))
        {
            case 0:
                data = BinaryData::Mic_Neumann_KM184_wav;
                size = static_cast<size_t>(BinaryData::Mic_Neumann_KM184_wavSize);
                return;
            case 1:
                data = BinaryData::Mic_Neumann_TLM103_wav;
                size = static_cast<size_t>(BinaryData::Mic_Neumann_TLM103_wavSize);
                return;
            case 2:
                data = BinaryData::Mic_Neumann_U87_wav;
                size = static_cast<size_t>(BinaryData::Mic_Neumann_U87_wavSize);
                return;
            case 3:
            default:
                data = BinaryData::RCA_44BX_NEVE_wav;
                size = static_cast<size_t>(BinaryData::RCA_44BX_NEVE_wavSize);
                return;
        }
    }

    void getRoomImpulseResource(int index, const void*& data, size_t& size)
    {
        switch (index)
        {
            case 0:
                data = BinaryData::Room_ViennaKonzerthaus_wav;
                size = static_cast<size_t>(BinaryData::Room_ViennaKonzerthaus_wavSize);
                break;
            case 1:
                data = BinaryData::Room_MapleStage40ft_wav;
                size = static_cast<size_t>(BinaryData::Room_MapleStage40ft_wavSize);
                break;
            case 2:
            default:
                data = BinaryData::Room_ScoringStudioHall_wav;
                size = static_cast<size_t>(BinaryData::Room_ScoringStudioHall_wavSize);
                break;
        }
    }
}

BosendorferAudioProcessor::BosendorferAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    LicenseManager::getInstance().checkStoredLicense();
    loadEmbeddedSoundFont(EmbeddedBank::Standard);
}

BosendorferAudioProcessor::~BosendorferAudioProcessor() = default;

const juce::String BosendorferAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool BosendorferAudioProcessor::acceptsMidi() const { return true; }
bool BosendorferAudioProcessor::producesMidi() const { return false; }
bool BosendorferAudioProcessor::isMidiEffect() const { return false; }
double BosendorferAudioProcessor::getTailLengthSeconds() const { return 3.5; }

int BosendorferAudioProcessor::getNumPrograms()
{
    return juce::jmax(1, sfEngine.getPresetCount());
}

int BosendorferAudioProcessor::getCurrentProgram()
{
    return sfEngine.getPresetIndex();
}

void BosendorferAudioProcessor::setCurrentProgram(int index)
{
    selectPreset(index);
}

const juce::String BosendorferAudioProcessor::getProgramName(int index)
{
    const auto presets = sfEngine.getPresetNames();
    if (juce::isPositiveAndBelow(index, presets.size()))
        return presets[index];
    return "Bosendorfer Imperial";
}

void BosendorferAudioProcessor::changeProgramName(int, const juce::String&) {}

bool BosendorferAudioProcessor::loadEmbeddedSoundFont(EmbeddedBank bank)
{
    const void* data = (bank == EmbeddedBank::Bright)
                           ? static_cast<const void*>(BinaryData::BOSENBRI_SF2)
                           : static_cast<const void*>(BinaryData::BOSENDRF_SF2);
    const int size = (bank == EmbeddedBank::Bright)
                         ? BinaryData::BOSENBRI_SF2Size
                         : BinaryData::BOSENDRF_SF2Size;
    const juce::String label = (bank == EmbeddedBank::Bright)
                                   ? "Brite"
                                   : "Standard";

    const bool ok = sfEngine.loadFromMemory(data, size, label);

    if (ok)
    {
        usingExternalSf2.store(false, std::memory_order_relaxed);
        currentEmbeddedBank.store(bank, std::memory_order_relaxed);
        externalSf2Path.clear();
        apvts.state.setProperty("externalSf2Path", "", nullptr);
        apvts.state.setProperty("embeddedBank", static_cast<int>(bank), nullptr);
        apvts.state.setProperty("presetIndex", sfEngine.getPresetIndex(), nullptr);
    }
    return ok;
}

bool BosendorferAudioProcessor::loadExternalSoundFont(const juce::File& sf2File)
{
    if (!sfEngine.loadFromFile(sf2File))
        return false;

    usingExternalSf2.store(true, std::memory_order_relaxed);
    externalSf2Path = sf2File.getFullPathName();
    apvts.state.setProperty("externalSf2Path", externalSf2Path, nullptr);
    apvts.state.setProperty("presetIndex", sfEngine.getPresetIndex(), nullptr);
    return true;
}

void BosendorferAudioProcessor::selectPreset(int presetIndex)
{
    sfEngine.setPresetIndex(presetIndex);
    apvts.state.setProperty("presetIndex", sfEngine.getPresetIndex(), nullptr);
}

void BosendorferAudioProcessor::setSustainPedalFromUI(bool down)
{
    sustainPedalDown.store(down, std::memory_order_relaxed);
    pendingSustainEvent.store(true, std::memory_order_relaxed);
}

void BosendorferAudioProcessor::setSoftPedalFromUI(bool down)
{
    softPedalDown.store(down, std::memory_order_relaxed);
}

void BosendorferAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    pedalWasDown = sustainPedalDown.load(std::memory_order_relaxed);
    pedalDownTimestamp = 0.0;
    damperClunk.prepare(currentSampleRate);

    sfEngine.prepare(sampleRate, samplesPerBlock);

    if (!sfEngine.isLoaded())
        loadEmbeddedSoundFont(currentEmbeddedBank.load(std::memory_order_relaxed));

    const int safeBlock = juce::jmax(1, samplesPerBlock);
    bufferBass.setSize(2, safeBlock);
    bufferTreble.setSize(2, safeBlock);
    bufferBass.clear();
    bufferTreble.clear();

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(safeBlock);
    spec.numChannels = 2;

    acousticEngine.prepare(spec);
    updateImpulses(true);
}

void BosendorferAudioProcessor::handleSustainPedalCc64(int ccVal)
{
    const bool isDown = (ccVal >= 64);
    const float pedalNoiseSetting = apvts.getRawParameterValue("pedal_noise")->load(); // 0.0 to 1.0

    if (isDown && !pedalWasDown)
    {
        pedalDownTimestamp = juce::Time::getMillisecondCounterHiRes();
        if (pedalNoiseSetting > 0.01f)
            acousticEngine.triggerPedalLiftSample(pedalNoiseSetting);
    }
    else if (!isDown && pedalWasDown)
    {
        const double durationHeldMs = juce::Time::getMillisecondCounterHiRes() - pedalDownTimestamp;
        juce::ignoreUnused(durationHeldMs);

        if (pedalNoiseSetting > 0.01f)
        {
            // Rapid foot release: full velocity thump
            // For standard 0/127 switches, duration held or sudden release sets full snap
            damperClunk.trigger(1.0f, pedalNoiseSetting);
        }
    }

    pedalWasDown = isDown;
    sustainPedalDown.store(isDown, std::memory_order_relaxed);
}

void BosendorferAudioProcessor::releaseResources()
{
    sfEngine.allNotesOff();
    damperClunk.reset();
    acousticEngine.reset();
}

bool BosendorferAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void BosendorferAudioProcessor::updateImpulses(bool forceReload)
{
    const int currentBassMic   = static_cast<int>(apvts.getRawParameterValue(IDs::bassMicType.getParamID())->load());
    const int currentTrebleMic = static_cast<int>(apvts.getRawParameterValue(IDs::trebleMicType.getParamID())->load());
    const int currentRoom      = static_cast<int>(apvts.getRawParameterValue(IDs::roomIrType.getParamID())->load());

#ifndef ASSETS_DIR_PATH
#define ASSETS_DIR_PATH "/mnt/Storage/Projects/Bosendoter_mperial_Grand/assets"
#endif

    if (forceReload || currentBassMic != activeBassMicIndex)
    {
        activeBassMicIndex = currentBassMic;
        const juce::String wavName = getMicModelAssets(activeBassMicIndex).wavFileName;
        juce::File localWav = juce::File(ASSETS_DIR_PATH).getChildFile("IRs").getChildFile(wavName);
        if (!localWav.existsAsFile())
            localWav = juce::File(ASSETS_DIR_PATH).getChildFile(wavName);

        if (localWav.existsAsFile())
        {
            micLoader.queueLoad(MicSlot::Bass, localWav);
        }
        else
        {
            const void* data = nullptr;
            size_t size = 0;
            getMicImpulseResource(activeBassMicIndex, data, size);
            micLoader.queueMemoryLoad(MicSlot::Bass, data, size);
        }
    }

    if (forceReload || currentTrebleMic != activeTrebleMicIndex)
    {
        activeTrebleMicIndex = currentTrebleMic;
        const juce::String wavName = getMicModelAssets(activeTrebleMicIndex).wavFileName;
        juce::File localWav = juce::File(ASSETS_DIR_PATH).getChildFile("IRs").getChildFile(wavName);
        if (!localWav.existsAsFile())
            localWav = juce::File(ASSETS_DIR_PATH).getChildFile(wavName);

        if (localWav.existsAsFile())
        {
            micLoader.queueLoad(MicSlot::Treble, localWav);
        }
        else
        {
            const void* data = nullptr;
            size_t size = 0;
            getMicImpulseResource(activeTrebleMicIndex, data, size);
            micLoader.queueMemoryLoad(MicSlot::Treble, data, size);
        }
    }

    if (forceReload || currentRoom != activeRoomIndex)
    {
        activeRoomIndex = currentRoom;
        const void* data = nullptr;
        size_t size = 0;
        getRoomImpulseResource(activeRoomIndex, data, size);
        acousticEngine.loadRoomImpulse(data, size);
    }
}

void BosendorferAudioProcessor::renderSoundFontBlock(juce::AudioBuffer<float>& buffer,
                                                     juce::MidiBuffer& midiMessages,
                                                     float velocityCurve,
                                                     float keyNoiseGain,
                                                     float pedalNoiseGain,
                                                     bool micBypassed,
                                                     float splitCenterNote,
                                                     float splitCrossfadeWidth)
{
    const int totalSamples = buffer.getNumSamples();
    int currentSample = 0;

    if (!micBypassed)
    {
        if (bufferBass.getNumSamples() < totalSamples)
        {
            bufferBass.setSize(2, totalSamples, false, false, true);
            bufferTreble.setSize(2, totalSamples, false, false, true);
        }
        bufferBass.clear(0, totalSamples);
        bufferTreble.clear(0, totalSamples);
    }

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        const int messageSample = juce::jlimit(0, totalSamples, metadata.samplePosition);

        if (messageSample > currentSample)
        {
            const int samplesToRender = messageSample - currentSample;
            if (micBypassed)
            {
                juce::AudioBuffer<float> subBuffer(
                    buffer.getArrayOfWritePointers(),
                    buffer.getNumChannels(),
                    currentSample,
                    samplesToRender);
                sfEngine.renderBlock(subBuffer);
            }
            else
            {
                sfEngine.renderSplitBlock(bufferBass, bufferTreble,
                                          currentSample, samplesToRender,
                                          splitCenterNote, splitCrossfadeWidth);
            }
            currentSample = messageSample;
        }

        const int channel0 = juce::jlimit(0, 15, message.getChannel() - 1);

        if (message.isNoteOn())
        {
            // Velocity curve strictly governs how key strike velocity maps to sample layer selection & volume
            const float curvedVel = PianoAcousticEngine::applyVelocityCurve(message.getFloatVelocity(), velocityCurve);
            sfEngine.noteOn(channel0, message.getNoteNumber(), curvedVel);

            if (keyNoiseGain > 0.05f)
                acousticEngine.triggerKeyNoiseSample(keyNoiseGain * message.getFloatVelocity());
        }
        else if (message.isNoteOff())
        {
            sfEngine.noteOff(channel0, message.getNoteNumber());
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            sfEngine.allNotesOff(channel0);
        }
        else if (message.isPitchWheel())
        {
            sfEngine.pitchBend(channel0, message.getPitchWheelValue());
        }
        else if (message.isController())
        {
            const int cc = message.getControllerNumber();
            const int val = message.getControllerValue();
            if (cc == 64)
            {
                juce::ignoreUnused(pedalNoiseGain);
                handleSustainPedalCc64(val);
            }
            else if (cc == 67)
            {
                softPedalDown.store(val >= 64, std::memory_order_relaxed);
            }

            sfEngine.controlChange(channel0, cc, val);
        }
        else if (message.isProgramChange())
        {
            selectPreset(message.getProgramChangeNumber());
        }
    }

    if (currentSample < totalSamples)
    {
        const int samplesToRender = totalSamples - currentSample;
        if (micBypassed)
        {
            juce::AudioBuffer<float> subBuffer(
                buffer.getArrayOfWritePointers(),
                buffer.getNumChannels(),
                currentSample,
                samplesToRender);
            sfEngine.renderBlock(subBuffer);
        }
        else
        {
            sfEngine.renderSplitBlock(bufferBass, bufferTreble,
                                      currentSample, samplesToRender,
                                      splitCenterNote, splitCrossfadeWidth);
        }
    }
}

void BosendorferAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || !sfEngine.isLoaded())
        return;

    // Merge on-screen 97-key keyboard state with incoming hardware MIDI
    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

    const float velCurve       = apvts.getRawParameterValue(IDs::velCurve.getParamID())->load();
    const float keyNoiseGain   = apvts.getRawParameterValue(IDs::keyNoise.getParamID())->load();
    const float pedalNoiseGain = apvts.getRawParameterValue(IDs::pedalNoise.getParamID())->load();
    const float lidPos         = apvts.getRawParameterValue(IDs::lidPosition.getParamID())->load();
    const float tuningSt       = apvts.getRawParameterValue(IDs::tuningSt.getParamID())->load();
    const bool micBypassed     = apvts.getRawParameterValue(IDs::micSectionBypassed.getParamID())->load() > 0.5f;
    const float splitCenter    = apvts.getRawParameterValue(IDs::splitCenter.getParamID())->load();
    const float splitWidth     = apvts.getRawParameterValue(IDs::splitWidth.getParamID())->load();

    if (pendingSustainEvent.exchange(false, std::memory_order_relaxed))
    {
        const int ccVal = sustainPedalDown.load(std::memory_order_relaxed) ? 127 : 0;
        handleSustainPedalCc64(ccVal);
        for (int ch = 0; ch < 16; ++ch)
            sfEngine.controlChange(ch, 64, ccVal);
    }

    sfEngine.setTuningSemitones(tuningSt);
    acousticEngine.setMicSectionBypassed(micBypassed);
    acousticEngine.setLidPosition(lidPos, softPedalDown.load(std::memory_order_relaxed));

    // 1. Render TSF sample voices into buffer
    renderSoundFontBlock(buffer, midiMessages, velCurve, keyNoiseGain, pedalNoiseGain, micBypassed, splitCenter, splitWidth);

    // 2. Synthesize damper clunk into buffer:
    damperClunk.processAndAdd(buffer, numSamples);

    // 3. Run buffer into acousticEngine (mics + crossfeed + room IR)
    // The synthesized clunk naturally reflects through the mic and hall convolution!
    updateImpulses(false);

    const float bassGain   = apvts.getRawParameterValue(IDs::bassMicGain.getParamID())->load();
    const float trebleGain = apvts.getRawParameterValue(IDs::trebleMicGain.getParamID())->load();
    const float crossfeed  = apvts.getRawParameterValue(IDs::micCrossfeed.getParamID())->load();
    const float roomMix    = apvts.getRawParameterValue(IDs::roomMix.getParamID())->load();
    const float masterGain = apvts.getRawParameterValue(IDs::masterGain.getParamID())->load();

    acousticEngine.processSplit(buffer, bufferBass, bufferTreble, bassGain, trebleGain, crossfeed, roomMix);

    // 4. Apply Master Volume (with Una Corda -2.5 dB attenuation when soft pedal is active)
    const float unaCordaDb = softPedalDown.load(std::memory_order_relaxed) ? -2.5f : 0.0f;
    buffer.applyGain(juce::Decibels::decibelsToGain(masterGain + unaCordaDb));

    rmsLeft.store(buffer.getRMSLevel(0, 0, numSamples), std::memory_order_relaxed);
    rmsRight.store(buffer.getNumChannels() > 1 ? buffer.getRMSLevel(1, 0, numSamples) : rmsLeft.load(std::memory_order_relaxed),
                   std::memory_order_relaxed);
}

bool BosendorferAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* BosendorferAudioProcessor::createEditor()
{
    return new BosendorferAudioProcessorEditor(*this);
}

void BosendorferAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("externalSf2Path", externalSf2Path, nullptr);
    state.setProperty("embeddedBank", static_cast<int>(currentEmbeddedBank.load(std::memory_order_relaxed)), nullptr);
    state.setProperty("presetIndex", sfEngine.getPresetIndex(), nullptr);

    if (std::unique_ptr<juce::XmlElement> xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void BosendorferAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        auto restoredState = juce::ValueTree::fromXml(*xmlState);
        apvts.replaceState(restoredState);

        const juce::String savedPath = restoredState.getProperty("externalSf2Path", "").toString();
        const int savedBankInt = static_cast<int>(restoredState.getProperty("embeddedBank", 0));
        const auto savedBank = (savedBankInt == 1) ? EmbeddedBank::Bright : EmbeddedBank::Standard;
        const int savedPreset = static_cast<int>(restoredState.getProperty("presetIndex", 0));

        if (savedPath.isNotEmpty())
        {
            juce::File extFile(savedPath);
            if (!loadExternalSoundFont(extFile))
                loadEmbeddedSoundFont(savedBank);
        }
        else
        {
            loadEmbeddedSoundFont(savedBank);
        }

        selectPreset(savedPreset);
        updateImpulses(true);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BosendorferAudioProcessor();
}
