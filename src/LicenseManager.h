#pragma once
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include <atomic>

class LicenseManager
{
public:
    static LicenseManager& getInstance()
    {
        static LicenseManager instance;
        return instance;
    }

    bool isLicensed() const { return licenseValid.load(std::memory_order_relaxed); }

    juce::String getLicensedEmail() const { return licensedEmail; }

    void checkStoredLicense()
    {
        auto licenseFile = getLicenseFile();
        if (licenseFile.existsAsFile())
        {
            auto content = licenseFile.loadFileAsString();
            auto tokens = juce::StringArray::fromTokens(content, "|", "");
            if (tokens.size() == 2)
            {
                const auto email = tokens[0].trim();
                const auto key   = tokens[1].trim();
                if (verifyKey(email, key))
                {
                    licensedEmail = email;
                    licenseValid.store(true, std::memory_order_relaxed);
                    return;
                }
            }
        }
        licenseValid.store(false, std::memory_order_relaxed);
    }

    bool activateLicense(const juce::String& email, const juce::String& serialKey)
    {
        if (verifyKey(email, serialKey))
        {
            auto licenseFile = getLicenseFile();
            licenseFile.getParentDirectory().createDirectory();
            licenseFile.replaceWithText(email.trim() + "|" + serialKey.trim().toUpperCase());
            licensedEmail = email.trim();
            licenseValid.store(true, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    juce::File getLicenseFile() const
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("SignalForge");
        auto viennaLic = dir.getChildFile("ClassicVienna290.lic");
        auto legacyLic = dir.getChildFile("Bosendorfer290.lic");
        if (!viennaLic.existsAsFile() && legacyLic.existsAsFile())
            return legacyLic;
        return viennaLic;
    }

private:
    LicenseManager() { checkStoredLicense(); }

    bool verifyKey(const juce::String& email, const juce::String& serialKey) const
    {
        if (email.isEmpty() || serialKey.isEmpty())
            return false;

        // Shared secret matched against sfalabs.digital key generator
        const juce::String sharedSecret = "SFA-BOSEN-290-SECURE-KEY-2026";

        // Compare formatted output against serial format: XXXX-XXXX-XXXX-XXXX
        return serialKey.trim().equalsIgnoreCase(generateExpectedSerial(email, sharedSecret));
    }

    juce::String generateExpectedSerial(const juce::String& email, const juce::String& secret) const
    {
        const juce::String payload = email.toLowerCase().trim() + secret;
        juce::SHA256 sha(payload.toUTF8());
        auto rawHex = sha.toHexString().toUpperCase();

        // Format into 16-char product key: ABCD-EFGH-IJKL-MNOP
        return rawHex.substring(0, 4) + "-" +
               rawHex.substring(4, 8) + "-" +
               rawHex.substring(8, 12) + "-" +
               rawHex.substring(12, 16);
    }

    std::atomic<bool> licenseValid { false };
    juce::String licensedEmail;
};

