#pragma once

#include <stdio.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAX_LISTENERS 10
#define SCAN_MS 20

class StatusKey
{
   public:
    static StatusKey &GetInstance()
    {
        static StatusKey instance;

        return instance;
    }

    StatusKey(gpio_num_t key_pin1 = GPIO_NUM_4,
              gpio_num_t key_pin2 = GPIO_NUM_5,
              gpio_num_t key_pin3 = GPIO_NUM_6,
              uint32_t longMs = 2000);

    ~StatusKey() = default;

    enum KeyStatusEnum
    {
        KEY_NONE = 0,
        KEY_SHORT = 1,
        KEY_LONG = 2,
        KEY_PRESSING = 3,
    };

    struct Event
    {
        KeyStatusEnum key[3];
    };

    void InitKeys();  // 启动任务

    bool RegisterListener(QueueHandle_t queue);

    bool UnregisterListener(QueueHandle_t queue);

   private:
    static void GetKeyTask(void *);  // 静态入口

    void ScanKeys();

    void KeyStatus();

   private:
    gpio_num_t key_pin_[3];

    uint32_t longMs_;

    QueueHandle_t queue_;

    Event ev_;  // 缓存

    struct KeyBuffer
    {
        bool act;        // 是否正在按下
        bool triggered;  // 是否已经触发过事件（新增）
        uint32_t t;      // 按下时刻
    };

    KeyBuffer buffer_[3] = {};

    QueueHandle_t listeners_[MAX_LISTENERS] = {};

    uint8_t listener_count_ = 0;
};