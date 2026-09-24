#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "lvgl_display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <lvgl.h>

enum class FaceState {
    Idle,       // Mata tidur/terpejam dengan efek bernapas
    Listening,  // Mata bangun & menyimak
    Speaking    // Mata bergerak/memantul saat AI berbicara
};

class OledDisplay : public LvglDisplay {
public:
    OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                int width, int height, bool mirror_x, bool mirror_y);
    virtual ~OledDisplay();

    virtual void SetupUI() override;
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    virtual void SetTheme(Theme* theme) override;

    // Fungsi-fungsi penangkap panggilan dari application.cc
    virtual void SetStatus(const char* status) override;
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetChatMessage(const char* role, const char* content) override;

    // Fungsi kontrol status manual
    void SetState(FaceState state);

private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    lv_obj_t* container_ = nullptr;
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    lv_timer_t* timer_ = nullptr;

    FaceState state_ = FaceState::Idle;
    int blink_phase_ = 0;
    uint32_t speak_last_update_ = 0;
    int speak_mouth_target_ = 0;
    bool setup_ui_called_ = false;

    void Update();
    void IdleBehavior(int base_eye_height);
    void ListeningBehavior(int base_eye_height);
    void SpeakingBehavior(int base_eye_height);
};

#endif // OLED_DISPLAY_H
