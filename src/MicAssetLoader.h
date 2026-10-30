#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <queue>
#include <mutex>
#include <functional>
#include "BsendorferMicDSP.h"
#include "PianoAcousticEngine.h"
#include "AssetShield.h"

enum class MicSlot
{
    Bass,
    Treble
};

struct MicLoadJob
{
    MicSlot slot { MicSlot::Bass };
    juce::File irFile;
    const void* rawData { nullptr };
    size_t rawDataSize { 0 };
};

class MicAssetLoader : private juce::Thread
{
public:
    explicit MicAssetLoader(BsendorferMicDSP& dspEngine)
        : juce::Thread("MicAssetLoaderThread"),
          dsp(&dspEngine),
          acoustic(nullptr)
    {
        formatManager.registerBasicFormats();
        startThread(juce::Thread::Priority::normal);
    }

    explicit MicAssetLoader(PianoAcousticEngine& acousticEngine)
        : juce::Thread("MicAssetLoaderThread"),
          dsp(nullptr),
          acoustic(&acousticEngine)
    {
        formatManager.registerBasicFormats();
        startThread(juce::Thread::Priority::normal);
    }

    ~MicAssetLoader() override
    {
        signalThreadShouldExit();
        notify();
        stopThread(2000);
    }

    void queueLoad(MicSlot slot, const juce::File& file)
    {
        {
            const std::lock_guard<std::mutex> lock(queueMutex);
            pendingJobs.push({ slot, file, nullptr, 0 });
        }
        notify();
    }

    void queueMemoryLoad(MicSlot slot, const void* wavData, size_t dataSize)
    {
        if (wavData == nullptr || dataSize == 0)
            return;

        {
            const std::lock_guard<std::mutex> lock(queueMutex);
            pendingJobs.push({ slot, juce::File(), wavData, dataSize });
        }
        notify();
    }

private:
    void run() override
    {
        while (!threadShouldExit())
        {
            MicLoadJob currentJob;
            bool hasJob = false;

            {
                const std::lock_guard<std::mutex> lock(queueMutex);
                if (!pendingJobs.empty())
                {
                    currentJob = pendingJobs.front();
                    pendingJobs.pop();
                    hasJob = true;
                }
            }

            if (hasJob)
            {
                processJob(currentJob);
            }
            else
            {
                wait(250);
            }
        }
    }

    void processJob(const MicLoadJob& job)
    {
        std::unique_ptr<juce::AudioFormatReader> reader;
        juce::MemoryBlock decryptedBlock;

        if (job.irFile.existsAsFile())
        {
            reader.reset(formatManager.createReaderFor(job.irFile));
        }
        else if (job.rawData != nullptr && job.rawDataSize > 0)
        {
            decryptedBlock = AssetShield::descramble(job.rawData, job.rawDataSize);
            auto memInput = std::make_unique<juce::MemoryInputStream>(decryptedBlock.getData(), decryptedBlock.getSize(), false);
            reader.reset(formatManager.createReaderFor(std::move(memInput)));
        }

        if (!reader)
        {
            decryptedBlock.fillWith(0);
            return;
        }

        // Read up to 2048 samples (minimum-phase mic FIR filter)
        const int samplesToRead = static_cast<int>(std::min<juce::int64>(reader->lengthInSamples, 2048));
        if (samplesToRead <= 0)
        {
            decryptedBlock.fillWith(0);
            return;
        }

        const double irSampleRate = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
        const int numCh = juce::jmax(1, static_cast<int>(reader->numChannels));
        juce::AudioBuffer<float> tempBuffer(numCh, samplesToRead);
        reader->read(&tempBuffer, 0, samplesToRead, 0, true, true);
        reader.reset();
        decryptedBlock.fillWith(0);

        // Strip leading dead air to sample 0 to guarantee perfect alignment
        int firstNonZeroSample = 0;
        const float threshold = 0.0001f;
        for (int i = 0; i < samplesToRead; ++i)
        {
            float maxSample = 0.0f;
            for (int ch = 0; ch < tempBuffer.getNumChannels(); ++ch)
                maxSample = std::max(maxSample, std::abs(tempBuffer.getSample(ch, i)));

            if (maxSample > threshold)
            {
                firstNonZeroSample = i;
                break;
            }
        }

        const int trimmedLength = juce::jmin(1024, samplesToRead - firstNonZeroSample);
        if (trimmedLength <= 0)
            return;

        juce::AudioBuffer<float> trimmedBuffer(numCh, trimmedLength);
        for (int ch = 0; ch < numCh; ++ch)
            trimmedBuffer.copyFrom(ch, 0, tempBuffer, ch, firstNonZeroSample, trimmedLength);

        if (acoustic != nullptr)
        {
            if (job.slot == MicSlot::Bass)
                acoustic->loadBassMicImpulse(std::move(trimmedBuffer), irSampleRate);
            else
                acoustic->loadTrebleMicImpulse(std::move(trimmedBuffer), irSampleRate);
        }
    }

    BsendorferMicDSP* dsp { nullptr };
    PianoAcousticEngine* acoustic { nullptr };
    juce::AudioFormatManager formatManager;
    std::queue<MicLoadJob> pendingJobs;
    std::mutex queueMutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MicAssetLoader)
};

