#include "scene_manager.h"
#include <cstdio>

#include "ina226.h"

static const char *TAG = "SceneManager";

// INA226 实际 I2C 地址（非连续）
static constexpr uint16_t kIna226Addrs[5] = {0x40, 0x41, 0x44, 0x45, 0x42};

void SceneManager::SceneManagerInit()
{
    esp_err_t ret = LcdRgb::GetInstance().LcdInit();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "LCD init failed: %s", esp_err_to_name(ret));
        return;
    }

    panel_ = LcdRgb::GetInstance().GetPanel();

    size_t buf_size = LCD_H_RES * LCD_V_RES * sizeof(uint16_t);
    buf_ = (uint16_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (buf_ == nullptr)
    {
        ESP_LOGE(TAG, "PSRAM alloc failed!");
        return;
    }

    lcd_ = new LcdDriver(buf_, LCD_H_RES, LCD_V_RES, Rotation::k270);

    xTaskCreatePinnedToCore(UIManagerTask, "UIManagerTask", 8096, this, 5, nullptr, 0);
    xTaskCreatePinnedToCore(MonitorListenerTask, "MonitorListenerTask", 4096, this, 5, nullptr, 0);
}

void SceneManager::UIManagerTask(void *pvParameters)
{
    static_cast<SceneManager *>(pvParameters)->UIManager();
}

void SceneManager::MonitorListenerTask(void *pvParameters)
{
    static_cast<SceneManager *>(pvParameters)->MonitorListener();
}

static void DrawSegmentedBar(
    LcdDriver &lcd, int x, int y, int segs, int segW, int segH, int gap, float pct, uint16_t color)
{
    if (pct < 0.0f)
        pct = 0.0f;
    if (pct > 1.0f)
        pct = 1.0f;
    int filled = (int)(segs * pct + 0.5f);
    for (int i = 0; i < segs; ++i)
    {
        int sx = x + i * (segW + gap);
        if (i < filled)
            lcd.FillRect(sx, y, segW, segH, color); // 实心
        else
            lcd.DrawRect(sx, y, segW, segH, color); // 空心
    }
}

static void DrawScanlines(LcdDriver &lcd, int x, int y, int w, int h)
{
    for (int sy = y; sy < y + h; sy += 2)
        lcd.DrawHLine(x, sy, w, kColorBlack);
}

static void DrawCornerBrackets(LcdDriver &lcd, int cx, int cy, int cw, int ch, uint16_t color)
{
    const int B = 10;
    lcd.DrawHLine(cx + 6, cy + 4, B, color);
    lcd.DrawVLine(cx + 4, cy + 6, B, color);
    lcd.DrawHLine(cx + cw - 6 - B, cy + 4, B, color);
    lcd.DrawVLine(cx + cw - 6, cy + 6, B, color);
    lcd.DrawHLine(cx + 6, cy + ch - 6, B, color);
    lcd.DrawVLine(cx + 4, cy + ch - 6 - B, B, color);
    lcd.DrawHLine(cx + cw - 6 - B, cy + ch - 6, B, color);
    lcd.DrawVLine(cx + cw - 6, cy + ch - 6 - B, B, color);
}

// 顶部状态指示：实心圆 + ON/OFF（y=共同的视觉中心线）
static void DrawStatusIndicator(LcdDriver &lcd, int x, int y, bool on)
{
    uint16_t color = on ? kColorGreen : kColorRed;
    lcd.FillCircle(x, y, 6, color);
    lcd.DrawString(x + 14, y - 16, on ? "ON" : "OFF", color, kColorBlack, kFont16x32);
}

void SceneManager::UIManager()
{
    LcdDriver &lcd = *lcd_;
    const int W = lcd.Width();
    const int H = lcd.Height();

    // ===== CRT 荧光绿 =====
    constexpr uint16_t kPhosphor = 0x07E0;
    constexpr uint16_t kPhosphorDim = 0x0300;
    constexpr uint16_t kCardFill = 0x0120;
    constexpr uint16_t kBarHi = 0x07E0;
    constexpr uint16_t kBarMid = 0x0600;
    constexpr uint16_t kBarLo = 0x0300;

    constexpr int kCardW = 185;
    constexpr int kCardH = 305;
    constexpr int kCardR = 6;
    constexpr int kCardY = 62;
    constexpr int kCardGap = 6;

    // ===== 静态背景 =====
    lcd.FillScreen(kColorBlack);

    // 屏幕外框
    lcd.DrawRect(0, 0, W - 2, H, kPhosphorDim);
    lcd.DrawRect(1, 1, W - 4, H - 2, kPhosphorDim);
    for (int i = 0; i < 6; ++i)
    {
        lcd.DrawPixel(2 + i, 2, kPhosphor);
        lcd.DrawPixel(2, 2 + i, kPhosphor);
        lcd.DrawPixel(W - 5 - i, 2, kPhosphor);
        lcd.DrawPixel(W - 5, 2 + i, kPhosphor);
        lcd.DrawPixel(2 + i, H - 3, kPhosphor);
        lcd.DrawPixel(2, H - 3 - i, kPhosphor);
        lcd.DrawPixel(W - 5 - i, H - 3, kPhosphor);
        lcd.DrawPixel(W - 5, H - 3 - i, kPhosphor);
    }

    // 顶部标题分隔线
    lcd.DrawHLine(0, 46, W, kPhosphorDim);
    lcd.DrawHLine(0, 47, W, kPhosphor);
    lcd.DrawHLine(0, 48, W, kPhosphorDim);

    // 顶部状态指示（初始全部 OFF，CH + 圆 + ON/OFF 整体与卡片居中）
    for (int i = 0; i < 5; ++i)
    {
        int cx = 4 + i * (kCardW + kCardGap);
        char label[8];
        snprintf(label, sizeof(label), "CH%d", i + 1);
        lcd.DrawString(cx + 21, 7, label, kPhosphor, kColorBlack, kFont16x32);
        int ix = cx + 91;
        DrawStatusIndicator(lcd, ix, 23, false);
    }

    // 卡片
    for (int i = 0; i < 5; ++i)
    {
        int cx = 4 + i * (kCardW + kCardGap);
        lcd.FillRoundRect(cx, kCardY, kCardW, kCardH, kCardR, kCardFill);
        lcd.DrawRoundRect(cx, kCardY, kCardW, kCardH, kCardR, kPhosphorDim);
        lcd.DrawRoundRect(cx + 2, kCardY + 2, kCardW - 4, kCardH - 4, kCardR - 2, kPhosphor);
        DrawCornerBrackets(lcd, cx, kCardY, kCardW, kCardH, kPhosphor);
    }

    // CH 标签
    for (int i = 0; i < 5; ++i)
    {
        int cx = 4 + i * (kCardW + kCardGap);
        char label[8];
        snprintf(label, sizeof(label), "CH-%d >>", i + 1);
        lcd.DrawString(cx + 14, kCardY + 7, label, kPhosphor, kCardFill, kFont8x16);
    }

    for (int i = 0; i < 5; ++i)
    {
        int cx = 4 + i * (kCardW + kCardGap);
        bool found = (I2CBusManager::GetInstance().GetDeviceByAddr<INA226>(kIna226Addrs[i]) != nullptr);
        if (!found)
        {
            int tx = cx + (kCardW - 9 * 16) / 2;
            char label[8];
            snprintf(label, sizeof(label), "CH-%d", i + 1);
            lcd.DrawString(tx, kCardY + kCardH / 2 - 36, label, kColorRed, kCardFill, kFont16x32);
            lcd.DrawString(tx, kCardY + kCardH / 2 - 36 + 32, "OFFLINE", kColorRed, kCardFill, kFont16x32);
        }
    }

    // 初始选中通道 CH-1 方框
    {
        int cx0 = 4 + 0 * (kCardW + kCardGap);
        lcd.DrawRect(cx0 + 8, 4, kCardW - 16, 40, kPhosphor);
    }

    for (int i = 0; i < 5; ++i)
    {
        int cx = 4 + i * (kCardW + kCardGap);
        DrawScanlines(lcd, cx + 8, kCardY + 22, kCardW - 16, kCardH - 32);
    }

    lcd.Flush(panel_);
    lcd.Flush(panel_);

    float old_v[5] = {-1, -1, -1, -1, -1};
    float old_a[5] = {-1, -1, -1, -1, -1};
    float old_w[5] = {-1, -1, -1, -1, -1};
    bool old_enabled[5] = {false, false, false, false, false};
    uint8_t old_selected = 0xFF;  // 0xFF 强制首次绘制

    while (true)
    {
        if (!data_)
        {
            vTaskDelay(pdMS_TO_TICKS(70));
            continue;
        }

        auto &ev = *data_;
        char buf[64];
        bool dirty = false;

        // ===== 顶部状态指示更新 =====
        for (int i = 0; i < 5; ++i)
        {
            bool on = ev.ina_data_[i].enabled_ && !ev.ina_data_[i].not_found_;
            if (on != old_enabled[i])
            {
                int cx = 4 + i * (kCardW + kCardGap);
                int ix = cx + 91;
                // 清除旧指示（32px 字体, 中心线 y=23）
                lcd.FillRect(ix - 8, 5, 70, 38, kColorBlack);
                // 重绘
                DrawStatusIndicator(lcd, ix, 23, on);
                old_enabled[i] = on;
                dirty = true;
            }
        }

        // ===== 选中通道方框更新 =====
        uint8_t selected = ev.selected_ch_;
        if (selected != old_selected)
        {
            // 清除旧方框
            if (old_selected < 5)
            {
                int ocx = 4 + old_selected * (kCardW + kCardGap);
                lcd.DrawRect(ocx + 8, 4, kCardW - 16, 40, kColorBlack);
            }
            // 绘制新方框
            int cx = 4 + selected * (kCardW + kCardGap);
            lcd.DrawRect(cx + 8, 4, kCardW - 16, 40, kPhosphor);
            old_selected = selected;
            dirty = true;
        }

        // 5 张卡片
        for (int i = 0; i < 5; ++i)
        {
            int cx = 4 + i * (kCardW + kCardGap);
            auto &port = ev.ina_data_[i];
            if (port.not_found_)
                continue;

            int x = cx + 12;
            int vy = kCardY + 28;
            int ay = kCardY + 118;
            int wy = kCardY + 208;

            // 电压
            if (port.bus_voltage_ != old_v[i])
            {
                lcd.FillRect(x - 2, vy - 2, 164, 74, kCardFill);
                snprintf(buf, sizeof(buf), "%.2f", port.bus_voltage_);
                lcd.DrawString(x, vy, buf, kPhosphor, kCardFill, kFont24x48);
                lcd.DrawString(x + 4 * 24 + 24, vy + 14, "[V]", kPhosphorDim, kCardFill, kFont16x32);
                DrawScanlines(lcd, x - 2, vy - 2, 169, 74);
                DrawSegmentedBar(lcd, x, vy + 52, 20, 6, 12, 2, port.bus_voltage_ / 24.0f, kBarHi);
                old_v[i] = port.bus_voltage_;
                dirty = true;
            }
            // 电流
            if (port.current_ != old_a[i])
            {
                lcd.FillRect(x - 2, ay - 2, 164, 74, kCardFill);
                snprintf(buf, sizeof(buf), "%.3f", port.current_);
                lcd.DrawString(x, ay, buf, kPhosphor, kCardFill, kFont24x48);
                lcd.DrawString(x + 4 * 24 + 24, ay + 14, "[A]", kPhosphorDim, kCardFill, kFont16x32);
                DrawScanlines(lcd, x - 2, ay - 2, 169, 74);
                DrawSegmentedBar(lcd, x, ay + 52, 20, 6, 12, 2, port.current_ / 7.0f, kBarHi);
                old_a[i] = port.current_;
                dirty = true;
            }
            // 功率
            if (port.power_ != old_w[i])
            {
                lcd.FillRect(x - 2, wy - 2, 164, 74, kCardFill);
                snprintf(buf, sizeof(buf), "%.2f", port.power_);
                lcd.DrawString(x, wy, buf, kPhosphor, kCardFill, kFont24x48);
                lcd.DrawString(x + 4 * 24 + 24, wy + 14, "[W]", kPhosphorDim, kCardFill, kFont16x32);
                DrawScanlines(lcd, x - 2, wy - 2, 169, 74);
                DrawSegmentedBar(lcd, x, wy + 52, 20, 6, 12, 2, port.power_ / 140.0f, kBarHi);
                old_w[i] = port.power_;
                dirty = true;
            }
        }

        if (dirty)
            lcd.Flush(panel_);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void SceneManager::MonitorListener()
{
    QueueHandle_t q = xQueueCreate(1, sizeof(PowerMonitor::Event *));
    PowerMonitor::GetInstance().RegisterListener(q);
    while (true)
    {
        xQueueReceive(q, &data_, portMAX_DELAY);
    }
}
