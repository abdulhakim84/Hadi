#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "lvgl_display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <stdint.h>

enum class FaceState {
    Idle,
    Listening,
    Speaking
};

class OledDisplay : public LvglDisplay {
private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    // Elemen UI Animasi Wajah
    lv_obj_t* container_ = nullptr;
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_obj_t* mouth_ = nullptr;
    lv_timer_t* timer_ = nullptr;

    // Variabel Status dan Parameter Animasi
    FaceState state_ = FaceState::Idle;
    int eye_size_ = 16;
    int idle_move_offset_x_ = 0;
    int idle_move_offset_y_ = 0;
    int blink_phase_ = 0;
    uint32_t speak_last_update_ = 0;
    int speak_mouth_target_ = 0;
    int speak_mouth_current_ = 0;

    // Fungsi Internal Animasi Wajah
    void IdleBehavior(int base_eye_height);
    void ListeningBehavior(int base_eye_height);
    void SpeakingBehavior(int eye_height);
    void Update();

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

public:
    OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, bool mirror_x, bool mirror_y);
    ~OledDisplay();

    virtual void SetupUI() override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetTheme(Theme* theme) override;

    void SetState(FaceState state);
};

#endif // OLED_DISPLAY_H
