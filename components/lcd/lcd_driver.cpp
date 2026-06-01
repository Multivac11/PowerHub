#include "lcd_driver.h"

#include <cstdlib>
#include <cstring>

// ================== 构造 ==================
LcdDriver::LcdDriver(uint16_t* buf, int buf_w, int buf_h, Rotation rot) : buf_(buf), buf_w_(buf_w), buf_h_(buf_h)
{
    switch (rot)
    {
        case Rotation::k0:
            w_ = buf_w_;
            h_ = buf_h_;
            break;
        case Rotation::k90:
            w_ = buf_h_;
            h_ = buf_w_;
            break;
        case Rotation::k180:
            w_ = buf_w_;
            h_ = buf_h_;
            break;
        case Rotation::k270:
            w_ = buf_h_;
            h_ = buf_w_;
            break;
    }

    switch (rot)
    {
        case Rotation::k0:
            for (int i = 0; i < w_; ++i) lut_x_[i] = i;
            for (int i = 0; i < h_; ++i) lut_y_[i] = i * buf_w_;
            break;
        case Rotation::k90:
            for (int i = 0; i < w_; ++i) lut_x_[i] = i * buf_w_;
            for (int i = 0; i < h_; ++i) lut_y_[i] = buf_w_ - 1 - i;
            break;
        case Rotation::k180:
            for (int i = 0; i < w_; ++i) lut_x_[i] = buf_w_ - 1 - i;
            for (int i = 0; i < h_; ++i) lut_y_[i] = (buf_h_ - 1 - i) * buf_w_;
            break;
        case Rotation::k270:
            for (int i = 0; i < w_; ++i) lut_x_[i] = (buf_h_ - 1 - i) * buf_w_;
            for (int i = 0; i < h_; ++i) lut_y_[i] = i;
            break;
    }
}

// ================== 点 (LUT: idx = lut_x_[x] + lut_y_[y]) ==================
void LcdDriver::DrawPixel(int x, int y, uint16_t color)
{
    if (x < 0 || x >= w_ || y < 0 || y >= h_) return;
    buf_[lut_x_[x] + lut_y_[y]] = color;
}

// ================== 直线 (Bresenham) ==================
void LcdDriver::DrawLine(int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true)
    {
        DrawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// ================== 水平线 ==================
void LcdDriver::DrawHLine(int x, int y, int len, uint16_t color)
{
    if (y < 0 || y >= h_) return;
    if (x < 0) { len += x; x = 0; }
    if (x + len > w_) len = w_ - x;
    if (len <= 0) return;

    uint32_t base = lut_y_[y];
    for (int i = 0; i < len; ++i) buf_[lut_x_[x + i] + base] = color;
}

// ================== 垂直线 ==================
void LcdDriver::DrawVLine(int x, int y, int len, uint16_t color)
{
    if (x < 0 || x >= w_) return;
    if (y < 0) { len += y; y = 0; }
    if (y + len > h_) len = h_ - y;
    if (len <= 0) return;

    uint32_t base = lut_x_[x];
    for (int i = 0; i < len; ++i) buf_[base + lut_y_[y + i]] = color;
}

// ================== 矩形边框 ==================
void LcdDriver::DrawRect(int x, int y, int w, int h, uint16_t color)
{
    DrawHLine(x, y, w, color);
    DrawHLine(x, y + h - 1, w, color);
    DrawVLine(x, y, h, color);
    DrawVLine(x + w - 1, y, h, color);
}

// ================== 填充矩形 (所有旋转统一, LUT 保证连续) ==================
void LcdDriver::FillRect(int x, int y, int w, int h, uint16_t color)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > w_) w = w_ - x;
    if (y + h > h_) h = h_ - y;
    if (w <= 0 || h <= 0) return;

    // 逐行填充: 每行物理不保证连续, 但每列在 LUT 中连续
    for (int row = 0; row < h; ++row)
    {
        uint32_t base = lut_y_[y + row];
        for (int col = 0; col < w; ++col) buf_[lut_x_[x + col] + base] = color;
    }
}

// ================== 圆角矩形边框 ==================
void LcdDriver::DrawRoundRect(int x, int y, int w, int h, int r, uint16_t color)
{
    if (r <= 0) { DrawRect(x, y, w, h, color); return; }
    int d = r + r;
    if (d > w) r = w / 2;
    if (d > h) r = h / 2;

    DrawHLine(x + r, y, w - 2 * r, color);
    DrawHLine(x + r, y + h - 1, w - 2 * r, color);
    DrawVLine(x, y + r, h - 2 * r, color);
    DrawVLine(x + w - 1, y + r, h - 2 * r, color);

    struct Corner { int cx, cy, sx, sy; };
    Corner corners[4] = {
        {x + r,         y + r,         -1, -1},
        {x + w - 1 - r, y + r,          1, -1},
        {x + r,         y + h - 1 - r, -1,  1},
        {x + w - 1 - r, y + h - 1 - r,  1,  1},
    };
    for (auto& c : corners)
    {
        int px = 0, py = r, err = 3 - 2 * r;
        while (px <= py)
        {
            DrawPixel(c.cx + c.sx * py, c.cy + c.sy * px, color);
            DrawPixel(c.cx + c.sx * px, c.cy + c.sy * py, color);
            if (err < 0) err += 4 * px + 6;
            else { err += 4 * (px - py) + 10; --py; }
            ++px;
        }
    }
}

// ================== 填充圆角矩形 ==================
void LcdDriver::FillRoundRect(int x, int y, int w, int h, int r, uint16_t color)
{
    if (r <= 0) { FillRect(x, y, w, h, color); return; }
    int d = r + r;
    if (d > w) r = w / 2;
    if (d > h) r = h / 2;

    FillRect(x, y + r, w, h - 2 * r, color);
    FillRect(x + r, y, w - 2 * r, r, color);
    FillRect(x + r, y + h - r, w - 2 * r, r, color);

    struct Corner { int cx, cy, sx, sy; };
    Corner corners[4] = {
        {x + r,         y + r,         -1, -1},
        {x + w - 1 - r, y + r,          1, -1},
        {x + r,         y + h - 1 - r, -1,  1},
        {x + w - 1 - r, y + h - 1 - r,  1,  1},
    };
    for (auto& c : corners)
    {
        int px = 0, py = r, err = 3 - 2 * r;
        while (px <= py)
        {
            int x0, len0, x1, len1;
            if (c.sx < 0)
            {
                x0 = c.cx - py; len0 = py + 1;
                x1 = c.cx - px; len1 = px + 1;
            }
            else
            {
                x0 = c.cx; len0 = py + 1;
                x1 = c.cx; len1 = px + 1;
            }
            DrawHLine(x0, c.cy + c.sy * px, len0, color);
            DrawHLine(x1, c.cy + c.sy * py, len1, color);
            if (err < 0) err += 4 * px + 6;
            else { err += 4 * (px - py) + 10; --py; }
            ++px;
        }
    }
}

// ================== 圆形边框 (Bresenham) ==================
void LcdDriver::DrawCircle(int cx, int cy, int r, uint16_t color)
{
    int x = 0, y = r, d = 3 - 2 * r;
    while (x <= y)
    {
        DrawPixel(cx + x, cy + y, color);
        DrawPixel(cx + x, cy - y, color);
        DrawPixel(cx - x, cy + y, color);
        DrawPixel(cx - x, cy - y, color);
        DrawPixel(cx + y, cy + x, color);
        DrawPixel(cx + y, cy - x, color);
        DrawPixel(cx - y, cy + x, color);
        DrawPixel(cx - y, cy - x, color);
        if (d < 0) d += 4 * x + 6;
        else { d += 4 * (x - y) + 10; --y; }
        ++x;
    }
}

// ================== 填充圆形 ==================
void LcdDriver::FillCircle(int cx, int cy, int r, uint16_t color)
{
    int x = 0, y = r, d = 3 - 2 * r;
    while (x <= y)
    {
        DrawHLine(cx - x, cy - y, 2 * x + 1, color);
        DrawHLine(cx - x, cy + y, 2 * x + 1, color);
        DrawHLine(cx - y, cy - x, 2 * y + 1, color);
        DrawHLine(cx - y, cy + x, 2 * y + 1, color);
        if (d < 0) d += 4 * x + 6;
        else { d += 4 * (x - y) + 10; --y; }
        ++x;
    }
}

// ================== 字符绘制 (通用, 按字体宽度分发) ==================
void LcdDriver::DrawChar(int x, int y, char c, uint16_t color, uint16_t bg, const Font& font)
{
    if (c < 0x20 || c > 0x7E) c = '?';
    int idx = c - 0x20;

    FillRect(x, y, font.w, font.h, bg);

    if (font.w <= 8)
    {
        const uint8_t* glyphs = (const uint8_t*)font.data;
        const uint8_t* glyph = glyphs + idx * font.h;
        for (int col = 0; col < font.w; ++col)
            for (int row = 0; row < font.h; ++row)
                if (glyph[row] & (0x80 >> col))
                    DrawPixel(x + col, y + row, color);
    }
    else if (font.w <= 16)
    {
        const uint16_t* glyphs = (const uint16_t*)font.data;
        const uint16_t* glyph = glyphs + idx * font.h;
        for (int col = 0; col < font.w; ++col)
            for (int row = 0; row < font.h; ++row)
                if (glyph[row] & (0x8000 >> col))
                    DrawPixel(x + col, y + row, color);
    }
    else if (font.w <= 32)
    {
        const uint32_t* glyphs = (const uint32_t*)font.data;
        const uint32_t* glyph = glyphs + idx * font.h;
        for (int col = 0; col < font.w; ++col)
            for (int row = 0; row < font.h; ++row)
                if (glyph[row] & (0x80000000 >> col))
                    DrawPixel(x + col, y + row, color);
    }
}

// ================== 字符串绘制 ==================
void LcdDriver::DrawString(int x, int y, const char* str, uint16_t color, uint16_t bg, const Font& font)
{
    while (*str)
    {
        DrawChar(x, y, *str, color, bg, font);
        x += font.w;
        ++str;
    }
}

// ================== 全屏填充 ==================
void LcdDriver::FillScreen(uint16_t color)
{
    size_t total = (size_t)buf_w_ * buf_h_;
    for (size_t i = 0; i < total; ++i) buf_[i] = color;
}

// ================== 刷新到屏幕 ==================
void LcdDriver::Flush(esp_lcd_panel_handle_t panel)
{
    esp_lcd_panel_draw_bitmap(panel, 0, 0, buf_w_, buf_h_, buf_);
}
