#include "lcd_rgb.h"

#include "esp_check.h"

static const char *TAG = "LcdRgb";

LcdRgb::~LcdRgb()
{
    if (spi_dev_) spi_bus_remove_device(spi_dev_);
    spi_bus_free(LCD_SPI_HOST);
}

// ================== 硬件 SPI 9-bit 初始化 ==================
void LcdRgb::InitSpi()
{
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = LCD_SPI_SDA_GPIO;
    buscfg.miso_io_num = -1;
    buscfg.sclk_io_num = LCD_SPI_SCL_GPIO;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 4;

    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_DISABLED));

    // command_bits=1 做 D/C，address_bits=8 做数据，组成 9bit
    spi_device_interface_config_t devcfg = {};
    devcfg.command_bits = 1;
    devcfg.address_bits = 8;
    devcfg.mode = 0;                           // CPOL=0, CPHA=0
    devcfg.clock_speed_hz = 10 * 1000 * 1000;  // 10MHz，稳定优先
    devcfg.spics_io_num = LCD_SPI_CS_GPIO;
    devcfg.queue_size = 1;

    ESP_ERROR_CHECK(spi_bus_add_device(LCD_SPI_HOST, &devcfg, &spi_dev_));
}

// ================== 硬件 SPI 发送 9bit ==================
void LcdRgb::SpiWrite(bool dc, uint8_t data)
{
    spi_transaction_t t = {};
    t.cmd = dc ? 1 : 0;  // D/C: 0=命令, 1=数据
    t.addr = data;       // 8bit 内容
    ESP_ERROR_CHECK(spi_device_transmit(spi_dev_, &t));
}

// ================== 飞雄光电 F318B30-20 厂家序列 ==================
void LcdRgb::SendInitSequence()
{
    vTaskDelay(pdMS_TO_TICKS(200));  // RST接EN，多等一会

    // Page 3
    WriteCmd(0xFF);
    WriteData(0x77);
    WriteData(0x01);
    WriteData(0x00);
    WriteData(0x00);
    WriteData(0x13);
    WriteCmd(0xEF);
    WriteData(0x08);

    // Page 0
    WriteCmd(0xFF);
    WriteData(0x77);
    WriteData(0x01);
    WriteData(0x00);
    WriteData(0x00);
    WriteData(0x10);

    WriteCmd(0xC0);
    WriteData(0x77);
    WriteData(0x00);
    WriteCmd(0xC1);
    WriteData(0x0C);
    WriteData(0x0C);
    WriteCmd(0xC2);
    WriteData(0x27);
    WriteData(0x0A);
    WriteCmd(0xCC);
    WriteData(0x10);

    // Positive Gamma
    WriteCmd(0xB0);
    WriteData(0x00);
    WriteData(0x0C);
    WriteData(0x19);
    WriteData(0x0B);
    WriteData(0x0F);
    WriteData(0x06);
    WriteData(0x05);
    WriteData(0x08);
    WriteData(0x08);
    WriteData(0x1F);
    WriteData(0x04);
    WriteData(0x11);
    WriteData(0x0F);
    WriteData(0x26);
    WriteData(0x2F);
    WriteData(0x1D);

    // Negative Gamma
    WriteCmd(0xB1);
    WriteData(0x00);
    WriteData(0x17);
    WriteData(0x19);
    WriteData(0x0F);
    WriteData(0x12);
    WriteData(0x05);
    WriteData(0x05);
    WriteData(0x08);
    WriteData(0x07);
    WriteData(0x1F);
    WriteData(0x03);
    WriteData(0x10);
    WriteData(0x10);
    WriteData(0x27);
    WriteData(0x2F);
    WriteData(0x1D);

    // Page 1
    WriteCmd(0xFF);
    WriteData(0x77);
    WriteData(0x01);
    WriteData(0x00);
    WriteData(0x00);
    WriteData(0x11);

    WriteCmd(0xB0);
    WriteData(0x25);
    WriteCmd(0xB1);
    WriteData(0x76);
    WriteCmd(0xB2);
    WriteData(0x81);
    WriteCmd(0xB3);
    WriteData(0x80);
    WriteCmd(0xB5);
    WriteData(0x4E);
    WriteCmd(0xB7);
    WriteData(0x85);
    WriteCmd(0xB8);
    WriteData(0x20);
    WriteCmd(0xC1);
    WriteData(0x78);
    WriteCmd(0xC2);
    WriteData(0x78);
    WriteCmd(0xD0);
    WriteData(0x88);

    // Page 0 电源
    WriteCmd(0xE0);
    WriteData(0x00);
    WriteData(0x00);
    WriteData(0x02);

    WriteCmd(0xE1);
    WriteData(0x02);
    WriteData(0x8C);
    WriteData(0x04);
    WriteData(0x8C);
    WriteData(0x01);
    WriteData(0x8C);
    WriteData(0x03);
    WriteData(0x8C);
    WriteData(0x00);
    WriteData(0x44);
    WriteData(0x44);

    WriteCmd(0xE2);
    WriteData(0x00);
    WriteData(0x00);
    WriteData(0x00);
    WriteData(0x00);
    WriteData(0x00);
    WriteData(0x00);
    WriteData(0x00);
    WriteData(0x00);
    WriteData(0x00);
    WriteData(0x00);
    WriteData(0x00);
    WriteData(0x00);

    WriteCmd(0xE3);
    WriteData(0x00);
    WriteData(0x00);
    WriteData(0x33);
    WriteData(0x33);
    WriteCmd(0xE4);
    WriteData(0x44);
    WriteData(0x44);

    WriteCmd(0xE5);
    WriteData(0x09);
    WriteData(0xD2);
    WriteData(0x35);
    WriteData(0x8C);
    WriteData(0x0B);
    WriteData(0xD4);
    WriteData(0x35);
    WriteData(0x8C);
    WriteData(0x05);
    WriteData(0xCE);
    WriteData(0x35);
    WriteData(0x8C);
    WriteData(0x07);
    WriteData(0xD0);
    WriteData(0x35);
    WriteData(0x8C);

    WriteCmd(0xE6);
    WriteData(0x00);
    WriteData(0x00);
    WriteData(0x33);
    WriteData(0x33);
    WriteCmd(0xE7);
    WriteData(0x44);
    WriteData(0x44);

    WriteCmd(0xE8);
    WriteData(0x08);
    WriteData(0xD1);
    WriteData(0x35);
    WriteData(0x8C);
    WriteData(0x0A);
    WriteData(0xD3);
    WriteData(0x35);
    WriteData(0x8C);
    WriteData(0x04);
    WriteData(0xCD);
    WriteData(0x35);
    WriteData(0x8C);
    WriteData(0x06);
    WriteData(0xCF);
    WriteData(0x35);
    WriteData(0x8C);

    WriteCmd(0xEB);
    WriteData(0x00);
    WriteData(0x01);
    WriteData(0xE4);
    WriteData(0xE4);
    WriteData(0x44);
    WriteData(0x00);

    WriteCmd(0xED);
    WriteData(0x77);
    WriteData(0x66);
    WriteData(0x55);
    WriteData(0x44);
    WriteData(0xCA);
    WriteData(0xF1);
    WriteData(0x03);
    WriteData(0xBF);
    WriteData(0xFB);
    WriteData(0x30);
    WriteData(0x1F);
    WriteData(0xAC);
    WriteData(0x44);
    WriteData(0x55);
    WriteData(0x66);
    WriteData(0x77);

    WriteCmd(0xEF);
    WriteData(0x10);
    WriteData(0x0D);
    WriteData(0x04);
    WriteData(0x08);
    WriteData(0x3F);
    WriteData(0x1F);

    // Page 0 显示设置
    WriteCmd(0xFF);
    WriteData(0x77);
    WriteData(0x01);
    WriteData(0x00);
    WriteData(0x00);
    WriteData(0x00);

    // 关键：16bit 硬件接线必须用 0x50（RGB565）
    WriteCmd(0x3A);
    WriteData(0x50);

    WriteCmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    WriteCmd(0x29);
    WriteCmd(0x36);
    WriteData(0x00);

    ESP_LOGI(TAG, "F318B30-20 init done");
}

esp_err_t LcdRgb::InitRgbPanel()
{
    esp_lcd_rgb_panel_config_t panel_config = {};
    panel_config.clk_src = LCD_CLK_SRC_PLL240M;

    panel_config.timings.pclk_hz = LCD_PCLK_MHZ * 1000 * 1000;
    panel_config.timings.h_res = LCD_H_RES;
    panel_config.timings.v_res = LCD_V_RES;
    panel_config.timings.hsync_pulse_width = LCD_HSYNC_PULSE;
    panel_config.timings.hsync_back_porch = LCD_HSYNC_BACK;
    panel_config.timings.hsync_front_porch = LCD_HSYNC_FRONT;
    panel_config.timings.vsync_pulse_width = LCD_VSYNC_PULSE;
    panel_config.timings.vsync_back_porch = LCD_VSYNC_BACK;
    panel_config.timings.vsync_front_porch = LCD_VSYNC_FRONT;

    // 关键：按博客验证的参数配置（与之前版本相反！）
    panel_config.timings.flags.hsync_idle_low = false;   // 空闲高电平
    panel_config.timings.flags.vsync_idle_low = false;   // 空闲高电平
    panel_config.timings.flags.de_idle_high = false;     // 空闲低电平
    panel_config.timings.flags.pclk_active_neg = false;  // 上升沿有效
    panel_config.timings.flags.pclk_idle_high = false;   // 空闲高电平

    panel_config.data_width = 16;
    panel_config.bits_per_pixel = 16;
    panel_config.num_fbs = 2;
    panel_config.bounce_buffer_size_px = LCD_H_RES * 10;  // 关闭 bounce buffer
    panel_config.sram_trans_align = 4;
    panel_config.psram_trans_align = 64;

    panel_config.hsync_gpio_num = LCD_RGB_HSYNC_GPIO;
    panel_config.vsync_gpio_num = LCD_RGB_VSYNC_GPIO;
    panel_config.de_gpio_num = LCD_RGB_DE_GPIO;
    panel_config.pclk_gpio_num = LCD_RGB_PCLK_GPIO;
    panel_config.disp_gpio_num = GPIO_NUM_NC;

    panel_config.data_gpio_nums[0] = LCD_RGB_B0_GPIO;
    panel_config.data_gpio_nums[1] = LCD_RGB_B1_GPIO;
    panel_config.data_gpio_nums[2] = LCD_RGB_B2_GPIO;
    panel_config.data_gpio_nums[3] = LCD_RGB_B3_GPIO;
    panel_config.data_gpio_nums[4] = LCD_RGB_B4_GPIO;
    panel_config.data_gpio_nums[5] = LCD_RGB_G0_GPIO;
    panel_config.data_gpio_nums[6] = LCD_RGB_G1_GPIO;
    panel_config.data_gpio_nums[7] = LCD_RGB_G2_GPIO;
    panel_config.data_gpio_nums[8] = LCD_RGB_G3_GPIO;
    panel_config.data_gpio_nums[9] = LCD_RGB_G4_GPIO;
    panel_config.data_gpio_nums[10] = LCD_RGB_G5_GPIO;
    panel_config.data_gpio_nums[11] = LCD_RGB_R0_GPIO;
    panel_config.data_gpio_nums[12] = LCD_RGB_R1_GPIO;
    panel_config.data_gpio_nums[13] = LCD_RGB_R2_GPIO;
    panel_config.data_gpio_nums[14] = LCD_RGB_R3_GPIO;
    panel_config.data_gpio_nums[15] = LCD_RGB_R4_GPIO;

    panel_config.flags.fb_in_psram = true;
    panel_config.flags.refresh_on_demand = false;
    panel_config.flags.fb_in_psram = true;

    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&panel_config, &panel_), TAG, "new rgb panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel_), TAG, "panel init failed");

    return ESP_OK;
}

esp_err_t LcdRgb::LcdInit()
{
    ESP_LOGI(TAG, "Init SPI...");
    InitSpi();

    ESP_LOGI(TAG, "Send init sequence...");
    SendInitSequence();

    ESP_LOGI(TAG, "Init RGB panel...");
    ESP_RETURN_ON_ERROR(InitRgbPanel(), TAG, "RGB panel init failed");

    gpio_config_t bl_conf = {
        .pin_bit_mask = (1ULL << LCD_RGB_BL_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&bl_conf);

    SetBacklight(true);
    ESP_LOGI(TAG, "LCD init done, %dx%d", LCD_H_RES, LCD_V_RES);
    return ESP_OK;
}

void LcdRgb::SetBacklight(bool on)
{
    gpio_set_level(LCD_RGB_BL_GPIO, on ? 1 : 0);
    ESP_LOGI(TAG, "Backlight %s", on ? "ON" : "OFF");
}

void LcdRgb::LvglPortInit()
{
    LcdInit();
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 5,       /* LVGL task priority */
        .task_stack = 16384,      /* LVGL task stack size */
        .task_affinity = 0,       /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500, /* Maximum sleep in LVGL task */
        .timer_period_ms = 20     /* LVGL timer tick period in ms */
    };
    lvgl_port_init(&lvgl_cfg);

    ESP_LOGD(TAG, "Add LCD screen");
    lvgl_port_display_cfg_t disp_cfg = {};
    disp_cfg.panel_handle = panel_;
    disp_cfg.buffer_size = 10 * LCD_V_RES;
    disp_cfg.double_buffer = 1;
    disp_cfg.hres = LCD_H_RES;
    disp_cfg.vres = LCD_V_RES;
    disp_cfg.monochrome = false;
    disp_cfg.color_format = LV_COLOR_FORMAT_RGB565;
    disp_cfg.rotation = {
        .swap_xy = true,
        .mirror_x = true,
        .mirror_y = false,
    };

    disp_cfg.flags.buff_dma = true;
    disp_cfg.flags.buff_spiram = true;
    disp_cfg.flags.sw_rotate = false;
    disp_cfg.flags.full_refresh = false;
    disp_cfg.flags.direct_mode = true;
    disp_cfg.flags.swap_bytes = false;

    lvgl_port_display_rgb_cfg_t rgb_cfg = {.flags = {
                                               .bb_mode = true,
                                               .avoid_tearing = true,

                                           }};
    lvgl_display_ = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);
    // lv_display_set_rotation(lvgl_display_, LV_DISPLAY_ROTATION_90);
}