#pragma once
#include <juce_core/juce_core.h>
#include "BinaryData.h"

#define venue_dry_enc_wav                   Venue_Dry_bin
#define venue_dry_enc_wavSize               Venue_Dry_binSize
#define vienna_konzerthaus_enc_wav          ConradPrebysConcertHallSeatF111_bin
#define vienna_konzerthaus_enc_wavSize      ConradPrebysConcertHallSeatF111_binSize
#define maple_stage_40ft_enc_wav            MillsGreekTheater_bin
#define maple_stage_40ft_enc_wavSize        MillsGreekTheater_binSize
#define naumburg_bandshell_enc_wav          NaumburgBandshell_bin
#define naumburg_bandshell_enc_wavSize      NaumburgBandshell_binSize
#define scoring_studio_hall_enc_wav         SteinmanHall_bin
#define scoring_studio_hall_enc_wavSize     SteinmanHall_binSize
#define art_gallery_enc_wav                 Space4ArtGallery_bin
#define art_gallery_enc_wavSize             Space4ArtGallery_binSize

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
