#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "IRShieldLoader.h"
#include "BinaryData.h"

namespace
{
    void getMicImpulseResource(int index, const void*& data, size_t& size)
    {
        switch (juce::jlimit(0, 3, index))
        {
            case 0:
                data = BinaryData::Mic_Neumann_KM184_bin;
                size = static_cast<size_t>(BinaryData::Mic_Neumann_KM184_binSize);
                return;
            case 1:
                data = BinaryData::Mic_Neumann_TLM103_bin;
                size = static_cast<size_t>(BinaryData::Mic_Neumann_TLM103_binSize);
                return;
            case 2:
                data = BinaryData::Mic_Neumann_U87_bin;
                size = static_cast<size_t>(BinaryData::Mic_Neumann_U87_binSize);
                return;
            case 3:
            default:
                data = BinaryData::RCA_44BX_NEVE_bin;
                size = static_cast<size_t>(BinaryData::RCA_44BX_NEVE_binSize);
                return;
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
                           ? static_cast<const void*>(BinaryData::BOSENBRI_bin)
                           : static_cast<const void*>(BinaryData::BOSENDRF_bin);
    const int size = (bank == EmbeddedBank::Bright)
                         ? BinaryData::BOSENBRI_binSize
                         : BinaryData::BOSENDRF_binSize;
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

    if (forceReload || currentBassMic != activeBassMicIndex)
    {
        activeBassMicIndex = currentBassMic;
        const void* data = nullptr;
        size_t size = 0;
        getMicImpulseResource(activeBassMicIndex, data, size);
        micLoader.queueMemoryLoad(MicSlot::Bass, data, size);
    }

    if (forceReload || currentTrebleMic != activeTrebleMicIndex)
    {
        activeTrebleMicIndex = currentTrebleMic;
        const void* data = nullptr;
        size_t size = 0;
        getMicImpulseResource(activeTrebleMicIndex, data, size);
        micLoader.queueMemoryLoad(MicSlot::Treble, data, size);
    }

    if (forceReload || currentRoom != activeRoomIndex)
    {
        activeRoomIndex = currentRoom;

        const void* rawData = nullptr;
        size_t rawSize = 0;

        switch (activeRoomIndex)
        {
            case 0: // Vienna Konzerthaus (~2.1s RT60)
                rawData = BinaryData::vienna_konzerthaus_enc_wav;
                rawSize = BinaryData::vienna_konzerthaus_enc_wavSize;
                break;

            case 1: // Maple Stage 40ft (~1.3s RT60)
                rawData = BinaryData::maple_stage_40ft_enc_wav;
                rawSize = BinaryData::maple_stage_40ft_enc_wavSize;
                break;

            case 2: // Scoring Studio Hall (~0.9s RT60)
                rawData = BinaryData::scoring_studio_hall_enc_wav;
                rawSize = BinaryData::scoring_studio_hall_enc_wavSize;
                break;

            default:
                break;
        }

        if (rawData != nullptr && rawSize > 0)
        {
            // Decrypt in RAM without writing unencrypted WAV files to disk
            auto decryptedBlock = IRShieldLoader::decryptIR(rawData, rawSize);
            acousticEngine.loadRoomImpulse(decryptedBlock.getData(), decryptedBlock.getSize());
        }
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
            const float rawVel = message.getFloatVelocity();
            lastNoteVelocity.store(rawVel, std::memory_order_relaxed);
            noteHitCounter.fetch_add(1, std::memory_order_relaxed);

            const float curvedVel = PianoAcousticEngine::applyVelocityCurve(rawVel, velocityCurve);
            sfEngine.noteOn(channel0, message.getNoteNumber(), curvedVel);

            if (keyNoiseGain > 0.05f)
                acousticEngine.triggerKeyNoiseSample(keyNoiseGain * rawVel);
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

    // 1. Check license state flag
    const bool licensed = LicenseManager::getInstance().isLicensed();

    if (!licensed)
    {
        // 15-minute trial demo mode: mute after timeout
        trialTimerSamples += buffer.getNumSamples();
        const double sampleRate = getSampleRate() > 0 ? getSampleRate() : 44100.0;

        if (trialTimerSamples > (int64_t)(15.0 * 60.0 * sampleRate))
        {
            buffer.clear();
            rmsLeft.store(0.0f, std::memory_order_relaxed);
            rmsRight.store(0.0f, std::memory_order_relaxed);
            return;
        }
    }

    // 2. Standard TSF MIDI & convolution processing continues below...
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

    // Push post-convolution stereo samples into lock-free ring buffer for CenterMonitorView goniometer
    const float* outL = buffer.getReadPointer(0);
    const float* outR = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : outL;
    const int step = juce::jmax(1, numSamples / 32);
    for (int i = 0; i < numSamples; i += step)
    {
        const int writeIdx = scopeWriteIndex.fetch_add(1, std::memory_order_relaxed) & (kScopeBufferSize - 1);
        scopeRingL[static_cast<size_t>(writeIdx)].store(outL[i], std::memory_order_relaxed);
        scopeRingR[static_cast<size_t>(writeIdx)].store(outR[i], std::memory_order_relaxed);
    }
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

#if JUCE_LINUX
// Provide local definitions for ALSA 1.2.10+ / 1.2.13+ UMP symbols so the linker
// resolves JUCE 9's weak UMP declarations internally rather than binding to
// ALSA_1.2.10 / ALSA_1.2.13 version nodes in the build container's libasound.so.2.
// This ensures the binary launches on any Linux distro with ALSA >= 1.0 (MIDI 1.0 sequencer).
extern "C"
{
    struct _snd_seq_port_info;
    unsigned int snd_seq_port_info_get_capability(const _snd_seq_port_info*);

    int snd_seq_client_info_get_midi_version(const void*) { return 0; }
    int snd_seq_set_client_midi_version(void*, int) { return -38; }
    int snd_seq_set_client_ump_conversion(void*, int) { return -38; }
    int snd_seq_ump_event_input(void*, void**) { return -38; }
    int snd_seq_ump_event_output_direct(void*, void*) { return -38; }
    int snd_seq_port_info_get_ump_is_midi1(const void*) { return 1; }
    int snd_seq_port_info_get_ump_group(const void*) { return 0; }
    int snd_seq_create_ump_endpoint(void*, const void*, unsigned int) { return -38; }
    int snd_seq_create_ump_block(void*, int, const void*) { return -38; }
    int snd_seq_set_ump_block_info(void*, int, const void*) { return -38; }
    int snd_seq_get_ump_block_info(void*, int, int, void*) { return -38; }
    int snd_seq_get_ump_endpoint_info(void*, int, void*) { return -38; }

    const char* snd_ump_endpoint_info_get_product_id(const void*) { return ""; }
    const unsigned char* snd_ump_endpoint_info_get_sw_revision(const void*)
    {
        static const unsigned char rev[4] = { 0, 0, 0, 0 };
        return rev;
    }
    unsigned int snd_ump_endpoint_info_get_family_id(const void*) { return 0; }
    unsigned int snd_ump_endpoint_info_get_flags(const void*) { return 0; }
    unsigned int snd_ump_endpoint_info_get_manufacturer_id(const void*) { return 0; }
    unsigned int snd_ump_endpoint_info_get_model_id(const void*) { return 0; }
    unsigned int snd_ump_endpoint_info_get_num_blocks(const void*) { return 0; }
    unsigned int snd_ump_endpoint_info_get_protocol(const void*) { return 0; }
    unsigned int snd_ump_endpoint_info_get_protocol_caps(const void*) { return 0; }
    unsigned int snd_ump_endpoint_info_get_version(const void*) { return 0; }

    void snd_ump_endpoint_info_set_product_id(void*, const char*) {}
    void snd_ump_endpoint_info_set_sw_revision(void*, const unsigned char*) {}
    void snd_ump_endpoint_info_set_family_id(void*, unsigned int) {}
    void snd_ump_endpoint_info_set_flags(void*, unsigned int) {}
    void snd_ump_endpoint_info_set_manufacturer_id(void*, unsigned int) {}
    void snd_ump_endpoint_info_set_model_id(void*, unsigned int) {}
    void snd_ump_endpoint_info_set_name(void*, const char*) {}
    void snd_ump_endpoint_info_set_num_blocks(void*, unsigned int) {}
    void snd_ump_endpoint_info_set_protocol(void*, unsigned int) {}
    void snd_ump_endpoint_info_set_protocol_caps(void*, unsigned int) {}
    void snd_ump_endpoint_info_set_version(void*, unsigned int) {}

    const char* snd_ump_block_info_get_name(const void*) { return ""; }
    unsigned int snd_ump_block_info_get_active(const void*) { return 0; }
    unsigned int snd_ump_block_info_get_direction(const void*) { return 0; }
    unsigned int snd_ump_block_info_get_first_group(const void*) { return 0; }
    unsigned int snd_ump_block_info_get_flags(const void*) { return 0; }
    unsigned int snd_ump_block_info_get_num_groups(const void*) { return 0; }
    unsigned int snd_ump_block_info_get_sysex8_streams(const void*) { return 0; }
    unsigned int snd_ump_block_info_get_ui_hint(const void*) { return 0; }

    void snd_ump_block_info_set_name(void*, const char*) {}
    void snd_ump_block_info_set_active(void*, unsigned int) {}
    void snd_ump_block_info_set_direction(void*, unsigned int) {}
    void snd_ump_block_info_set_first_group(void*, unsigned int) {}
    void snd_ump_block_info_set_flags(void*, unsigned int) {}
    void snd_ump_block_info_set_num_groups(void*, unsigned int) {}
    void snd_ump_block_info_set_sysex8_streams(void*, unsigned int) {}
    void snd_ump_block_info_set_ui_hint(void*, unsigned int) {}
    void snd_ump_block_info_set_block_id(void*, unsigned int) {}

    int snd_seq_port_info_get_direction(const _snd_seq_port_info* port)
    {
        if (port == nullptr)
            return 0;
        const unsigned int caps = snd_seq_port_info_get_capability(port);
        const bool canRead  = (caps & (1u << 0)) != 0; // SND_SEQ_PORT_CAP_READ
        const bool canWrite = (caps & (1u << 1)) != 0; // SND_SEQ_PORT_CAP_WRITE
        if (canRead && canWrite) return 3;             // SND_SEQ_PORT_DIR_BIDIRECTION
        if (canRead)             return 1;             // SND_SEQ_PORT_DIR_INPUT
        if (canWrite)            return 2;             // SND_SEQ_PORT_DIR_OUTPUT
        return 0;
    }

    size_t snd_ump_block_info_sizeof(void) { return 64; }
    size_t snd_ump_endpoint_info_sizeof(void) { return 64; }
}
#endif
