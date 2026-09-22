#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "lvgl_display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

class OledDisplay : public LvglDisplay {
private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    lv_obj_t* container_ = nullptr;

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    void SetStatus(const char* status) override;
    void SetupUI_128x64();

public:
    OledDisplay(
        esp_lcd_panel_io_handle_t panel_io,
        esp_lcd_panel_handle_t panel,
        int width,
        int height,
        bool mirror_x,
        bool mirror_y
    );

    ~OledDisplay();

    virtual void SetupUI() override;
    virtual void SetChatMessage(
        const char* role,
        const char* content
    ) override;

    virtual void SetEmotion(const char* emotion) override;
    virtual void SetTheme(Theme* theme) override;
};

#endif
