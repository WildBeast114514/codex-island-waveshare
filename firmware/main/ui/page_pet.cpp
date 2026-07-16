#include "ui/page_pet.hpp"

#include "esp_log.h"
#include "ui/ui_theme.hpp"

extern const uint8_t pet_asset_start[]
    asm("_binary_codex_pet_asset_start");
extern const uint8_t pet_asset_end[]
    asm("_binary_codex_pet_asset_end");

namespace codex_island::ui {
namespace {

constexpr char kTag[] = "pet_page";

pet::Animation activity_animation(PetActivity activity) {
    switch (activity) {
        case PetActivity::kRunning:
            return pet::Animation::kRunning;
        case PetActivity::kWaiting:
            return pet::Animation::kWaiting;
        case PetActivity::kReview:
            return pet::Animation::kReview;
        case PetActivity::kFailed:
            return pet::Animation::kFailed;
        case PetActivity::kIdle:
        default:
            return pet::Animation::kIdle;
    }
}

const char *activity_label(PetActivity activity) {
    switch (activity) {
        case PetActivity::kRunning:
            return "WORKING";
        case PetActivity::kWaiting:
            return "NEEDS INPUT";
        case PetActivity::kReview:
            return "READY";
        case PetActivity::kFailed:
            return "BLOCKED";
        case PetActivity::kIdle:
        default:
            return "IDLE";
    }
}

uint32_t activity_color(PetActivity activity) {
    switch (activity) {
        case PetActivity::kRunning:
            return kGreen;
        case PetActivity::kWaiting:
            return kOrange;
        case PetActivity::kReview:
            return kCyan;
        case PetActivity::kFailed:
            return 0xFF4E72;
        case PetActivity::kIdle:
        default:
            return kPurple;
    }
}

}  // namespace

void PetPage::create(lv_obj_t *parent) {
    style_black_surface(parent);
    const std::size_t asset_size =
        static_cast<std::size_t>(pet_asset_end - pet_asset_start);
    ready_ = asset_.begin(pet_asset_start, asset_size);
    if (!ready_) {
        lv_obj_t *error =
            make_label(parent, "PET ASSET ERROR", &lv_font_montserrat_20, kOrange);
        lv_obj_align(error, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    image_ = lv_image_create(parent);
    lv_image_set_src(image_, asset_.image());
    lv_obj_set_size(image_, asset_.width(), asset_.height());
    lv_obj_align(image_, LV_ALIGN_CENTER, 0, 0);

    status_pill_ = lv_obj_create(parent);
    lv_obj_remove_style_all(status_pill_);
    lv_obj_set_size(status_pill_, 226, 38);
    lv_obj_align(status_pill_, LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_set_style_bg_color(status_pill_, lv_color_hex(kBlack), 0);
    lv_obj_set_style_bg_opa(status_pill_, LV_OPA_80, 0);
    lv_obj_set_style_border_width(status_pill_, 2, 0);
    lv_obj_set_style_border_color(status_pill_, lv_color_hex(kPurple), 0);
    lv_obj_set_style_radius(status_pill_, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(status_pill_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(status_pill_, LV_OBJ_FLAG_CLICKABLE);

    status_label_ =
        make_label(status_pill_, "", &lv_font_montserrat_16, kWhite);
    lv_obj_center(status_label_);
    lv_label_set_text_fmt(status_label_, "%s  |  IDLE", asset_.name());

    timer_ = lv_timer_create(animation_timer, asset_.frame_ms(animation_), this);
    lv_timer_pause(timer_);
    ESP_LOGI(kTag, "pet page ready: %s", asset_.name());
}

void PetPage::set_active(bool active) {
    active_ = active;
    if (timer_ == nullptr) {
        return;
    }
    if (active) {
        lv_timer_resume(timer_);
        lv_timer_ready(timer_);
    } else {
        lv_timer_pause(timer_);
    }
}

void PetPage::select_animation(pet::Animation animation) {
    if (!asset_.has_animation(animation)) {
        animation = pet::Animation::kIdle;
    }
    if (animation_ == animation) {
        return;
    }
    animation_ = animation;
    frame_ = 0;
    lv_timer_set_period(timer_, asset_.frame_ms(animation_));
    if (asset_.decode(animation_, frame_)) {
        lv_obj_invalidate(image_);
    }
}

void PetPage::update(const AppState &state) {
    if (!ready_) {
        return;
    }
    const bool online = state.link.ble_connected && state.pet.valid;
    const PetActivity activity =
        online ? state.pet.activity : PetActivity::kIdle;
    select_animation(activity_animation(activity));

    const uint32_t color = online ? activity_color(activity) : kMuted;
    lv_obj_set_style_border_color(status_pill_, lv_color_hex(color), 0);
    lv_obj_set_style_text_color(status_label_, lv_color_hex(color), 0);
    if (!online) {
        lv_label_set_text_fmt(status_label_, "%s  |  OFFLINE", asset_.name());
    } else if (state.pet.active_tasks > 1 &&
               activity == PetActivity::kRunning) {
        lv_label_set_text_fmt(status_label_, "%s  |  %u TASKS", asset_.name(),
                              static_cast<unsigned>(state.pet.active_tasks));
    } else {
        lv_label_set_text_fmt(status_label_, "%s  |  %s", asset_.name(),
                              activity_label(activity));
    }
}

void PetPage::animation_timer(lv_timer_t *timer) {
    auto *page = static_cast<PetPage *>(lv_timer_get_user_data(timer));
    if (page != nullptr) {
        page->advance();
    }
}

void PetPage::advance() {
    if (!active_ || !ready_) {
        return;
    }
    const uint16_t count = asset_.frame_count(animation_);
    if (count == 0) {
        return;
    }
    frame_ = static_cast<uint16_t>((frame_ + 1) % count);
    if (asset_.decode(animation_, frame_)) {
        lv_obj_invalidate(image_);
    } else {
        ESP_LOGE(kTag, "failed to decode animation frame");
    }
}

}  // namespace codex_island::ui
