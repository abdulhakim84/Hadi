#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "display.h"
#include <lvgl.h>

// Enum untuk status animasi mata
enum class FaceState {
    Idle,
    Listening,
    Speaking
};

class OledDisplay : public Display {
public:
    OledDisplay();
    virtual ~OledDisplay();

    void SetStatus(const char* status) override;
    void SetEmotion(const char* emotion) override;

    void Update(); // Dipanggil secara periodik oleh timer/loop LVGL

private:
    FaceState state_ = FaceState::Idle;

    // Komponen Objek LVGL Mata
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;

    // Konstanta Ukuran & Posisi Mata
    static constexpr int EYE_WIDTH = 30;
    static constexpr int EYE_HEIGHT = 40;
    static constexpr int EYE_OFFSET_X = 25;
    static constexpr int EYE_RADIUS = 12;

    // Variabel Animasi
    uint32_t speak_last_update_ = 0;
    int speak_mouth_target_ = EYE_HEIGHT;

    // Fungsi Animasi Masing-Masing Mode
    void IdleBehavior(int base_eye_height);
    void ListeningBehavior(int base_eye_height);
    void SpeakingBehavior(int eye_height);
};

#endif // OLED_DISPLAY_H
