#pragma once
#include <juce_core/juce_core.h>
#include "BinaryData.h"

#define vienna_konzerthaus_enc_wav          Room_ViennaKonzerthaus_bin
#define vienna_konzerthaus_enc_wavSize      Room_ViennaKonzerthaus_binSize
#define maple_stage_40ft_enc_wav            Room_MapleStage40ft_bin
#define maple_stage_40ft_enc_wavSize        Room_MapleStage40ft_binSize
#define scoring_studio_hall_enc_wav         Room_ScoringStudioHall_bin
#define scoring_studio_hall_enc_wavSize     Room_ScoringStudioHall_binSize

class IRShieldLoader
{
public:
    static juce::MemoryBlock decryptIR(const void* scrambledData, size_t dataSize)
    {
        const juce::String key = "SFALabs_AudioShield_2026_AJ_Key";
        const auto* keyPtr = reinterpret_cast<const uint8_t*>(key.toRawUTF8());
        const size_t keyLen = key.getNumBytesAsUTF8();

        juce::MemoryBlock decrypted(scrambledData, dataSize);
        auto* dest = static_cast<uint8_t*>(decrypted.getData());

        for (size_t i = 0; i < dataSize; ++i)
        {
            dest[i] ^= keyPtr[i % keyLen];
        }

        return decrypted;
    }
};
