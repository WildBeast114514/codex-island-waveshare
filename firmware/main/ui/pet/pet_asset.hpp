#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "lvgl.h"

namespace codex_island::ui::pet {

enum class Animation : uint8_t {
    kIdle = 0,
    kRunRight = 1,
    kRunLeft = 2,
    kWaving = 3,
    kJumping = 4,
    kFailed = 5,
    kWaiting = 6,
    kRunning = 7,
    kReview = 8,
    kCount = 9,
};

class PetAsset {
public:
    bool begin(const uint8_t *data, std::size_t size);
    bool decode(Animation animation, uint16_t frame);
    bool has_animation(Animation animation) const;
    uint16_t frame_count(Animation animation) const;
    uint16_t frame_ms(Animation animation) const;
    const lv_image_dsc_t *image() const { return &image_; }
    const char *name() const { return name_.data(); }
    uint16_t width() const { return width_; }
    uint16_t height() const { return height_; }

private:
    struct AnimationInfo {
        uint16_t first_frame = 0;
        uint16_t frame_count = 0;
        uint16_t frame_ms = 150;
    };

    bool read_bounds(uint32_t offset, uint32_t length) const;

    const uint8_t *data_ = nullptr;
    std::size_t size_ = 0;
    uint32_t frames_offset_ = 0;
    uint16_t total_frames_ = 0;
    uint16_t width_ = 0;
    uint16_t height_ = 0;
    std::array<char, 33> name_{};
    std::array<AnimationInfo, static_cast<std::size_t>(Animation::kCount)>
        animations_{};
    std::array<lv_color32_t, 256> palette_{};
    lv_color32_t *pixels_ = nullptr;
    lv_image_dsc_t image_{};
};

}  // namespace codex_island::ui::pet
