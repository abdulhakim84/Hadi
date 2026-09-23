#include "oled_display.h"

#include <stdlib.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>

#define TAG "OledDisplay"

OledDisplay::OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                         int width, int height, bool mirror_x, bool mirror_y)
    : panel_io_(panel_io), panel_(panel) {
    width_ = width;
    height_ = height;

    // Balikkan warna jika background masih putih
    esp_lcd_panel_invert_color(panel_, true); 

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.task_stack = 8192;
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
                .full_refresh = 1, // Kunci kestabilan I2C OLED
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
        ESP_LOGW(TAG, "SetupUI() dipanggil ulang, dilewati");
        return;
    }

    LvglDisplay::SetupUI();

    DisplayLockGuard lock(this);
    auto screen = lv_screen_active();

    // Set background layar menjadi hitam
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

    // Container Utama
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, width_, height_);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_remove_flag(container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(container_);

    // Buat hanya 2 Mata (Tanpa Mulut)
    left_eye_ = lv_obj_create(container_);
    right_eye_ = lv_obj_create(container_);

    lv_obj_remove_flag(left_eye_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(right_eye_, LV_OBJ_FLAG_SCROLLABLE);

    // Warna Mata Menyala (Putih di LVGL = Biru Terang di OLED Biru)
    lv_obj_set_style_bg_color(left_eye_, lv_color_white(), 0);
    lv_obj_set_style_bg_color(right_eye_, lv_color_white(), 0);

    lv_obj_set_style_border_width(left_eye_, 0, 0);
    lv_obj_set_style_border_width(right_eye_, 0, 0);

    // Ciri khas EMO: Lengkungan sudut (Radius 10)
    lv_obj_set_style_radius(left_eye_, 10, 0);
    lv_obj_set_style_radius(right_eye_, 10, 0);

    // Ukuran awal mata besar EMO (Lebar 36px, Tinggi 32px)
    lv_obj_set_size(left_eye_, 36, 32);
    lv_obj_set_size(right_eye_, 36, 32);

    // Posisi Mata Kiri (-22px) dan Kanan (+22px) dari Tengah
    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -22, 0);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, 22, 0);

    // Timer animasi 80ms
    timer_ = lv_timer_create(
        [](lv_timer_t* t) {
            auto disp = static_cast<OledDisplay*>(lv_timer_get_user_data(t));
            disp->Update();
        },
        80, this);
}

bool OledDisplay::Lock(int timeout_ms) { return lvgl_port_lock(timeout_ms); }

void OledDisplay::Unlock() { lvgl_port_unlock(); }

void OledDisplay::SetTheme(Theme* theme) {}

void OledDisplay::SetState(FaceState state) { 
    DisplayLockGuard lock(this);
    state_ = state; 
}

void OledDisplay::SetEmotion(const char* emotion) {
    if (emotion == nullptr) return;

    DisplayLockGuard lock(this);
    std::string em(emotion);
    if (em == "listening" || em == "think") {
        state_ = FaceState::Listening;
    } else if (em == "speaking" || em == "talk") {
        state_ = FaceState::Speaking;
    } else {
        state_ = FaceState::Idle;
    }
}

void OledDisplay::SetChatMessage(const char* role, const char* content) {}

// Mode Diam / Standby (EMO Style)
void OledDisplay::IdleBehavior(int base_eye_height) {
    // Lirik mata acak sesekali
    if (rand() % 50 == 0) {
        idle_move_offset_x_ = (rand() % 7) - 3;
        idle_move_offset_y_ = (rand() % 5) - 2;
    }

    int eye_h = base_eye_height; // Normal = 32px
    int eye_w = (eye_h < 10) ? 38 : 36; // Saat kedip, lebar agak meluas sedikit

    lv_obj_set_size(left_eye_, eye_w, eye_h);
    lv_obj_set_size(right_eye_, eye_w, eye_h);

    // Sudut tetap membulat
    lv_obj_set_style_radius(left_eye_, (eye_h < 10) ? 2 : 10, 0);
    lv_obj_set_style_radius(right_eye_, (eye_h < 10) ? 2 : 10, 0);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -22 + idle_move_offset_x_, idle_move_offset_y_);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, 22 + idle_move_offset_x_, idle_move_offset_y_);
}

// Mode Mendengarkan / Menyimak (Mata agak miring / penasaran)
void OledDisplay::ListeningBehavior(int base_eye_height) {
    // Mata kanan sedikit lebih besar dari mata kiri (efek EMO heran/menyimak)
    int left_h = base_eye_height - 4;
    int right_h = base_eye_height + 2;

    lv_obj_set_size(left_eye_, 32, left_h);
    lv_obj_set_size(right_eye_, 38, right_h);

    lv_obj_set_style_radius(left_eye_, 8, 0);
    lv_obj_set_style_radius(right_eye_, 10, 0);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -22, -2);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, 22, 2);
}

// Mode Bicara (Karakter Mata Memantul/Bicara menggantikan Mulut)
void OledDisplay::SpeakingBehavior(int eye_height) {
    uint32_t now = lv_tick_get();

    if (now - speak_last_update_ > 100) {
        speak_last_update_ = now;
        // Variasi tinggi mata secara acak saat bicara (24px sampai 38px)
        speak_mouth_target_ = 24 + (rand() % 15);
    }

    int eye_h = speak_mouth_target_;
    if (eye_height < 10) eye_h = eye_height; // Jika sedang kedip

    // Efek squash & stretch: jika tinggi mengecil, lebar melebar
    int eye_w = 36 + (32 - eye_h) / 2;

    lv_obj_set_size(left_eye_, eye_w, eye_h);
    lv_obj_set_size(right_eye_, eye_w, eye_h);

    lv_obj_set_style_radius(left_eye_, (eye_h < 10) ? 2 : 10, 0);
    lv_obj_set_style_radius(right_eye_, (eye_h < 10) ? 2 : 10, 0);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -22, 0);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, 22, 0);
}

void OledDisplay::Update() {
    if (!container_) return;

    // Logika Berkedip
    if (blink_phase_ == 0) {
        if (rand() % 80 == 0) {
            blink_phase_ = 1;
        }
    }

    int eye_height = 32; // Tinggi standar mata EMO

    switch (blink_phase_) {
        case 1:
            eye_height = 16;
            blink_phase_ = 2;
            break;
        case 2:
            eye_height = 3;  // Garis tipis saat kedip penuh
            blink_phase_ = 3;
            break;
        case 3:
            eye_height = 16;
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
