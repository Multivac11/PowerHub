#pragma once

#include <stdint.h>

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "font/font.h"

#define RGB565(r, g, b) (uint16_t)(((r) & 0xF8) << 8 | ((g) & 0xFC) << 3 | (b) >> 3)

// 常用颜色
static constexpr uint16_t kColorBlack = 0x0000;
static constexpr uint16_t kColorWhite = 0xFFFF;
static constexpr uint16_t kColorRed = 0xF800;
static constexpr uint16_t kColorGreen = 0x07E0;
static constexpr uint16_t kColorBlue = 0x001F;
static constexpr uint16_t kColorYellow = 0xFFE0;
static constexpr uint16_t kColorCyan = 0x07FF;
static constexpr uint16_t kColorMagenta = 0xF81F;
static constexpr uint16_t kColorSkyBlue = 0x055F;  // #00a8ff
static constexpr uint16_t kColorGray = 0x8410;
static constexpr uint16_t kColorOrange = 0xFD20;

enum class Rotation
{
    k0,
    k90,
    k180,
    k270,
};

class LcdDriver
{
   public:
    LcdDriver(uint16_t* buf, int buf_w, int buf_h, Rotation rot = Rotation::k90);

    void DrawPixel(int x, int y, uint16_t color);

    void DrawLine(int x0, int y0, int x1, int y1, uint16_t color);

    void DrawHLine(int x, int y, int len, uint16_t color);

    void DrawVLine(int x, int y, int len, uint16_t color);

    void DrawRect(int x, int y, int w, int h, uint16_t color);

    void FillRect(int x, int y, int w, int h, uint16_t color);

    void DrawRoundRect(int x, int y, int w, int h, int r, uint16_t color);

    void FillRoundRect(int x, int y, int w, int h, int r, uint16_t color);

    void DrawCircle(int cx, int cy, int r, uint16_t color);

    void FillCircle(int cx, int cy, int r, uint16_t color);

    void DrawChar(int x, int y, char c, uint16_t color, uint16_t bg, const Font& font = kFont16x32);

    void DrawString(int x, int y, const char* str, uint16_t color, uint16_t bg, const Font& font = kFont16x32);

    void FillScreen(uint16_t color);

    void Flush(esp_lcd_panel_handle_t panel);

    int Width() const { return w_; }
    int Height() const { return h_; }

   private:
    static constexpr int kLutMax = 960;  // max(物理宽, 物理高), 够存所有旋转方向

    uint16_t* buf_;
    int buf_w_;
    int buf_h_;
    int w_;
    int h_;

    // 查表: PhysIdx(x,y) = lut_x_[x] + lut_y_[y]
    // 有效索引: lut_x_[0..w_-1], lut_y_[0..h_-1]
    uint32_t lut_x_[kLutMax];
    uint32_t lut_y_[kLutMax];
};
