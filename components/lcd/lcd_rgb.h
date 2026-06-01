#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ================== ST7701S 3-line 9-bit SPI 引脚 ==================
#define LCD_SPI_HOST SPI2_HOST
#define LCD_SPI_CS_GPIO GPIO_NUM_1
#define LCD_SPI_SCL_GPIO GPIO_NUM_2
#define LCD_SPI_SDA_GPIO GPIO_NUM_42

// ================== RGB 控制引脚 ==================
#define LCD_RGB_BL_GPIO GPIO_NUM_7
#define LCD_RGB_HSYNC_GPIO GPIO_NUM_8
#define LCD_RGB_VSYNC_GPIO GPIO_NUM_18
#define LCD_RGB_PCLK_GPIO GPIO_NUM_19
#define LCD_RGB_DE_GPIO GPIO_NUM_20

// ================== RGB565 数据引脚 (LSB -> MSB) ==================
#define LCD_RGB_B0_GPIO GPIO_NUM_41
#define LCD_RGB_B1_GPIO GPIO_NUM_40
#define LCD_RGB_B2_GPIO GPIO_NUM_38
#define LCD_RGB_B3_GPIO GPIO_NUM_39
#define LCD_RGB_B4_GPIO GPIO_NUM_45

#define LCD_RGB_G0_GPIO GPIO_NUM_48
#define LCD_RGB_G1_GPIO GPIO_NUM_47
#define LCD_RGB_G2_GPIO GPIO_NUM_21
#define LCD_RGB_G3_GPIO GPIO_NUM_14
#define LCD_RGB_G4_GPIO GPIO_NUM_13
#define LCD_RGB_G5_GPIO GPIO_NUM_12

#define LCD_RGB_R0_GPIO GPIO_NUM_11
#define LCD_RGB_R1_GPIO GPIO_NUM_10
#define LCD_RGB_R2_GPIO GPIO_NUM_9
#define LCD_RGB_R3_GPIO GPIO_NUM_46
#define LCD_RGB_R4_GPIO GPIO_NUM_3

// ================== 屏幕时序参数 (376x960) ==================
#define LCD_H_RES 376
#define LCD_V_RES 960
#define LCD_PCLK_MHZ 20

#define LCD_HSYNC_PULSE 2
#define LCD_HSYNC_BACK 4
#define LCD_HSYNC_FRONT 12

#define LCD_VSYNC_PULSE 4
#define LCD_VSYNC_BACK 4
#define LCD_VSYNC_FRONT 19

class LcdRgb
{
   public:
    static LcdRgb& GetInstance()
    {
        static LcdRgb instance;
        return instance;
    }

    ~LcdRgb();

    esp_err_t LcdInit();
    void LvglPortInit();
    void SetBacklight(bool on);
    esp_lcd_panel_handle_t GetPanel() const { return panel_; }

   private:
    LcdRgb() = default;

    void InitSpi();
    void SpiWrite(bool dc, uint8_t data);
    void WriteCmd(uint8_t cmd) { SpiWrite(false, cmd); }
    void WriteData(uint8_t data) { SpiWrite(true, data); }
    void SendInitSequence();
    esp_err_t InitRgbPanel();

   private:
    spi_device_handle_t spi_dev_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    lv_display_t* lvgl_display_ = nullptr;
};