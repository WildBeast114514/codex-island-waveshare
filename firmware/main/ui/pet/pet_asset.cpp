#include "ui/pet/pet_asset.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>

#include "esp_heap_caps.h"
#include "esp_log.h"

namespace codex_island::ui::pet {
namespace {

constexpr char kTag[] = "pet_asset";
constexpr uint8_t kMagic[] = {'C', 'P', 'E', 'T'};
constexpr uint16_t kPackVersion = 1;
constexpr uint32_t kHeaderSize = 72;
constexpr uint32_t kAnimationEntrySize = 8;
constexpr uint32_t kFrameEntrySize = 8;

uint16_t read_u16(const uint8_t *data) {
    return static_cast<uint16_t>(data[0]) |
           static_cast<uint16_t>(data[1] << 8U);
}

uint32_t read_u32(const uint8_t *data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8U) |
           (static_cast<uint32_t>(data[2]) << 16U) |
           (static_cast<uint32_t>(data[3]) << 24U);
}

}  // namespace

bool PetAsset::read_bounds(uint32_t offset, uint32_t length) const {
    return offset <= size_ && length <= size_ - offset;
}

bool PetAsset::begin(const uint8_t *data, std::size_t size) {
    if (data == nullptr || size < kHeaderSize ||
        std::memcmp(data, kMagic, sizeof(kMagic)) != 0 ||
        read_u16(data + 4) != kPackVersion ||
        read_u16(data + 6) != kHeaderSize) {
        ESP_LOGE(kTag, "invalid pet pack header");
        return false;
    }

    data_ = data;
    size_ = size;
    width_ = read_u16(data + 8);
    height_ = read_u16(data + 10);
    const uint16_t palette_count = read_u16(data + 12);
    const uint16_t animation_count = read_u16(data + 14);
    total_frames_ = read_u16(data + 16);
    const uint32_t animations_offset = read_u32(data + 20);
    frames_offset_ = read_u32(data + 24);
    const uint32_t palette_offset = read_u32(data + 28);
    const uint32_t data_offset = read_u32(data + 32);
    const uint32_t total_size = read_u32(data + 36);

    const uint64_t pixel_count =
        static_cast<uint64_t>(width_) * static_cast<uint64_t>(height_);
    if (width_ == 0 || height_ == 0 || width_ > 466 || height_ > 466 ||
        palette_count != palette_.size() ||
        animation_count == 0 ||
        animation_count >
            static_cast<uint16_t>(Animation::kCount) ||
        total_frames_ == 0 || total_size != size_ ||
        pixel_count >
            std::numeric_limits<uint32_t>::max() / sizeof(lv_color32_t) ||
        !read_bounds(animations_offset,
                     animation_count * kAnimationEntrySize) ||
        !read_bounds(frames_offset_, total_frames_ * kFrameEntrySize) ||
        !read_bounds(palette_offset, palette_.size() * 4U) ||
        data_offset > size_) {
        ESP_LOGE(kTag, "pet pack dimensions or offsets are invalid");
        return false;
    }

    std::memcpy(name_.data(), data + 40, 32);
    name_[32] = '\0';
    if (name_[0] == '\0') {
        std::snprintf(name_.data(), name_.size(), "Codex Pet");
    }

    for (uint16_t index = 0; index < animation_count; ++index) {
        const uint8_t *entry =
            data + animations_offset + index * kAnimationEntrySize;
        const uint8_t animation_id = entry[0];
        const uint16_t first_frame = read_u16(entry + 2);
        const uint16_t frame_count = read_u16(entry + 4);
        const uint16_t frame_ms = read_u16(entry + 6);
        if (animation_id >= static_cast<uint8_t>(Animation::kCount) ||
            frame_count == 0 || first_frame >= total_frames_ ||
            frame_count > total_frames_ - first_frame || frame_ms < 40) {
            ESP_LOGE(kTag, "pet animation %u is invalid",
                     static_cast<unsigned>(index));
            return false;
        }
        animations_[animation_id] =
            AnimationInfo{first_frame, frame_count, frame_ms};
    }
    if (!has_animation(Animation::kIdle)) {
        ESP_LOGE(kTag, "pet pack has no idle animation");
        return false;
    }

    for (std::size_t index = 0; index < palette_.size(); ++index) {
        const uint8_t *color = data + palette_offset + index * 4U;
        palette_[index] =
            lv_color32_make(color[0], color[1], color[2], color[3]);
    }

    const std::size_t buffer_size =
        static_cast<std::size_t>(pixel_count) * sizeof(lv_color32_t);
    pixels_ = static_cast<lv_color32_t *>(
        heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (pixels_ == nullptr) {
        ESP_LOGE(kTag, "cannot allocate %u-byte pet frame in PSRAM",
                 static_cast<unsigned>(buffer_size));
        return false;
    }
    std::fill_n(pixels_, static_cast<std::size_t>(pixel_count),
                lv_color32_make(0, 0, 0, 0));

    image_.header.magic = LV_IMAGE_HEADER_MAGIC;
    image_.header.cf = LV_COLOR_FORMAT_ARGB8888;
    image_.header.flags = 0;
    image_.header.w = width_;
    image_.header.h = height_;
    image_.header.stride = width_ * sizeof(lv_color32_t);
    image_.data_size = buffer_size;
    image_.data = reinterpret_cast<const uint8_t *>(pixels_);
    ESP_LOGI(kTag, "loaded %s: %ux%u, %u frames, buffer=%u",
             name_.data(), static_cast<unsigned>(width_),
             static_cast<unsigned>(height_),
             static_cast<unsigned>(total_frames_),
             static_cast<unsigned>(buffer_size));
    if (!decode(Animation::kIdle, 0)) {
        heap_caps_free(pixels_);
        pixels_ = nullptr;
        image_ = {};
        return false;
    }
    return true;
}

bool PetAsset::has_animation(Animation animation) const {
    const auto index = static_cast<std::size_t>(animation);
    return index < animations_.size() && animations_[index].frame_count > 0;
}

uint16_t PetAsset::frame_count(Animation animation) const {
    const auto index = static_cast<std::size_t>(animation);
    return index < animations_.size() ? animations_[index].frame_count : 0;
}

uint16_t PetAsset::frame_ms(Animation animation) const {
    const auto index = static_cast<std::size_t>(animation);
    return index < animations_.size() ? animations_[index].frame_ms : 150;
}

bool PetAsset::decode(Animation animation, uint16_t frame) {
    if (pixels_ == nullptr || !has_animation(animation)) {
        animation = Animation::kIdle;
    }
    const AnimationInfo &info =
        animations_[static_cast<std::size_t>(animation)];
    if (info.frame_count == 0) {
        return false;
    }
    const uint16_t absolute_frame =
        info.first_frame + (frame % info.frame_count);
    const uint8_t *frame_entry =
        data_ + frames_offset_ + absolute_frame * kFrameEntrySize;
    const uint32_t frame_offset = read_u32(frame_entry);
    const uint32_t frame_size = read_u32(frame_entry + 4);
    if (!read_bounds(frame_offset, frame_size)) {
        ESP_LOGE(kTag, "pet frame %u is out of bounds",
                 static_cast<unsigned>(absolute_frame));
        return false;
    }

    const uint8_t *input = data_ + frame_offset;
    const uint8_t *input_end = input + frame_size;
    lv_color32_t *output = pixels_;
    lv_color32_t *output_end =
        pixels_ + static_cast<std::size_t>(width_) * height_;
    while (input < input_end && output < output_end) {
        const uint8_t command = *input++;
        const std::size_t count = (command & 0x7FU) + 1U;
        if (count > static_cast<std::size_t>(output_end - output)) {
            return false;
        }
        if ((command & 0x80U) != 0U) {
            if (input >= input_end) {
                return false;
            }
            const lv_color32_t color = palette_[*input++];
            std::fill_n(output, count, color);
            output += count;
        } else {
            if (count > static_cast<std::size_t>(input_end - input)) {
                return false;
            }
            for (std::size_t index = 0; index < count; ++index) {
                *output++ = palette_[*input++];
            }
        }
    }
    return input == input_end && output == output_end;
}

}  // namespace codex_island::ui::pet
