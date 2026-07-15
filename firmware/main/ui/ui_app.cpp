#include "ui/ui_app.hpp"

#include <algorithm>

#include "ui/ui_theme.hpp"

namespace codex_island::ui {

void UiApp::begin(AppStateStore *store) {
    store_ = store;
    lv_obj_t *screen = lv_screen_active();
    style_black_surface(screen);

    tileview_ = lv_tileview_create(screen);
    lv_obj_set_size(tileview_, 466, 466);
    lv_obj_set_pos(tileview_, 0, 0);
    style_black_surface(tileview_);
    lv_obj_set_scrollbar_mode(tileview_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(tileview_, tile_event, LV_EVENT_VALUE_CHANGED, this);

    tiles_[0] = lv_tileview_add_tile(tileview_, 0, 0, LV_DIR_RIGHT);
    tiles_[1] = lv_tileview_add_tile(tileview_, 1, 0, static_cast<lv_dir_t>(LV_DIR_LEFT | LV_DIR_RIGHT));
    tiles_[2] = lv_tileview_add_tile(tileview_, 2, 0, LV_DIR_LEFT);
    for (lv_obj_t *tile : tiles_) {
        style_black_surface(tile);
        lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_OFF);
    }

    usage_.create(tiles_[0]);
    radar_.create(tiles_[1]);
    status_.create(tiles_[2]);
    for (uint8_t page = 0; page < 3; ++page) {
        create_dots(tiles_[page], page);
    }
    set_page(0, false);
    refresh(0);
}

void UiApp::refresh(int64_t monotonic_seconds) {
    if (store_ == nullptr) {
        return;
    }
    const AppState state = store_->snapshot();
    usage_.update(state);
    radar_.update(state);
    status_.update(state, monotonic_seconds);
}

void UiApp::tick(int64_t monotonic_seconds) {
    if (store_ == nullptr) {
        return;
    }
    store_->update([](AppState &state) {
        if (state.usage.valid && state.usage.reset_seconds > 0) {
            --state.usage.reset_seconds;
        }
    });
    const AppState state = store_->snapshot();
    usage_.update(state);
    status_.update(state, monotonic_seconds);
}

void UiApp::set_page(uint8_t page, bool animate) {
    current_page_ = std::min<uint8_t>(page, 2);
    lv_tileview_set_tile_by_index(tileview_, current_page_, 0,
                                  animate ? LV_ANIM_ON : LV_ANIM_OFF);
}

void UiApp::next_page() {
    set_page(static_cast<uint8_t>((current_page_ + 1) % 3));
}

void UiApp::dot_event(lv_event_t *event) {
    auto *context = static_cast<DotContext *>(lv_event_get_user_data(event));
    if (context != nullptr && context->app != nullptr) {
        context->app->set_page(context->page);
    }
}

void UiApp::tile_event(lv_event_t *event) {
    auto *app = static_cast<UiApp *>(lv_event_get_user_data(event));
    if (app == nullptr) {
        return;
    }
    lv_obj_t *active = lv_tileview_get_tile_active(app->tileview_);
    for (uint8_t i = 0; i < app->tiles_.size(); ++i) {
        if (active == app->tiles_[i]) {
            app->current_page_ = i;
            break;
        }
    }
}

void UiApp::create_dots(lv_obj_t *parent, uint8_t active) {
    for (uint8_t page = 0; page < 3; ++page) {
        const int diameter = page == active ? 10 : 9;
        const int x = 208 + page * 20;
        lv_obj_t *dot = make_dot(parent, x, 445, diameter,
                                 page == active ? kWhite : 0x4B4F54);
        lv_obj_add_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        DotContext &context = dot_contexts_[next_dot_context_++];
        context = DotContext{this, page};
        lv_obj_add_event_cb(dot, dot_event, LV_EVENT_CLICKED, &context);
    }
}

}  // namespace codex_island::ui
