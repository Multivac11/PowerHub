#include "perf_monitor.h"

#include "esp_timer.h"

static const char *TAG = "Perf";

void PerfMonitor::Init(uint32_t interval_ms)
{
    if (initialized_)
    {
        ESP_LOGW(TAG, "Already initialized");
        return;
    }
    initialized_ = true;
    interval_ms_ = interval_ms;

    xTaskCreatePinnedToCore(TaskFunc, "PerfTask", 4096, this, 1, nullptr, 1);

    ESP_LOGI(TAG, "Init OK, interval=%" PRIu32 "ms", interval_ms_);
}

void PerfMonitor::TaskFunc(void *arg)
{
    static_cast<PerfMonitor *>(arg)->Run();
}

bool PerfMonitor::RegisterListener(QueueHandle_t queue)
{
    if (queue == nullptr || listener_count_ >= PERF_MAX_LISTENERS)
        return false;

    for (int i = 0; i < listener_count_; ++i)
    {
        if (listeners_[i] == queue)
            return true; // 已存在，直接返回成功
    }

    listeners_[listener_count_++] = queue;
    return true;
}

bool PerfMonitor::UnregisterListener(QueueHandle_t queue)
{
    for (int i = 0; i < listener_count_; ++i)
    {
        if (listeners_[i] == queue)
        {
            for (int j = i; j < listener_count_ - 1; ++j)
                listeners_[j] = listeners_[j + 1];
            listeners_[--listener_count_] = nullptr;
            return true;
        }
    }
    return false;
}

void PerfMonitor::Run()
{
    vTaskDelay(pdMS_TO_TICKS(2000)); // 等待系统稳定

    // 直接获取两个核的空闲任务句柄
    TaskHandle_t idle0 = xTaskGetIdleTaskHandleForCore(0);
    TaskHandle_t idle1 = xTaskGetIdleTaskHandleForCore(1);

    TaskStatus_t st0, st1;

    // 首次采样
    vTaskGetInfo(idle0, &st0, pdFALSE, eInvalid);
    vTaskGetInfo(idle1, &st1, pdFALSE, eInvalid);
    uint32_t prev_idle0 = st0.ulRunTimeCounter;
    uint32_t prev_idle1 = st1.ulRunTimeCounter;
    int64_t prev_time = esp_timer_get_time();

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(interval_ms_));
        int64_t now_time = esp_timer_get_time();
        int64_t elapsed = now_time - prev_time;
        prev_time = now_time;

        // 单任务查询，临界区比 uxTaskGetSystemState 短得多
        vTaskGetInfo(idle0, &st0, pdFALSE, eInvalid);
        vTaskGetInfo(idle1, &st1, pdFALSE, eInvalid);
        uint32_t now_idle0 = st0.ulRunTimeCounter;
        uint32_t now_idle1 = st1.ulRunTimeCounter;

        uint32_t d0 = now_idle0 - prev_idle0;
        uint32_t d1 = now_idle1 - prev_idle1;
        prev_idle0 = now_idle0;
        prev_idle1 = now_idle1;

        if (elapsed <= 0)
            elapsed = interval_ms_ * 1000ULL;

        int cpu0 = 100 - (int)((uint64_t)d0 * 100ULL / (uint64_t)elapsed);
        int cpu1 = 100 - (int)((uint64_t)d1 * 100ULL / (uint64_t)elapsed);
        if (cpu0 < 0)
            cpu0 = 0;
        if (cpu1 < 0)
            cpu1 = 0;
        if (cpu0 > 100)
            cpu0 = 100;
        if (cpu1 > 100)
            cpu1 = 100;

        // 填充 Event 并发布给所有监听者
        event_.cpu0_ = (uint8_t)cpu0;
        event_.cpu1_ = (uint8_t)cpu1;
        event_.free_heap_ = esp_get_free_heap_size();

        for (int i = 0; i < listener_count_; ++i)
        {
            if (listeners_[i])
            {
                Event *p = &event_;
                xQueueOverwrite(listeners_[i], &p);
            }
        }
    }
}
