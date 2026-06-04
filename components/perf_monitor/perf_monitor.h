#pragma once

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PERF_MAX_LISTENERS 10

class PerfMonitor
{
public:
    static PerfMonitor &GetInstance()
    {
        static PerfMonitor instance;
        return instance;
    }

    struct Event
    {
        uint8_t cpu0_ = 0;  // CPU0 使用率 0~100%
        uint8_t cpu1_ = 0;  // CPU1 使用率 0~100%
        uint32_t free_heap_ = 0;
    };

    void Init(uint32_t interval_ms = 1000);

    bool RegisterListener(QueueHandle_t queue);
    bool UnregisterListener(QueueHandle_t queue);

private:
    PerfMonitor() = default;
    ~PerfMonitor() = default;

    static void TaskFunc(void *);
    void Run();

    Event event_;
    QueueHandle_t listeners_[PERF_MAX_LISTENERS] = {};
    uint8_t listener_count_ = 0;
    uint32_t interval_ms_ = 1000;
    bool initialized_ = false;
};
