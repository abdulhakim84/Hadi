#include "oled_display.h"

#include <stdlib.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include "assets/lang_config.h"

#define TAG "OledDisplay"

#define EYE_OFFSET_X 5
#define EYE_OFFSET_Y 4

OledDisplay::OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                         int width, int height, bool mirror_x, bool mirror_y)
    : panel_io_(panel_io), panel_(panel) {
    width_ = width;
    height_ = height;

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.task_stack = 6144;
#if CONFIG_SOC_CPU_CORES_NUM > 1
    port_cfg.task_affinity = 1;
#endif
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding OLED display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * height_),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = true,
        .rotation =
            {
                .swap_xy = false,
                .mirror_x = mirror_x,
                .mirror_y = mirror_y,
            },
        .flags =
            {
                .buff_dma = 1,
                .buff_spiram = 0,
                .sw_rotate = 0,
                .full_refresh = 0,
                .direct_mode = 0,
            },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }
}

OledDisplay::~OledDisplay() {
    if (timer_ != nullptr) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }

    if (container_ != nullptr) {
        lv_obj_del(container_);
        container_ = nullptr;
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    lvgl_port_deinit();
}

void OledDisplay::SetupUI() {
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() called multiple times, skipping duplicate call");
        return;
    }

    LvglDisplay::SetupUI();  // Menggunakan parent class LvglDisplay

    DisplayLockGuard lock(this);
    auto screen = lv_screen_active();

    // Container Utama Animasi Wajah
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, width_, height_);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);
    lv_obj_center(container_);

    // Elemen Wajah
    left_eye_ = lv_obj_create(container_);
    right_eye_ = lv_obj_create(container_);
    mouth_ = lv_obj_create(container_);

    lv_obj_set_style_bg_color(left_eye_, lv_color_white(), 0);
    lv_obj_set_style_bg_color(right_eye_, lv_color_white(), 0);
    lv_obj_set_style_bg_color(mouth_, lv_color_white(), 0);

    lv_obj_set_style_border_width(left_eye_, 0, 0);
    lv_obj_set_style_border_width(right_eye_, 0, 0);
    lv_obj_set_style_border_width(mouth_, 0, 0);

    lv_obj_set_style_radius(left_eye_, 2, 0);
    lv_obj_set_style_radius(right_eye_, 2, 0);
    lv_obj_set_style_radius(mouth_, 4, 0);

    lv_obj_set_size(left_eye_, eye_size_, eye_size_);
    lv_obj_set_size(right_eye_, eye_size_, eye_size_);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_size_ - EYE_OFFSET_X, -EYE_OFFSET_Y);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_size_ + EYE_OFFSET_X, -EYE_OFFSET_Y);

    // Timer LVGL untuk Animasi
    timer_ = lv_timer_create(
        [](lv_timer_t* t) {
            auto disp = static_cast<OledDisplay*>(lv_timer_get_user_data(t));
            disp->Update();
        },
        60, this);
}

bool OledDisplay::Lock(int timeout_ms) { return lvgl_port_lock(timeout_ms); }

void OledDisplay::Unlock() { lvgl_port_unlock(); }

void OledDisplay::SetTheme(Theme* theme) {
    // Dikosongkan karena tidak lagi menggunakan tema font/teks
}

void OledDisplay::SetState(FaceState state) { 
    state_ = state; 
}

void OledDisplay::SetEmotion(const char* emotion) {
    if (emotion == nullptr) return;

    std::string em(emotion);
    if (em == "listening" || em == "think") {
        SetState(FaceState::Listening);
    } else if (em == "speaking" || em == "talk") {
        SetState(FaceState::Speaking);
    } else {
        SetState(FaceState::Idle);
    }
}

void OledDisplay::SetChatMessage(const char* role, const char* content) {
    // Dikosongkan karena elemen teks pesan sudah dihapus
}

void OledDisplay::IdleBehavior(int base_eye_height) {
    if (rand() % 40 == 0) {
        idle_move_offset_x_ = (rand() % 5) - 2;
        idle_move_offset_y_ = (rand() % 3) - 1;
    }

    int eye_h = base_eye_height;
    int eye_w = eye_h * 0.65;

    lv_obj_set_size(left_eye_, eye_w, eye_h);
    lv_obj_set_size(right_eye_, eye_w, eye_h);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_size_ + idle_move_offset_x_,
                 -5 + idle_move_offset_y_);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_size_ + idle_move_offset_x_,
                 -5 + idle_move_offset_y_);

    lv_obj_set_size(mouth_, 20, 7);
    lv_obj_set_style_radius(mouth_, 4, 0);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, idle_move_offset_x_, 17 + idle_move_offset_y_);
}

void OledDisplay::ListeningBehavior(int base_eye_height) {
    int right_h = base_eye_height;
    int right_w = right_h * 0.65;

    int left_h = base_eye_height - 2;
    int left_w = left_h * 0.65;

    lv_obj_set_size(left_eye_, left_w, left_h);
    lv_obj_set_size(right_eye_, right_w, right_h);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_size_ - EYE_OFFSET_X, -EYE_OFFSET_Y);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_size_ + EYE_OFFSET_X, -EYE_OFFSET_Y);

    lv_obj_set_size(mouth_, 10, 2);
    lv_obj_set_style_radius(mouth_, 4, 0);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, -4, 17);
}

void OledDisplay::SpeakingBehavior(int eye_height) {
    uint32_t now = lv_tick_get();

    lv_obj_set_size(left_eye_, eye_height * 0.65, eye_height);
    lv_obj_set_size(right_eye_, eye_height * 0.65, eye_height);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_size_ - EYE_OFFSET_X, -EYE_OFFSET_Y);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_size_ + EYE_OFFSET_X, -EYE_OFFSET_Y);

    if (now - speak_last_update_ > 90 + (rand() % 60)) {
        speak_last_update_ = now;

        int r = rand() % 100;

        if (r < 20)
            speak_mouth_target_ = 2;
        else if (r < 50)
            speak_mouth_target_ = 6;
        else if (r < 80)
            speak_mouth_target_ = 10;
        else
            speak_mouth_target_ = 16;
    }

    if (speak_mouth_current_ < speak_mouth_target_)
        speak_mouth_current_ += 2;
    else if (speak_mouth_current_ > speak_mouth_target_)
        speak_mouth_current_ -= 2;

    lv_obj_set_size(mouth_, 15, speak_mouth_current_);
    lv_obj_set_style_radius(mouth_, 8, 0);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, 18);
}

void OledDisplay::Update() {
    if (!container_)
        return;

    if (blink_phase_ == 0) {
        if (rand() % 120 == 0) {
            blink_phase_ = 1;
        }
    }

    int eye_height = eye_size_;

    switch (blink_phase_) {
        case 1:
            eye_height = 4;
            blink_phase_ = 2;
            break;
        case 2:
            eye_height = 1;
            blink_phase_ = 3;
            break;
        case 3:
            eye_height = 6;
            blink_phase_ = 0;
            break;
        default:
            break;
    }

    switch (state_) {
        case FaceState::Idle:
            IdleBehavior(eye_height);
            break;
        case FaceState::Listening:
            ListeningBehavior(eye_height);
            break;
        case FaceState::Speaking:
            SpeakingBehavior(eye_height);
            break;
    }
}
