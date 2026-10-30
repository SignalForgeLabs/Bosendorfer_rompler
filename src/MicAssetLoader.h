#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <queue>
#include <mutex>
#include <functional>
#include "BsendorferMicDSP.h"
#include "PianoAcousticEngine.h"

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

        if (job.irFile.existsAsFile())
        {
            reader.reset(formatManager.createReaderFor(job.irFile));
        }
        else if (job.rawData != nullptr && job.rawDataSize > 0)
        {
            auto memInput = std::make_unique<juce::MemoryInputStream>(job.rawData, job.rawDataSize, false);
            reader.reset(formatManager.createReaderFor(std::move(memInput)));
        }

        if (!reader)
            return;

        // Read up to 2048 samples (minimum-phase mic FIR filter)
        const int samplesToRead = static_cast<int>(std::min<juce::int64>(reader->lengthInSamples, 2048));
        if (samplesToRead <= 0)
            return;

        juce::AudioBuffer<float> tempBuffer(static_cast<int>(reader->numChannels), samplesToRead);
        reader->read(&tempBuffer, 0, samplesToRead, 0, true, true);

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

        const int trimmedLength = samplesToRead - firstNonZeroSample;
        if (trimmedLength <= 0)
            return;

        juce::MemoryBlock memBlock;
        juce::WavAudioFormat wavFormat;
        {
            // AudioFormatWriter takes ownership of the OutputStream* and deletes it in ~AudioFormatWriter
            auto* memStream = new juce::MemoryOutputStream(memBlock, false);
            std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
                memStream,
                reader->sampleRate,
                static_cast<unsigned int>(tempBuffer.getNumChannels()),
                24,
                {},
                0));

            if (writer != nullptr)
            {
                writer->writeFromAudioSampleBuffer(tempBuffer, firstNonZeroSample, trimmedLength);
                writer->flush();
            }
            else
            {
                delete memStream;
            }
        }

        // Push trimmed WAV memory block into JUCE DSP convolution
        if (memBlock.getSize() > 0)
        {
            if (dsp != nullptr)
            {
                if (job.slot == MicSlot::Bass)
                    dsp->loadBassIR(memBlock.getData(), memBlock.getSize());
                else
                    dsp->loadTrebleIR(memBlock.getData(), memBlock.getSize());
            }

            if (acoustic != nullptr)
            {
                if (job.slot == MicSlot::Bass)
                    acoustic->loadBassMicImpulse(memBlock.getData(), memBlock.getSize());
                else
                    acoustic->loadTrebleMicImpulse(memBlock.getData(), memBlock.getSize());
            }
        }
    }

    BsendorferMicDSP* dsp { nullptr };
    PianoAcousticEngine* acoustic { nullptr };
    juce::AudioFormatManager formatManager;
    std::queue<MicLoadJob> pendingJobs;
    std::mutex queueMutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MicAssetLoader)
};

