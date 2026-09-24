#include "oled_display.h"

#include <stdlib.h>
#include <string>
#include <algorithm>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>

#define TAG "OledDisplay"

// =================================================================
// PENGATURAN UTAMA UKURAN & POSISI MATA ROBOT (EMO STYLE)
// =================================================================
#define EYE_WIDTH       28   // Lebar mata terbuka (px)
#define EYE_HEIGHT      16   // Tinggi mata terbuka (px)
#define EYE_RADIUS       7   // Kelengkungan sudut mata (px)
#define EYE_OFFSET_X    22   // Jarak tiap mata dari titik tengah layar (px)
// =================================================================

OledDisplay::OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                         int width, int height, bool mirror_x, bool mirror_y)
    : panel_io_(panel_io), panel_(panel) {
    width_ = width;
    height_ = height;

    // Invert warna hardware agar background hitam murni & piksel mata biru menyala
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
                .full_refresh = 1, // Kunci kestabilan transmisi I2C/SPI OLED
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

    // Set background layar utama ke warna Hitam
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

    // Container Utama Animasi
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, width_, height_);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_remove_flag(container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(container_);

    // Membuat Objek Mata Kiri dan Mata Kanan
    left_eye_ = lv_obj_create(container_);
    right_eye_ = lv_obj_create(container_);

    lv_obj_remove_flag(left_eye_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(right_eye_, LV_OBJ_FLAG_SCROLLABLE);

    // Set warna mata ke Putih di LVGL (menjadi Biru Menyala di OLED)
    lv_obj_set_style_bg_color(left_eye_, lv_color_white(), 0);
    lv_obj_set_style_bg_color(right_eye_, lv_color_white(), 0);

    lv_obj_set_style_border_width(left_eye_, 0, 0);
    lv_obj_set_style_border_width(right_eye_, 0, 0);

    lv_obj_set_style_radius(left_eye_, EYE_RADIUS, 0);
    lv_obj_set_style_radius(right_eye_, EYE_RADIUS, 0);

    lv_obj_set_size(left_eye_, EYE_WIDTH, EYE_HEIGHT);
    lv_obj_set_size(right_eye_, EYE_WIDTH, EYE_HEIGHT);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -EYE_OFFSET_X, 0);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, EYE_OFFSET_X, 0);

    // Timer pembaharuan animasi (80 ms / ~12.5 FPS)
    timer_ = lv_timer_create(
        [](lv_timer_t* t) {
            auto disp = static_cast<OledDisplay*>(lv_timer_get_user_data(t));
            disp->Update();
        },
        80, this);

    setup_ui_called_ = true;
}

bool OledDisplay::Lock(int timeout_ms) { return lvgl_port_lock(timeout_ms); }

void OledDisplay::Unlock() { lvgl_port_unlock(); }

void OledDisplay::SetTheme(Theme* theme) {}

// Manual Set State
void OledDisplay::SetState(FaceState state) { 
    DisplayLockGuard lock(this);
    state_ = state; 
}

// Tangkap status sistem dari application.cc
void OledDisplay::SetStatus(const char* status) {
    if (status == nullptr) return;

    DisplayLockGuard lock(this);
    std::string st(status);
    std::transform(st.begin(), st.end(), st.begin(), ::tolower);

    ESP_LOGI(TAG, "SetStatus dipanggil: %s", status);

    // Deteksi kata kunci Listening
    if (st.find("listen") != std::string::npos || st.find("dengar") != std::string::npos || 
        st.find("think") != std::string::npos || st.find("pikir") != std::string::npos) {
        state_ = FaceState::Listening;
    } 
    // Deteksi kata kunci Speaking
    else if (st.find("speak") != std::string::npos || st.find("bicara") != std::string::npos || 
             st.find("jawab") != std::string::npos || st.find("say") != std::string::npos) {
        state_ = FaceState::Speaking;
    } 
    // Deteksi status Standby / Idle
    else if (st.find("standby") != std::string::npos || st.find("ready") != std::string::npos || 
             st.find("sleep") != std::string::npos || st.find("siap") != std::string::npos ||
             st.find("tunggu") != std::string::npos || st.find("idle") != std::string::npos) {
        state_ = FaceState::Idle;
    }
}

// Tangkap emosi dari server / application.cc
void OledDisplay::SetEmotion(const char* emotion) {
    if (emotion == nullptr) return;

    DisplayLockGuard lock(this);
    std::string em(emotion);
    std::transform(em.begin(), em.end(), em.begin(), ::tolower);

    ESP_LOGI(TAG, "SetEmotion dipanggil: %s", emotion);

    if (em.find("listen") != std::string::npos || em.find("think") != std::string::npos) {
        state_ = FaceState::Listening;
    } 
    else if (em.find("speak") != std::string::npos || em.find("talk") != std::string::npos) {
        state_ = FaceState::Speaking;
    } 
    else {
        // "neutral", "sleep", "idle", dll. Semua dikembalikan ke mode Idle (mata terpejam/tidur)
        state_ = FaceState::Idle;
    }
}

// Tangkap percakapan masuk dari application.cc
void OledDisplay::SetChatMessage(const char* role, const char* content) {
    if (role == nullptr) return;

    DisplayLockGuard lock(this);
    std::string r(role);

    if (r == "user") {
        state_ = FaceState::Listening;
    } 
    else if (r == "assistant") {
        state_ = FaceState::Speaking;
    }
}

// Mode IDLE: Mode Tidur Terpejam (- -) dengan Efek Bernapas Halus
void OledDisplay::IdleBehavior(int base_eye_height) {
    uint32_t now = lv_tick_get();

    // Efek bernapas: Tinggi mata berdenyut halus (3px - 4px) setiap ~1.5 detik
    int breath_cycle = (now / 750) % 2; 
    int sleep_eye_h = 3 + breath_cycle;
    int sleep_eye_w = EYE_WIDTH - 2;

    lv_obj_set_size(left_eye_, sleep_eye_w, sleep_eye_h);
    lv_obj_set_size(right_eye_, sleep_eye_w, sleep_eye_h);

    lv_obj_set_style_radius(left_eye_, 2, 0);
    lv_obj_set_style_radius(right_eye_, 2, 0);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -EYE_OFFSET_X, 2);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, EYE_OFFSET_X, 2);
}

// Mode LISTENING: Mata Bangun & Menyimak (Simetris)
void OledDisplay::ListeningBehavior(int base_eye_height) {
    lv_obj_set_size(left_eye_, EYE_WIDTH, base_eye_height);
    lv_obj_set_size(right_eye_, EYE_WIDTH, base_eye_height);

    lv_obj_set_style_radius(left_eye_, EYE_RADIUS, 0);
    lv_obj_set_style_radius(right_eye_, EYE_RADIUS, 0);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -EYE_OFFSET_X, 0);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, EYE_OFFSET_X, 0);
}

// Mode SPEAKING: Animasi Mata Memantul/Bicara (Squash & Stretch)
void OledDisplay::SpeakingBehavior(int eye_height) {
    uint32_t now = lv_tick_get();

    if (now - speak_last_update_ > 100) {
        speak_last_update_ = now;
        // Variasi tinggi mata saat AI berbicara
        speak_mouth_target_ = (EYE_HEIGHT - 6) + (rand() % 10);
    }

    int eye_h = speak_mouth_target_;
    if (eye_height < 8) eye_h = eye_height; // Jika sedang berkedip

    // Efek Squash & Stretch
    int eye_w = EYE_WIDTH + (EYE_HEIGHT - eye_h) / 2;

    lv_obj_set_size(left_eye_, eye_w, eye_h);
    lv_obj_set_size(right_eye_, eye_w, eye_h);

    lv_obj_set_style_radius(left_eye_, (eye_h < 8) ? 2 : EYE_RADIUS, 0);
    lv_obj_set_style_radius(right_eye_, (eye_h < 8) ? 2 : EYE_RADIUS, 0);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -EYE_OFFSET_X, 0);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, EYE_OFFSET_X, 0);
}

void OledDisplay::Update() {
    if (!container_) return;

    // Logika berkedip hanya aktif saat BUKAN mode Idle (Tidur)
    if (state_ != FaceState::Idle) {
        if (blink_phase_ == 0) {
            if (rand() % 80 == 0) {
                blink_phase_ = 1;
            }
        }
    } else {
        blink_phase_ = 0; // Kunci mata tetap terpejam saat tidur
    }

    int eye_height = EYE_HEIGHT;

    // Tahapan animasi berkedip
    switch (blink_phase_) {
        case 1:
            eye_height = EYE_HEIGHT / 2;
            blink_phase_ = 2;
            break;
        case 2:
            eye_height = 2; // Mata tertutup rapat saat kedip
            blink_phase_ = 3;
            break;
        case 3:
            eye_height = EYE_HEIGHT / 2;
            blink_phase_ = 0;
            break;
        default:
            break;
    }

    // Jalankan animasi sesuai status aktif
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
