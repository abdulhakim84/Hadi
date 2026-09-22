#include "oled_display.h"

#include <string>
#include <cstring>

#include <esp_err.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include "assets/lang_config.h"

#include "face_engine.h"

#define TAG "OledDisplay"

// FaceEngine global
static FaceEngine* face_engine_ = nullptr;


// ============================================================
// Constructor
// ============================================================

OledDisplay::OledDisplay(
    esp_lcd_panel_io_handle_t panel_io,
    esp_lcd_panel_handle_t panel,
    int width,
    int height,
    bool mirror_x,
    bool mirror_y
) : LvglDisplay(),
    panel_io_(panel_io),
    panel_(panel) {

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();

    port_cfg.task_priority = 1;
    port_cfg.task_stack = 6144;

#if CONFIG_SOC_CPU_CORES_NUM > 1
    port_cfg.task_affinity = 1;
#endif

    lvgl_port_init(&port_cfg);

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

        .rotation = {
            .swap_xy = false,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },

        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
}


// ============================================================
// Destructor
// ============================================================

OledDisplay::~OledDisplay() {
    face_engine_ = nullptr;
}


// ============================================================
// Lock / Unlock
// ============================================================

bool OledDisplay::Lock(int timeout_ms) {
    return LvglDisplay::Lock(timeout_ms);
}

void OledDisplay::Unlock() {
    LvglDisplay::Unlock();
}


// ============================================================
// Setup UI
// ============================================================

void OledDisplay::SetupUI() {
    Display::SetupUI();

    if (width_ == 128 && height_ == 64) {
        SetupUI_128x64();
    }
}


// ============================================================
// Setup OLED 128x64
// ============================================================

void OledDisplay::SetupUI_128x64() {

    DisplayLockGuard lock(this);

    lv_obj_t* screen = lv_screen_active();

    // --------------------------------------------------------
    // Bersihkan seluruh tampilan
    // --------------------------------------------------------

    lv_obj_clean(screen);

    // Background hitam
    lv_obj_set_style_bg_color(
        screen,
        lv_color_black(),
        0
    );

    lv_obj_set_style_bg_opa(
        screen,
        LV_OPA_COVER,
        0
    );

    // --------------------------------------------------------
    // Buat container langsung memenuhi OLED
    // --------------------------------------------------------

    container_ = lv_obj_create(screen);

    lv_obj_set_size(
        container_,
        128,
        64
    );

    lv_obj_center(container_);

    lv_obj_set_style_bg_color(
        container_,
        lv_color_black(),
        0
    );

    lv_obj_set_style_bg_opa(
        container_,
        LV_OPA_COVER,
        0
    );

    lv_obj_set_style_border_width(
        container_,
        0,
        0
    );

    lv_obj_set_style_pad_all(
        container_,
        0,
        0
    );

    lv_obj_clear_flag(
        container_,
        LV_OBJ_FLAG_SCROLLABLE
    );

    // --------------------------------------------------------
    // FaceEngine
    // --------------------------------------------------------

    face_engine_ = new FaceEngine();

    face_engine_->Init(container_);

    // Mulai dari Idle
    face_engine_->SetState(FaceState::Idle);

    ESP_LOGI(TAG, "FaceEngine initialized");
}


// ============================================================
// Status Xiaozhi
// ============================================================

void OledDisplay::SetStatus(const char* status) {

    if (face_engine_ == nullptr) {
        return;
    }

    if (status == nullptr) {
        return;
    }

    if (strcmp(status, Lang::Strings::STANDBY) == 0) {

        face_engine_->SetState(
            FaceState::Idle
        );

    } else if (strcmp(status, Lang::Strings::LISTENING) == 0) {

        face_engine_->SetState(
            FaceState::Listening
        );

    } else if (strcmp(status, Lang::Strings::SPEAKING) == 0) {

        face_engine_->SetState(
            FaceState::Speaking
        );

    } else {

        face_engine_->SetState(
            FaceState::Idle
        );
    }
}


// ============================================================
// Chat Message
//
// Xiaozhi masih boleh memanggil fungsi ini,
// tetapi kita sengaja tidak menampilkan teks.
// ============================================================

void OledDisplay::SetChatMessage(
    const char* role,
    const char* content
) {
    // Tidak melakukan apa-apa.
}


// ============================================================
// Emotion
//
// Untuk sekarang tidak digunakan oleh FaceEngine.
// ============================================================

void OledDisplay::SetEmotion(
    const char* emotion
) {
    // Tidak melakukan apa-apa.
}


// ============================================================
// Theme
//
// Wajah menggunakan warna sendiri.
// ============================================================

void OledDisplay::SetTheme(
    Theme* theme
) {
    // Tidak melakukan apa-apa.
}
