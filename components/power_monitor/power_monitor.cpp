#include "power_monitor.h"

static const char *TAG = "PowerMonitor";

void PowerMonitor::PowerMonitorInit()
{
    event_.ina_data_[0].ina_ = I2CBusManager::GetInstance().GetDeviceByAddr<INA226>(0x40);
    if (event_.ina_data_[0].ina_ != nullptr)
    {
        event_.ina_data_[0].ina_->Configure(0x056F);
        event_.ina_data_[0].ina_->SetShuntResistor(0.003f, 19.0f);
        event_.ina_data_[0].not_found_ = false;
    }
    else
    {
        ESP_LOGE(TAG, "INA226 0x40 not found");
        event_.ina_data_[0].not_found_ = true;
    }

    event_.ina_data_[1].ina_ = I2CBusManager::GetInstance().GetDeviceByAddr<INA226>(0x41);
    if (event_.ina_data_[1].ina_ != nullptr)
    {
        event_.ina_data_[1].ina_->Configure(0x056F);
        event_.ina_data_[1].ina_->SetShuntResistor(0.003f, 19.0f);
        event_.ina_data_[1].not_found_ = false;
    }
    else
    {
        ESP_LOGE(TAG, "INA226 0x41 not found");
        event_.ina_data_[1].not_found_ = true;
    }

    event_.ina_data_[2].ina_ = I2CBusManager::GetInstance().GetDeviceByAddr<INA226>(0x44);
    if (event_.ina_data_[2].ina_ != nullptr)
    {
        event_.ina_data_[2].ina_->Configure(0x056F);
        event_.ina_data_[2].ina_->SetShuntResistor(0.003f, 19.0f);
        event_.ina_data_[2].not_found_ = false;
    }
    else
    {
        ESP_LOGE(TAG, "INA226 0x44 not found");
        event_.ina_data_[2].not_found_ = true;
    }

    event_.ina_data_[3].ina_ = I2CBusManager::GetInstance().GetDeviceByAddr<INA226>(0x45);
    if (event_.ina_data_[3].ina_ != nullptr)
    {
        event_.ina_data_[3].ina_->Configure(0x056F);
        event_.ina_data_[3].ina_->SetShuntResistor(0.003f, 19.0f);
        event_.ina_data_[3].not_found_ = false;
    }
    else
    {
        ESP_LOGE(TAG, "INA226 0x45 not found");
        event_.ina_data_[3].not_found_ = true;
    }

    event_.ina_data_[4].ina_ = I2CBusManager::GetInstance().GetDeviceByAddr<INA226>(0x42);
    if (event_.ina_data_[4].ina_ != nullptr)
    {
        event_.ina_data_[4].ina_->Configure(0x056F);
        event_.ina_data_[4].ina_->SetShuntResistor(0.003f, 19.0f);
        event_.ina_data_[4].not_found_ = false;
    }
    else
    {
        ESP_LOGE(TAG, "INA226 0x42 not found");
        event_.ina_data_[4].not_found_ = true;
    }

    tca9535_ = I2CBusManager::GetInstance().GetDeviceByAddr<TCA9535>(0x20);
    if (tca9535_ != nullptr)
    {
        // P00~P04 配置为输出（0=输出），P05~P07 保持输入（1=输入），Port 1 全部保持输入
        tca9535_->SetDirection(0, 0xE0);
        tca9535_->SetDirection(1, 0xFF);
        // 默认全部输出低电平（关断）
        tca9535_->WriteOutput(0, 0x00);
        ESP_LOGI(TAG, "TCA9535 init OK, P00~P04 as output, default LOW");
    }
    else
    {
        ESP_LOGE(TAG, "TCA9535 0x20 not found");
    }

    key_queue_ = xQueueCreate(1, sizeof(StatusKey::Event *));
    StatusKey::GetInstance().RegisterListener(key_queue_);

    ESP_LOGI(TAG, "PowerMonitorInit");

    xTaskCreatePinnedToCore(PowerMonitorTask, "MonitorTask", 8192, this, 2, nullptr, 1);
}

void PowerMonitor::PowerMonitorTask(void *pvParameters)
{
    static_cast<PowerMonitor *>(pvParameters)->Monitor();
}

void PowerMonitor::Monitor()
{
    while (true)
    {

        for (int i = 0; i < MAX_INA; i++)
        {
            if (!event_.ina_data_[i].not_found_)
            {
                event_.ina_data_[i].bus_voltage_ = event_.ina_data_[i].ina_->ReadBusVoltage();
                if (event_.ina_data_[i].ina_->ReadCurrent() < 0.0f)
                {
                    event_.ina_data_[i].current_ = -event_.ina_data_[i].ina_->ReadCurrent();
                }
                else
                {
                    event_.ina_data_[i].current_ = event_.ina_data_[i].ina_->ReadCurrent();
                }
                event_.ina_data_[i].power_ = event_.ina_data_[i].ina_->ReadPower();
            }
            // 同步通道开关状态到 UI 可见数据
            event_.ina_data_[i].enabled_ = channel_state_[i];
        }

        for (int i = 0; i < listener_count_; ++i)
        {
            if (listeners_[i])
            {
                Event *p = &event_;
                xQueueOverwrite(listeners_[i], &p);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(70));
    }
}

bool PowerMonitor::RegisterListener(QueueHandle_t queue)
{
    if (queue == nullptr || listener_count_ >= MAX_LISTENERS)
        return false;

    for (int i = 0; i < listener_count_; ++i)
    {
        if (listeners_[i] == queue)
            return true; // 已存在，直接返回成功
    }

    listeners_[listener_count_++] = queue;
    return true;
}

bool PowerMonitor::UnregisterListener(QueueHandle_t queue)
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

/* ---------- 通道输出控制 ---------- */

bool PowerMonitor::SetChannelOutput(uint8_t ch, bool on)
{
    if (ch >= MAX_INA || tca9535_ == nullptr)
        return false;
    if (tca9535_->DigitalWrite(ch, on))
    {
        channel_state_[ch] = on;
        return true;
    }
    return false;
}

bool PowerMonitor::EnableChannel(uint8_t ch)
{
    return SetChannelOutput(ch, true);
}

bool PowerMonitor::DisableChannel(uint8_t ch)
{
    return SetChannelOutput(ch, false);
}

/* ---------- 按键监听 ---------- */
