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
    if (!I2CBusManager::GetInstance().RegisterINA226(0x44))
    {
        ESP_LOGE(TAG, "INA226 0x44 register failed");
    }
    if (!I2CBusManager::GetInstance().RegisterINA226(0x45))
    {
        ESP_LOGE(TAG, "INA226 0x45 register failed");
    }
    if (!I2CBusManager::GetInstance().RegisterINA226(0x42))
    {
        ESP_LOGE(TAG, "INA226 0x42 register failed");
    }

    if (!I2CBusManager::GetInstance().RegisterTCA9535(0x20))
    {
        ESP_LOGE(TAG, "TCA9535 0x20 register failed");
    }

    if (!I2CBusManager::GetInstance().RegisterMP4201(0x3F))
    {
        ESP_LOGE(TAG, "MP4201 0x3F register failed");
    }

    // MCP4725 DAC ①: A0=GND → 0x60 → MP4201 FREQ 引脚（开关频率）
    if (!I2CBusManager::GetInstance().RegisterMCP4725(0x60))
    {
        ESP_LOGE(TAG, "MCP4725 FREQ (0x60) register failed");
    }

    // MCP4725 DAC ②: A0=VDD → 0x61 → MP4201 MODE 引脚（PFM/FCCM 选择）
    if (!I2CBusManager::GetInstance().RegisterMCP4725(0x61))
    {
        ESP_LOGE(TAG, "MCP4725 MODE (0x61) register failed");
    }
}