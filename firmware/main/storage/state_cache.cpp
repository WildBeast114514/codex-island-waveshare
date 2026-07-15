#include "storage/state_cache.hpp"

#include <algorithm>
#include <cstddef>

#include "nvs.h"

namespace codex_island::storage {
namespace {

constexpr uint32_t kMagic = 0x43495831;  // CIX1
constexpr uint16_t kVersion = 1;

struct CacheBlob {
    uint32_t magic = kMagic;
    uint16_t version = kVersion;
    uint16_t reserved = 0;
    UsageState usage{};
    RadarState radar{};
    uint8_t page = 0;
    uint8_t brightness = 35;
    uint16_t padding = 0;
    uint32_t checksum = 0;
};

uint32_t checksum(const CacheBlob &blob) {
    const auto *bytes = reinterpret_cast<const uint8_t *>(&blob);
    uint32_t value = 2'166'136'261U;
    for (std::size_t index = 0; index < offsetof(CacheBlob, checksum); ++index) {
        value = (value ^ bytes[index]) * 16'777'619U;
    }
    return value;
}

}  // namespace

esp_err_t StateCache::begin() {
    nvs_handle_t handle = 0;
    const esp_err_t result = nvs_open("codex", NVS_READWRITE, &handle);
    if (result == ESP_OK) {
        nvs_close(handle);
        ready_ = true;
    }
    return result;
}

bool StateCache::load(AppState &state, uint8_t &page, uint8_t &brightness) {
    if (!ready_) {
        return false;
    }
    nvs_handle_t handle = 0;
    if (nvs_open("codex", NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    CacheBlob blob{};
    std::size_t size = sizeof(blob);
    const esp_err_t result = nvs_get_blob(handle, "state", &blob, &size);
    nvs_close(handle);
    if (result != ESP_OK || size != sizeof(blob) || blob.magic != kMagic ||
        blob.version != kVersion || blob.checksum != checksum(blob)) {
        return false;
    }
    state.usage = blob.usage;
    state.radar = blob.radar;
    state.link = {};
    state.power = {};
    page = std::min<uint8_t>(blob.page, 2);
    brightness = std::clamp<uint8_t>(blob.brightness, 5, 100);
    last_checksum_ = blob.checksum;
    return true;
}

esp_err_t StateCache::save_if_changed(const AppState &state, uint8_t page,
                                      uint8_t brightness) {
    if (!ready_) {
        return ESP_ERR_INVALID_STATE;
    }
    CacheBlob blob{};
    blob.usage = state.usage;
    blob.radar = state.radar;
    blob.page = std::min<uint8_t>(page, 2);
    blob.brightness = std::clamp<uint8_t>(brightness, 5, 100);
    blob.checksum = checksum(blob);
    if (blob.checksum == last_checksum_) {
        return ESP_OK;
    }
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open("codex", NVS_READWRITE, &handle);
    if (result == ESP_OK) {
        result = nvs_set_blob(handle, "state", &blob, sizeof(blob));
    }
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    if (handle != 0) {
        nvs_close(handle);
    }
    if (result == ESP_OK) {
        last_checksum_ = blob.checksum;
    }
    return result;
}

}  // namespace codex_island::storage
