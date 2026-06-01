#include "key.h"

StatusKey::StatusKey(gpio_num_t p1, gpio_num_t p2, gpio_num_t p3, uint32_t lm) : longMs_(lm), queue_(nullptr)
{
    key_pin_[0] = p3;
    key_pin_[1] = p2;
    key_pin_[2] = p1;
}

void StatusKey::InitKeys()
{
    gpio_config_t gpio_cfg = {
        .pin_bit_mask = (1ULL << key_pin_[0]) | (1ULL << key_pin_[1]) | (1ULL << key_pin_[2]),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&gpio_cfg);

    queue_ = xQueueCreate(1, sizeof(Event));
    xTaskCreatePinnedToCore(GetKeyTask, "GetKeyTask", 4096, this, 2, nullptr, 1);
    ESP_LOGI("Key", "InitKeys successfull");
}

bool StatusKey::RegisterListener(QueueHandle_t queue)
{
    if (queue == nullptr || listener_count_ >= MAX_LISTENERS) return false;

    for (int i = 0; i < listener_count_; ++i)
    {
        if (listeners_[i] == queue) return true;  // 已存在，直接返回成功
    }

    listeners_[listener_count_++] = queue;
    return true;
}

bool StatusKey::UnregisterListener(QueueHandle_t queue)
{
    for (int i = 0; i < listener_count_; ++i)
    {
        if (listeners_[i] == queue)
        {
            for (int j = i; j < listener_count_ - 1; ++j) listeners_[j] = listeners_[j + 1];
            listeners_[--listener_count_] = nullptr;
            return true;
        }
    }
    return false;
}

void StatusKey::GetKeyTask(void *pvParameters)
{
    static_cast<StatusKey *>(pvParameters)->KeyStatus();
}

void StatusKey::KeyStatus()
{
    while (true)
    {
        ScanKeys();
    }
}

void StatusKey::ScanKeys()
{
    uint32_t now = esp_timer_get_time() / 1000;
    bool changed = false;

    for (int i = 0; i < 3; ++i)
    {
        bool down = gpio_get_level(key_pin_[i]) == 0;

        if (down)
        {  // ========== 按键被按住 ==========
            if (!buffer_[i].act)
            {  // 刚按下：初始化状态
                buffer_[i].act = true;
                buffer_[i].triggered = false;
                buffer_[i].t = now;
                ev_.key[i] = KEY_NONE;
            }
            else
            {  // 持续按住：实时判断时长
                uint32_t dt = now - buffer_[i].t;
                if (dt >= longMs_ && !buffer_[i].triggered)
                {  // 首次达到阈值 → 触发长按
                    ev_.key[i] = KEY_LONG;
                    buffer_[i].triggered = true;
                    changed = true;
                    ESP_LOGI("Key", "Key %d Long (hold %u ms)", key_pin_[i], dt);
                }
                else if (buffer_[i].triggered)
                {  // ← 新增：已经触发过，保持 LONG，不清除
                    ev_.key[i] = KEY_LONG;
                }
                else
                {  // 按着但还没达到阈值
                    ev_.key[i] = KEY_NONE;
                }
            }
        }
        else
        {  // ========== 按键松开 / 未按 ==========
            if (buffer_[i].act)
            {  // 之前是按下的，现在松开了
                if (!buffer_[i].triggered)
                {  // 未达到长按阈值 → 短按
                    ev_.key[i] = KEY_SHORT;
                    changed = true;
                    ESP_LOGI("Key", "Key %d Short", key_pin_[i]);
                }
                else
                {  // 长按松手 → 状态变回 NONE，通知监听者
                    ev_.key[i] = KEY_NONE;
                    changed = true;  // ← 新增：必须广播，否则监听者不知道松手了
                    ESP_LOGI("Key", "Key %d Long Release", key_pin_[i]);
                }
                buffer_[i].act = false;  // 清除按下状态
            }
            else
            {  // 一直未按
                ev_.key[i] = KEY_NONE;
            }
        }
    }

    if (changed)
    {
        for (int i = 0; i < listener_count_; ++i)
        {
            if (listeners_[i])
            {
                Event *p = &ev_;
                xQueueOverwrite(listeners_[i], &p);
            }
        }
    }

    vTaskDelay(pdMS_TO_TICKS(SCAN_MS));
}