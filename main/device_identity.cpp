#include "device_identity.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_matter_providers.h>
#include <mbedtls/sha256.h>

#include <crypto/CHIPCryptoPAL.h>
#include <lib/support/Span.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/CommissionableDataProvider.h>
#include <platform/ConfigurationManager.h>

#if !CONFIG_CUSTOM_COMMISSIONABLE_DATA_PROVIDER
#error "Enable CONFIG_CUSTOM_COMMISSIONABLE_DATA_PROVIDER in sdkconfig.defaults"
#endif

namespace {
constexpr char TAG[] = "identity";
constexpr char kContext[] = "DoNetIoT-rain-v1"; // Must match scripts/pairing_code.py
constexpr uint32_t kIterations = 1000;
constexpr size_t kSaltLength = 32;

struct Credentials {
    uint8_t mac[6];
    uint32_t passcode;
    uint16_t discriminator;
    uint8_t salt[kSaltLength];
};

void sha256(const uint8_t *data, size_t length, uint8_t out[32])
{
    mbedtls_sha256(data, length, out, 0);
}

// Passcodes the Matter specification forbids.
bool passcode_valid(uint32_t p)
{
    if (p < 1 || p > 99999998) return false;
    static constexpr uint32_t kInvalid[] = {11111111, 22222222, 33333333, 44444444, 55555555,
                                            66666666, 77777777, 88888888, 99999999, 12345678, 87654321};
    for (uint32_t bad : kInvalid) {
        if (p == bad) return false;
    }
    return true;
}

bool derive(Credentials &c)
{
    if (esp_efuse_mac_get_default(c.mac) != ESP_OK) return false;
    uint8_t input[sizeof(kContext) - 1 + sizeof(c.mac)];
    std::memcpy(input, kContext, sizeof(kContext) - 1);
    std::memcpy(input + sizeof(kContext) - 1, c.mac, sizeof(c.mac));
    uint8_t h[32];
    sha256(input, sizeof(input), h);
    for (;;) {
        c.passcode = ((uint32_t) h[0] << 24 | (uint32_t) h[1] << 16 | (uint32_t) h[2] << 8 | h[3]) % 99999998u + 1u;
        if (passcode_valid(c.passcode)) break;
        uint8_t next[32];
        sha256(h, sizeof(h), next);
        std::memcpy(h, next, sizeof(h));
    }
    c.discriminator = (uint16_t) ((((uint16_t) h[4] << 8) | h[5]) & 0x0FFF);
    uint8_t salt_input[4 + 32] = {'s', 'a', 'l', 't'};
    std::memcpy(salt_input + 4, h, sizeof(h));
    sha256(salt_input, sizeof(salt_input), c.salt);
    return true;
}

Credentials s_credentials{};

class IdentityProvider : public chip::DeviceLayer::CommissionableDataProvider {
public:
    CHIP_ERROR GetSetupDiscriminator(uint16_t &value) override
    {
        value = s_credentials.discriminator;
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR SetSetupDiscriminator(uint16_t) override { return CHIP_ERROR_NOT_IMPLEMENTED; }
    CHIP_ERROR GetSpake2pIterationCount(uint32_t &value) override
    {
        value = kIterations;
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetSpake2pSalt(chip::MutableByteSpan &salt) override
    {
        if (salt.size() < kSaltLength) return CHIP_ERROR_BUFFER_TOO_SMALL;
        std::memcpy(salt.data(), s_credentials.salt, kSaltLength);
        salt.reduce_size(kSaltLength);
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetSpake2pVerifier(chip::MutableByteSpan &verifier, size_t &length) override
    {
        // Computed on demand, only while commissioning; nothing is kept in RAM or flash.
        chip::Crypto::Spake2pVerifier generated;
        CHIP_ERROR err = generated.Generate(kIterations, chip::ByteSpan(s_credentials.salt, kSaltLength),
                                            s_credentials.passcode);
        if (err != CHIP_NO_ERROR) return err;
        err = generated.Serialize(verifier);
        if (err != CHIP_NO_ERROR) return err;
        length = verifier.size();
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetSetupPasscode(uint32_t &value) override
    {
        value = s_credentials.passcode;
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR SetSetupPasscode(uint32_t) override { return CHIP_ERROR_NOT_IMPLEMENTED; }
};

IdentityProvider s_provider;

void store_if_changed(const char *label, const char *wanted, CHIP_ERROR (chip::DeviceLayer::ConfigurationManager::*get)(char *, size_t),
                      CHIP_ERROR (chip::DeviceLayer::ConfigurationManager::*store)(const char *, size_t))
{
    auto &config = chip::DeviceLayer::ConfigurationMgr();
    char current[40] = {};
    if ((config.*get)(current, sizeof(current)) == CHIP_NO_ERROR && std::strcmp(current, wanted) == 0) {
        return;
    }
    const CHIP_ERROR err = (config.*store)(wanted, std::strlen(wanted));
    if (err != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Could not store %s: %" CHIP_ERROR_FORMAT, label, err.Format());
    }
}

void publish_names_locked(intptr_t)
{
    char serial[24];
    char unique[24];
    std::snprintf(serial, sizeof(serial), "DNIOT-%02X%02X%02X%02X%02X%02X", s_credentials.mac[0], s_credentials.mac[1],
                  s_credentials.mac[2], s_credentials.mac[3], s_credentials.mac[4], s_credentials.mac[5]);
    std::snprintf(unique, sizeof(unique), "dniot-%02x%02x%02x%02x%02x%02x", s_credentials.mac[0], s_credentials.mac[1],
                  s_credentials.mac[2], s_credentials.mac[3], s_credentials.mac[4], s_credentials.mac[5]);
    store_if_changed("serial number", serial, &chip::DeviceLayer::ConfigurationManager::GetSerialNumber,
                     &chip::DeviceLayer::ConfigurationManager::StoreSerialNumber);
    store_if_changed("unique ID", unique, &chip::DeviceLayer::ConfigurationManager::GetUniqueId,
                     &chip::DeviceLayer::ConfigurationManager::StoreUniqueId);
}
}

esp_err_t base::identity::install()
{
    if (!derive(s_credentials)) {
        return ESP_FAIL;
    }
    esp_matter::set_custom_commissionable_data_provider(&s_provider);
    ESP_LOGI(TAG, "Pairing: passcode=%08" PRIu32 " discriminator=%u (manual code: scripts/pairing_code.py)",
             s_credentials.passcode, (unsigned) s_credentials.discriminator);
    return ESP_OK;
}

esp_err_t base::identity::publish_names()
{
    const CHIP_ERROR err = chip::DeviceLayer::PlatformMgr().ScheduleWork(publish_names_locked, 0);
    return err == CHIP_NO_ERROR ? ESP_OK : ESP_FAIL;
}
