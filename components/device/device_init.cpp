#include "device_init.h"

#include "i2c_bus.h"

static const char *TAG = "Device";

void DeviceInit::Init()
{
    if (!I2CBusManager::GetInstance().Init())
    {
        ESP_LOGE(TAG, "I2C bus init failed");
        return;
    }

    if (!I2CBusManager::GetInstance().RegisterINA226(0x40))
    {
        ESP_LOGE(TAG, "INA226 0x40 register failed");
    }
    if (!I2CBusManager::GetInstance().RegisterINA226(0x41))
    {
        ESP_LOGE(TAG, "INA226 0x41 register failed");
    }
    if (!I2CBusManager::GetInstance().RegisterINA226(0x42))
    {
        ESP_LOGE(TAG, "INA226 0x42 register failed");
    }
    if (!I2CBusManager::GetInstance().RegisterINA226(0x44))
    {
        ESP_LOGE(TAG, "INA226 0x44 register failed");
    }
    if (!I2CBusManager::GetInstance().RegisterINA226(0x45))
    {
        ESP_LOGE(TAG, "INA226 0x45 register failed");
    }
}