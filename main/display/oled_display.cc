#include "oled_display.h"
#include "lang/lang.h"
#include <esp_log.h>
#include <cstring>
#include <cstdlib>

static const char* TAG = "OledDisplay";

void OledDisplay::SetStatus(const char* status) {
    if (status == nullptr) return;

    DisplayLockGuard lock(this);
    ESP_LOGI(TAG, "SetStatus: %s", status);

    // Sinkronisasi status dari application.cc berbasis Lang::Strings
    if (strcmp(status, Lang::Strings::LISTENING) == 0) {
        state_ = FaceState::Listening;
    } else if (strcmp(status, Lang::Strings::SPEAKING) == 0) {
        state_ = FaceState::Speaking;
    } else if (strcmp(status, Lang::Strings::STANDBY) == 0) {
        state_ = FaceState::Idle;
    } else {
        // Status lain (CONNECTING, ETHERNET, dll) diarahkan ke Idle
        state_ = FaceState::Idle;
    }
}

void OledDisplay::SetEmotion(const char* emotion) {
    if (emotion == nullptr) return;
    ESP_LOGI(TAG, "SetEmotion: %s", emotion);
    // Dibiarkan tanpa mengubah state_ agar panggilan SetEmotion("neutral") 
    // dari application.cc tidak mengacaukan status animasi mata.
}

// Mode IDLE: Efek bernapas / mata meram
void OledDisplay::IdleBehavior(int base_eye_height) {
    uint32_t now = lv_tick_get();

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

// Mode LISTENING: Mata Bangun & Terbuka Lebar
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
        speak_mouth_target_ = (EYE_HEIGHT - 6) + (rand() % 10);
    }

    int eye_h = speak_mouth_target_;
    if (eye_height < 8) eye_h = eye_height; // Jika sedang kedip

    int eye_w = EYE_WIDTH + (EYE_HEIGHT - eye_h) / 2;

    lv_obj_set_size(left_eye_, eye_w, eye_h);
    lv_obj_set_size(right_eye_, eye_w, eye_h);

    lv_obj_set_style_radius(left_eye_, (eye_h < 8) ? 2 : EYE_RADIUS, 0);
    lv_obj_set_style_radius(right_eye_, (eye_h < 8) ? 2 : EYE_RADIUS, 0);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -EYE_OFFSET_X, 0);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, EYE_OFFSET_X, 0);
}

// Loop Utama Rendering Animasi Mata
void OledDisplay::Update() {
    DisplayLockGuard lock(this);

    int current_eye_height = EYE_HEIGHT; 

    switch (state_) {
        case FaceState::Idle:
            IdleBehavior(current_eye_height);
            break;
        case FaceState::Listening:
            ListeningBehavior(current_eye_height);
            break;
        case FaceState::Speaking:
            SpeakingBehavior(current_eye_height);
            break;
    }
}
