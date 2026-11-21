#include "oled_display.h"

#include <stdlib.h>
#include <string.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>

#define TAG "OledDisplay"

// =================================================================
// PENGATURAN UKURAN DAN POSISI MATA & MULUT
// =================================================================
#define EYE_WIDTH       28   // Lebar mata
#define EYE_HEIGHT      16   // Tinggi mata
#define EYE_RADIUS       7   // Kehalusan sudut mata (kotak membulat)
#define EYE_OFFSET_X    24   // Jarak mata dari titik tengah layar
#define BROW_Y         -13   // Posisi alis dari titik tengah layar (marah/sedih)
// =================================================================

// Titik garis alis (relatif terhadap objek alis itu sendiri)
static lv_point_precise_t kBrowDown[] = {{0, 0}, {22, 8}};  // turun ke kanan
static lv_point_precise_t kBrowUp[]   = {{0, 8}, {22, 0}};  // naik ke kanan

OledDisplay::OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                         int width, int height, bool mirror_x, bool mirror_y)
    : panel_io_(panel_io), panel_(panel) {
    width_ = width;
    height_ = height;

    // Invert warna agar background hitam & mata menyala
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
                .full_refresh = 1,
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

    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, width_, height_);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_remove_flag(container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(container_);

    left_eye_ = lv_obj_create(container_);
    right_eye_ = lv_obj_create(container_);
    mouth_ = lv_obj_create(container_);

    lv_obj_remove_flag(left_eye_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(right_eye_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(mouth_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(left_eye_, lv_color_white(), 0);
    lv_obj_set_style_bg_color(right_eye_, lv_color_white(), 0);
    lv_obj_set_style_bg_color(mouth_, lv_color_white(), 0);

    lv_obj_set_style_border_width(left_eye_, 0, 0);
    lv_obj_set_style_border_width(right_eye_, 0, 0);
    lv_obj_set_style_border_width(mouth_, 0, 0);

    lv_obj_set_style_radius(left_eye_, EYE_RADIUS, 0);
    lv_obj_set_style_radius(right_eye_, EYE_RADIUS, 0);
    lv_obj_set_style_radius(mouth_, 2, 0);

    lv_obj_set_size(left_eye_, EYE_WIDTH, EYE_HEIGHT);
    lv_obj_set_size(right_eye_, EYE_WIDTH, EYE_HEIGHT);
    lv_obj_set_size(mouth_, 12, 3);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -EYE_OFFSET_X, -2);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, EYE_OFFSET_X, -2);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, 12);

    // --- Objek tambahan untuk emosi ---
    // Alis (dipakai untuk marah & sedih)
    left_brow_ = lv_line_create(container_);
    right_brow_ = lv_line_create(container_);
    for (lv_obj_t* b : {left_brow_, right_brow_}) {
        lv_obj_set_style_line_width(b, 3, 0);
        lv_obj_set_style_line_color(b, lv_color_white(), 0);
        lv_obj_set_style_line_rounded(b, true, 0);
        lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
    }

    // Mulut berbentuk busur (senyum & cemberut)
    mouth_arc_ = lv_arc_create(container_);
    lv_obj_set_size(mouth_arc_, 24, 24);
    lv_obj_remove_style(mouth_arc_, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(mouth_arc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(mouth_arc_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_arc_width(mouth_arc_, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_color(mouth_arc_, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(mouth_arc_, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);

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

// Ubah nama emosi (Inggris/Indonesia) menjadi enum. Return false jika tidak dikenal.
static bool ParseEmotion(const char* name, Emotion& out) {
    if (name == nullptr) return false;
    if (!strcmp(name, "happy") || !strcmp(name, "laughing") ||
        !strcmp(name, "senang") || !strcmp(name, "bahagia")) {
        out = Emotion::Happy;
    } else if (!strcmp(name, "sad") || !strcmp(name, "crying") ||
               !strcmp(name, "sedih")) {
        out = Emotion::Sad;
    } else if (!strcmp(name, "angry") || !strcmp(name, "marah")) {
        out = Emotion::Angry;
    } else if (!strcmp(name, "neutral") || !strcmp(name, "normal") ||
               !strcmp(name, "biasa")) {
        out = Emotion::Neutral;
    } else {
        return false;
    }
    return true;
}

// Dipanggil otomatis oleh Application (emosi dari server & reset state).
// Diabaikan selama wajah dikunci manual lewat SetFaceMode().
void OledDisplay::SetEmotion(const char* emotion) {
    DisplayLockGuard lock(this);
    if (emotion_locked_) return;
    Emotion e;
    emotion_ = ParseEmotion(emotion, e) ? e : Emotion::Neutral;
}

// Dipanggil oleh tool MCP. Emosi selain neutral akan bertahan sampai
// diganti lagi (neutral membuka kunci sehingga emosi otomatis aktif kembali).
bool OledDisplay::SetFaceMode(const char* mode) {
    Emotion e;
    if (!ParseEmotion(mode, e)) return false;
    DisplayLockGuard lock(this);
    emotion_ = e;
    emotion_locked_ = (e != Emotion::Neutral);
    return true;
}

void OledDisplay::SetChatMessage(const char* role, const char* content) {}

void OledDisplay::IdleBehavior(int base_eye_height) {
    if (rand() % 50 == 0) {
        idle_move_offset_x_ = (rand() % 5) - 2;
        idle_move_offset_y_ = (rand() % 5) - 2;
    }

    // Mata mengantuk/terpejam: buat tinggi sangat tipis (misal: 2px atau 3px)
    int eye_h = 3; 
    int eye_w = EYE_WIDTH + 2; // Buat sedikit lebih lebar saat merem/mengantuk

    lv_obj_set_size(left_eye_, eye_w, eye_h);
    lv_obj_set_size(right_eye_, eye_w, eye_h);

    // Radius dibuat kecil (atau 1-2) agar terlihat seperti garis kelopak mata
    lv_obj_set_style_radius(left_eye_, 1, 0);
    lv_obj_set_style_radius(right_eye_, 1, 0);

    // Posisi Y ditambahkan offset (misal: +2 atau +3) agar mata terlihat seperti turun/sayu
    int sleepy_y_offset = 2;

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -EYE_OFFSET_X + idle_move_offset_x_, idle_move_offset_y_ + sleepy_y_offset);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, EYE_OFFSET_X + idle_move_offset_x_, idle_move_offset_y_ + sleepy_y_offset);
    
    // Mulut diam (bisa senyum tipis atau garis kecil lurus)
    lv_obj_set_size(mouth_, 8, 2);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, idle_move_offset_x_, 12 + idle_move_offset_y_);
}


void OledDisplay::ListeningBehavior(int base_eye_height) {
    // Ukuran kedua mata disamakan menggunakan base_eye_height dan EYE_WIDTH normal
    lv_obj_set_size(left_eye_, EYE_WIDTH, base_eye_height);
    lv_obj_set_size(right_eye_, EYE_WIDTH, base_eye_height);

    // Radius sudut mata dibuat sama rata
    lv_obj_set_style_radius(left_eye_, EYE_RADIUS, 0);
    lv_obj_set_style_radius(right_eye_, EYE_RADIUS, 0);

    // Posisi vertikal disejajarkan (offset Y = 0) agar tidak terlihat miring
    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -EYE_OFFSET_X, 0);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, EYE_OFFSET_X, 0);
    
    // Mulut garis lurus kecil saat mendengarkan (tetap dipertahankan)
    lv_obj_set_size(mouth_, 8, 2);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, 12);
}


void OledDisplay::SpeakingBehavior(int eye_height) {
    uint32_t now = lv_tick_get();

    // Mengatur kecepatan buka-tutup mulut saat berbicara (tiap 100ms)
    if (now - speak_last_update_ > 100) {
        speak_last_update_ = now;
        // Tinggi mulut bervariasi antara 2 piksel (menutup) hingga 10 piksel (membuka lebar)
        speak_mouth_target_ = 2 + (rand() % 9); 
    }

    // Mengatur pergerakan dinamis pada mata agar terlihat hidup saat berbicara (tiap 180ms)
    if (now - speak_eye_last_update_ > 180) {
        speak_eye_last_update_ = now;
        // Pergeseran kecil pada posisi mata (-2 sampai +2 piksel secara horizontal & vertikal)
        speak_move_offset_x_ = (rand() % 5) - 2;
        speak_move_offset_y_ = (rand() % 3) - 1; 
    }

    int eye_w = EYE_WIDTH;
    int eye_h = eye_height;

    // Sesekali lebar mata sedikit berkedut/bervariasi saat berbicara
    if (rand() % 25 == 0) {
        eye_w += (rand() % 5) - 2;
    }

    lv_obj_set_size(left_eye_, eye_w, eye_h);
    lv_obj_set_size(right_eye_, eye_w, eye_h);

    lv_obj_set_style_radius(left_eye_, (eye_h < 8) ? 2 : EYE_RADIUS, 0);
    lv_obj_set_style_radius(right_eye_, (eye_h < 8) ? 2 : EYE_RADIUS, 0);

    // Mulut aktif bergerak naik-turun tingginya menyerupai robot sedang berbicara
    lv_obj_set_size(mouth_, 12 + (rand() % 3), speak_mouth_target_);
    lv_obj_set_style_radius(mouth_, speak_mouth_target_ > 5 ? 4 : 2, 0);

    // Posisi mata dan mulut dengan tambahan offset pergerakan agar tampak hidup
    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -EYE_OFFSET_X + speak_move_offset_x_, -2 + speak_move_offset_y_);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, EYE_OFFSET_X + speak_move_offset_x_, -2 + speak_move_offset_y_);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, speak_move_offset_x_, 12);
}


// Lapisan emosi: dijalankan setelah behavior state, jadi hanya menimpa
// bentuk mata/mulut dan menampilkan alis/busur mulut sesuai emosi.
void OledDisplay::ApplyEmotion(int eye_height) {
    // Bentuk mata sesuai emosi (tinggi dibatasi max_h, tetap ikut kedip)
    auto shape = [&](int max_h, int radius, int dy) {
        int h = eye_height < max_h ? eye_height : max_h;
        for (lv_obj_t* e : {left_eye_, right_eye_}) {
            lv_obj_set_height(e, h);
            lv_obj_set_style_radius(e, radius, 0);
            lv_obj_set_style_translate_y(e, dy, 0);
        }
    };

    bool brows = false;
    bool arc = false;
    const lv_point_precise_t* lp = kBrowDown;
    const lv_point_precise_t* rp = kBrowUp;

    switch (emotion_) {
        case Emotion::Happy:
            shape(10, 5, 0);
            arc = true;
            lv_arc_set_bg_angles(mouth_arc_, 30, 150);       // senyum
            lv_obj_align(mouth_arc_, LV_ALIGN_CENTER, 0, 3);
            break;
        case Emotion::Sad:
            shape(14, EYE_RADIUS, 2);
            brows = true;
            lp = kBrowUp;                                    // alis miring ke luar
            rp = kBrowDown;
            arc = true;
            lv_arc_set_bg_angles(mouth_arc_, 210, 330);      // cemberut
            lv_obj_align(mouth_arc_, LV_ALIGN_CENTER, 0, 20);
            break;
        case Emotion::Angry:
            shape(12, 3, 0);
            brows = true;
            lp = kBrowDown;                                  // alis miring ke dalam
            rp = kBrowUp;
            break;
        default:
            // Netral: behavior state tidak diubah, hanya reset geseran mata
            for (lv_obj_t* e : {left_eye_, right_eye_}) {
                lv_obj_set_style_translate_y(e, 0, 0);
            }
            break;
    }

    // Alis
    if (brows) {
        lv_line_set_points(left_brow_, lp, 2);
        lv_line_set_points(right_brow_, rp, 2);
        lv_obj_align(left_brow_, LV_ALIGN_CENTER, -EYE_OFFSET_X, BROW_Y);
        lv_obj_align(right_brow_, LV_ALIGN_CENTER, EYE_OFFSET_X, BROW_Y);
        lv_obj_remove_flag(left_brow_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(right_brow_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(left_brow_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(right_brow_, LV_OBJ_FLAG_HIDDEN);
    }

    // Saat berbicara, mulut kotak tetap dipakai agar animasi buka-tutup jalan
    bool use_arc = arc && state_ != FaceState::Speaking;
    if (use_arc) {
        lv_obj_remove_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(mouth_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(mouth_arc_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(mouth_, LV_OBJ_FLAG_HIDDEN);
    }
}


void OledDisplay::Update() {
    if (!container_) return;

    if (blink_phase_ == 0) {
        if (rand() % 80 == 0) {
            blink_phase_ = 1;
        }
    }

    int eye_height = EYE_HEIGHT;

    switch (blink_phase_) {
        case 1:
            eye_height = EYE_HEIGHT / 2;
            blink_phase_ = 2;
            break;
        case 2:
            eye_height = 2; // Kedip rapat
            blink_phase_ = 3;
            break;
        case 3:
            eye_height = EYE_HEIGHT / 2;
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

    ApplyEmotion(eye_height);
}
