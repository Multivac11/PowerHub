#pragma once

#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd_driver.h"
#include "lcd_rgb.h"
#include "power_monitor.h"

class SceneManager
{
   public:
    static SceneManager& GetInstance()
    {
        static SceneManager instance;
        return instance;
    }

    SceneManager() = default;
    ~SceneManager() = default;

    void SceneManagerInit();

    static void UIManagerTask(void*);

    static void MonitorListenerTask(void*);

    void UIManager();

    void MonitorListener();

   private:
    esp_lcd_panel_handle_t panel_ = nullptr;
    uint16_t* buf_ = nullptr;
    LcdDriver* lcd_ = nullptr;
    PowerMonitor::Event* data_ = nullptr;
};
