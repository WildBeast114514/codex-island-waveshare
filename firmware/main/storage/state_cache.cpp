#include "storage/state_cache.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <new>

#include "nvs.h"

namespace codex_island::storage {
namespace {

constexpr uint32_t kMagic = 0x43495831;  // CIX1
constexpr uint16_t kVersion1 = 1;
constexpr uint16_t kVersion2 = 2;
constexpr uint16_t kVersion3 = 3;

struct CacheBlobV1 {
    uint32_t magic = kMagic;
    uint16_t version = kVersion1;
    uint16_t reserved = 0;
    UsageState usage{};
    RadarState radar{};
    uint8_t page = 0;
    uint8_t brightness = 35;
    uint16_t padding = 0;
    uint32_t checksum = 0;
};

struct CacheBlobV2 {
    uint32_t magic = kMagic;
    uint16_t version = kVersion2;
    uint16_t reserved = 0;
    UsageState usage{};
    RadarState radar{};
    PetState pet{};
    uint8_t page = 0;
    uint8_t brightness = 35;
    uint16_t padding = 0;
    uint32_t checksum = 0;
};

struct CacheBlobV3 {
    uint32_t magic = kMagic;
    uint16_t version = kVersion3;
    uint16_t reserved = 0;
    UsageState usage{};
    RadarState radar{};
    DistributedRadarState distributed_radar{};
    PetState pet{};
    uint8_t page = 0;
    uint8_t brightness = 35;
    uint16_t padding = 0;
    uint32_t checksum = 0;
};

template <typename Blob>
uint32_t checksum(const Blob &blob) {
    const auto *bytes = reinterpret_cast<const uint8_t *>(&blob);
    uint32_t value = 2'166'136'261U;
    for (std::size_t index = 0; index < offsetof(Blob, checksum); ++index) {
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
    std::size_t size = 0;
    if (nvs_get_blob(handle, "state", nullptr, &size) != ESP_OK) {
        nvs_close(handle);
        return false;
    }
    if (size == sizeof(CacheBlobV3)) {
        auto blob = std::unique_ptr<CacheBlobV3>(
            new (std::nothrow) CacheBlobV3{});
        if (blob == nullptr) {
            nvs_close(handle);
            return false;
        }
        const esp_err_t result = nvs_get_blob(handle, "state", blob.get(), &size);
        nvs_close(handle);
        if (result != ESP_OK || blob->magic != kMagic ||
            blob->version != kVersion3 || blob->checksum != checksum(*blob)) {
            return false;
        }
        state.usage = blob->usage;
        state.radar = blob->radar;
        state.distributed_radar = blob->distributed_radar;
        state.pet = blob->pet;
        page = std::min<uint8_t>(blob->page, 4);
        brightness = std::clamp<uint8_t>(blob->brightness, 5, 100);
        last_checksum_ = blob->checksum;
    } else if (size == sizeof(CacheBlobV2)) {
        auto blob = std::unique_ptr<CacheBlobV2>(
            new (std::nothrow) CacheBlobV2{});
        if (blob == nullptr) {
            nvs_close(handle);
            return false;
        }
        const esp_err_t result = nvs_get_blob(handle, "state", blob.get(), &size);
        nvs_close(handle);
        if (result != ESP_OK || blob->magic != kMagic ||
            blob->version != kVersion2 || blob->checksum != checksum(*blob)) {
            return false;
        }
        state.usage = blob->usage;
        state.radar = blob->radar;
        state.distributed_radar = {};
        state.pet = blob->pet;
        const uint8_t old_page = std::min<uint8_t>(blob->page, 3);
        page = old_page >= 2 ? static_cast<uint8_t>(old_page + 1) : old_page;
        brightness = std::clamp<uint8_t>(blob->brightness, 5, 100);
        last_checksum_ = 0;
    } else if (size == sizeof(CacheBlobV1)) {
        auto blob = std::unique_ptr<CacheBlobV1>(
            new (std::nothrow) CacheBlobV1{});
        if (blob == nullptr) {
            nvs_close(handle);
            return false;
        }
        const esp_err_t result = nvs_get_blob(handle, "state", blob.get(), &size);
        nvs_close(handle);
        if (result != ESP_OK || blob->magic != kMagic ||
            blob->version != kVersion1 || blob->checksum != checksum(*blob)) {
            return false;
        }
        state.usage = blob->usage;
        state.radar = blob->radar;
        state.distributed_radar = {};
        state.pet = {};
        const uint8_t old_page = std::min<uint8_t>(blob->page, 2);
        page = old_page >= 2 ? static_cast<uint8_t>(old_page + 1) : old_page;
        brightness = std::clamp<uint8_t>(blob->brightness, 5, 100);
        last_checksum_ = 0;
    } else {
        nvs_close(handle);
        return false;
    }
    state.link = {};
    state.power = {};
    return true;
}

esp_err_t StateCache::save_if_changed(const AppState &state, uint8_t page,
                                      uint8_t brightness) {
    if (!ready_) {
        return ESP_ERR_INVALID_STATE;
    }
    static CacheBlobV3 blob{};
    blob = {};
    blob.usage = state.usage;
    blob.radar = state.radar;
    blob.distributed_radar = state.distributed_radar;
    blob.pet = state.pet;
    blob.page = std::min<uint8_t>(page, 4);
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
