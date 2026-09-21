#include "face_engine.h"
#include <stdlib.h>
#include <esp_log.h>

#define TAG "FaceEngine"

#define EYE_OFFSET_X 6
#define EYE_OFFSET_Y 4

void FaceEngine::Init(lv_obj_t* parent) {
    if (!parent) return;

    // Pastikan eye_size_ memiliki nilai valid jika belum terinisialisasi di header
    if (eye_size_ <= 0) eye_size_ = 14;

    // Container disesuaikan dengan ukuran parent (128x48) atau dipasang penuh
    container_ = lv_obj_create(parent);
    lv_obj_set_size(container_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_all(container_, 0, 0); // Matikan padding agar koordinat akurat
    lv_obj_set_scrollbar_mode(container_, LV_SCROLLBAR_MODE_OFF); // Matikan scrollbar
    lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);

    left_eye_ = lv_obj_create(container_);
    right_eye_ = lv_obj_create(container_);
    mouth_ = lv_obj_create(container_);

    // Hilangkan border, padding, dan scrollbar untuk semua elemen wajah
    lv_obj_t* elements[] = {left_eye_, right_eye_, mouth_};
    for (lv_obj_t* el : elements) {
        lv_obj_set_style_border_width(el, 0, 0);
        lv_obj_set_style_pad_all(el, 0, 0);
        lv_obj_set_scrollbar_mode(el, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_color(el, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(el, LV_OPA_COVER, 0);
    }

    lv_obj_set_style_radius(left_eye_, 2, 0);
    lv_obj_set_style_radius(right_eye_, 2, 0);
    lv_obj_set_style_radius(mouth_, 3, 0);

    // Set ukuran awal yang valid
    lv_obj_set_size(left_eye_, eye_size_, eye_size_);
    lv_obj_set_size(right_eye_, eye_size_, eye_size_);
    lv_obj_set_size(mouth_, 16, 4);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_size_ - EYE_OFFSET_X, -EYE_OFFSET_Y);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_size_ + EYE_OFFSET_X, -EYE_OFFSET_Y);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, 10);

    // Timer animasi
    lv_timer_create(
        [](lv_timer_t* t) {
            auto face = static_cast<FaceEngine*>(lv_timer_get_user_data(t));
            if (face) {
                face->Update();
            }
        },
        60, this);
}

void FaceEngine::SetState(FaceState state) { state_ = state; }

void FaceEngine::IdleBehavior(int base_eye_height) {
    if (rand() % 40 == 0) {
        idle_move_offset_x_ = (rand() % 5) - 2;
        idle_move_offset_y_ = (rand() % 3) - 1;
    }

    int eye_h = (base_eye_height > 0) ? base_eye_height : 2;
    int eye_w = eye_h * 0.65;
    if (eye_w < 1) eye_w = 1;

    lv_obj_set_size(left_eye_, eye_w, eye_h);
    lv_obj_set_size(right_eye_, eye_w, eye_h);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_size_ + idle_move_offset_x_,
                 -4 + idle_move_offset_y_);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_size_ + idle_move_offset_x_,
                 -4 + idle_move_offset_y_);

    lv_obj_set_size(mouth_, 16, 4);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, idle_move_offset_x_, 12 + idle_move_offset_y_);
}

void FaceEngine::ListeningBehavior(int base_eye_height) {
    int right_h = (base_eye_height > 0) ? base_eye_height : 2;
    int right_w = right_h * 0.65;

    int left_h = (base_eye_height - 2 > 0) ? base_eye_height - 2 : 2;
    int left_w = left_h * 0.65;

    lv_obj_set_size(left_eye_, left_w, left_h);
    lv_obj_set_size(right_eye_, right_w, right_h);

    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_size_ - EYE_OFFSET_X, -EYE_OFFSET_Y);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_size_ + EYE_OFFSET_X, -EYE_OFFSET_Y);

    lv_obj_set_size(mouth_, 10, 2);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, 12);
}

void FaceEngine::SpeakingBehavior(int eye_height) {
    uint32_t now = lv_tick_get();

    int h = (eye_height > 0) ? eye_height : 2;
    int w = h * 0.65;

    lv_obj_set_size(left_eye_, w, h);
    lv_obj_set_size(right_eye_, w, h);

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
            speak_mouth_target_ = 14;
    }

    if (speak_mouth_current_ < speak_mouth_target_)
        speak_mouth_current_ += 2;
    else if (speak_mouth_current_ > speak_mouth_target_)
        speak_mouth_current_ -= 2;

    if (speak_mouth_current_ < 2) speak_mouth_current_ = 2;

    lv_obj_set_size(mouth_, 14, speak_mouth_current_);
    lv_obj_set_style_radius(mouth_, 4, 0);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, 12);
}

void FaceEngine::Update() {
    if (!container_ || !left_eye_ || !right_eye_ || !mouth_)
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
