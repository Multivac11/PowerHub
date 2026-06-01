#pragma once

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"
#include "key.h"

#define MAX_LISTENERS 10
#define MAX_INA 5

class PowerMonitor
{
public:
    static PowerMonitor &GetInstance()
    {
        static PowerMonitor instance;
        return instance;
    }

    struct MonitorData
    {
        INA226 *ina_ = nullptr;
        float bus_voltage_ = 0.0f;
        float shunt_voltage_ = 0.0f;
        float current_ = 0.0f;
        float power_ = 0.0f;
        bool not_found_ = true;
        bool enabled_ = false;
    };

    struct Event
    {
        MonitorData ina_data_[MAX_INA];
    };

    PowerMonitor() = default;

    ~PowerMonitor() = default;

    void PowerMonitorInit();

    static void PowerMonitorTask(void *);

    void Monitor();

    // 通道输出控制（通过 TCA9535 P00~P04）
    bool EnableChannel(uint8_t ch); // 开启通道输出（对应引脚拉高）

    bool DisableChannel(uint8_t ch); // 关闭通道输出（对应引脚拉低）

    bool SetChannelOutput(uint8_t ch, bool on);

    bool RegisterListener(QueueHandle_t queue);

    bool UnregisterListener(QueueHandle_t queue);

private:
    MonitorData ina_data_[MAX_INA] = {0};

    Event event_;

    QueueHandle_t listeners_[MAX_LISTENERS] = {};

    uint8_t listener_count_ = 0;

    TCA9535 *tca9535_ = nullptr;

    QueueHandle_t key_queue_ = nullptr;

    bool channel_state_[MAX_INA] = {false};
};
