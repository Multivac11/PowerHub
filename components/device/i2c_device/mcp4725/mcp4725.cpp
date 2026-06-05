#include "mcp4725.h"

#include <cmath>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "MCP4725";

MCP4725::MCP4725(i2c_master_bus_handle_t bus, uint16_t addr) : I2CDevice(bus, addr, TAG)
{
}

bool MCP4725::Init()
{
    if (!I2CDevice::Init())
    {
        return false;
    }

    ESP_LOGI(TAG, "Init MCP4725 success");
    return true;
}

// ====== 3 字节写（Figure 6-2: Write DAC Register / Write DAC + EEPROM） ======
// 命令字节: [C2 C1 C0 X  X  PD1 PD0 X]
// 数据高字节: [D11 D10 D9 D8 D7 D6 D5 D4]
// 数据低字节: [D3  D2  D1 D0 X  X  X  X ]
esp_err_t MCP4725::WriteVolatile(uint16_t code, PowerDownMode pd)
{
    if (code > MAX_CODE)
    {
        code = MAX_CODE;
    }

    // C2:C1:C0 = 010 → 仅写 DAC 寄存器
    uint8_t byte1 = 0x40 | (static_cast<uint8_t>(pd) << 1);    // [0 1 0 0 0 PD1 PD0 0]
    uint8_t byte2 = (code >> 4) & 0xFF;                         // D11..D4
    uint8_t byte3 = (code & 0x0F) << 4;                         // D3..D0

    uint8_t buf[] = {byte1, byte2, byte3};
    return Write(buf, sizeof(buf));
}

// C2:C1:C0 = 011 → 写 DAC 寄存器 + EEPROM（需等待 ~25ms）
esp_err_t MCP4725::WriteEEPROM(uint16_t code, PowerDownMode pd)
{
    if (code > MAX_CODE)
    {
        code = MAX_CODE;
    }

    uint8_t byte1 = 0x60 | (static_cast<uint8_t>(pd) << 1);    // [0 1 1 0 0 PD1 PD0 0]
    uint8_t byte2 = (code >> 4) & 0xFF;                         // D11..D4
    uint8_t byte3 = (code & 0x0F) << 4;                         // D3..D0

    uint8_t buf[] = {byte1, byte2, byte3};
    return Write(buf, sizeof(buf));
}

// ====== 电压输出 ======

bool MCP4725::SetOutputVoltage(float voltage, float vdd)
{
    return SetOutputVoltage(voltage, vdd, PowerDownMode::NORMAL);
}

bool MCP4725::SetOutputVoltage(float voltage, float vdd, PowerDownMode pd)
{
    if (voltage < 0.0f)
    {
        voltage = 0.0f;
    }
    if (voltage > vdd)
    {
        voltage = vdd;
    }

    // Vout = (code / 4096) × VDD  →  code = Vout × 4096 / VDD
    uint16_t code = static_cast<uint16_t>(std::round(voltage * 4096.0f / vdd));
    return SetOutputRaw(code, pd);
}

// ====== 原始值输出 ======

bool MCP4725::SetOutputRaw(uint16_t code)
{
    return SetOutputRaw(code, PowerDownMode::NORMAL);
}

bool MCP4725::SetOutputRaw(uint16_t code, PowerDownMode pd)
{
    if (WriteVolatile(code, pd) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set DAC output");
        return false;
    }

    last_code_ = code;
    last_pd_ = pd;
    return true;
}

// ====== EEPROM ======

bool MCP4725::SaveToEEPROM()
{
    return SaveToEEPROM(last_code_, last_pd_);
}

bool MCP4725::SaveToEEPROM(uint16_t code, PowerDownMode pd)
{
    if (WriteEEPROM(code, pd) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write EEPROM");
        return false;
    }

    // EEPROM 写入时间约 25ms
    vTaskDelay(pdMS_TO_TICKS(30));

    ESP_LOGI(TAG, "EEPROM saved: code=%u", code);
    return true;
}

// ====== 掉电 / 唤醒 ======

bool MCP4725::PowerDown(PowerDownMode pd)
{
    if (pd == PowerDownMode::NORMAL)
    {
        return false;  // 用 WakeUp 恢复
    }

    uint16_t code = (last_pd_ == PowerDownMode::NORMAL) ? last_code_ : 0;
    return SetOutputRaw(code, pd);
}

bool MCP4725::WakeUp()
{
    return SetOutputRaw(last_code_, PowerDownMode::NORMAL);
}
