#pragma once

#include <cstdint>

#include "lvgl.h"
#include "model/app_state.hpp"
#include "ui/pet/pet_asset.hpp"

namespace codex_island::ui {

class PetPage {
public:
    void create(lv_obj_t *parent);
    void update(const AppState &state);
    void set_active(bool active);

private:
    static void animation_timer(lv_timer_t *timer);
    void advance();
    void select_animation(pet::Animation animation);

    pet::PetAsset asset_{};
    lv_obj_t *image_ = nullptr;
    lv_obj_t *status_pill_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_timer_t *timer_ = nullptr;
    pet::Animation animation_ = pet::Animation::kIdle;
    uint16_t frame_ = 0;
    bool active_ = false;
    bool ready_ = false;
};

}  // namespace codex_island::ui
