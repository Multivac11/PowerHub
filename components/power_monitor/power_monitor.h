#pragma once

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"

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
    };

    struct Event
    {
        MonitorData ina_data_[MAX_INA];
    };

    PowerMonitor() = default;

    ~PowerMonitor() = default;

    void PowerMonitorInit();

    static void PowerMonitorTask(void *);

    static void TempControlTask(void *);

    void Monitor();

    void TempControl();

    bool RegisterListener(QueueHandle_t queue);

    bool UnregisterListener(QueueHandle_t queue);

private:
    MonitorData ina_data_[MAX_INA] = {0};

    Event event_;

    QueueHandle_t listeners_[MAX_LISTENERS] = {};

    uint8_t listener_count_ = 0;
};
