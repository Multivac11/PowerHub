#pragma once

#include "i2c_device.h"

// MCP4725 — 12 位带 EEPROM 的轨到轨 DAC，I2C 接口
// 供电 2.7V–5.5V，VREF = VDD
class MCP4725 : public I2CDevice
{
   public:
    // 掉电模式（PD1:PD0）
    enum class PowerDownMode : uint8_t
    {
        NORMAL = 0,     // 正常输出
        RES_1K = 1,     // 掉电，1kΩ 对地
        RES_100K = 2,   // 掉电，100kΩ 对地
        RES_500K = 3,   // 掉电，500kΩ 对地
    };

    explicit MCP4725(i2c_master_bus_handle_t bus, uint16_t addr);

    // Device 接口
    bool Init() override;

    // ====== 电压输出 ======
    // Vout = (code / 4096) × VDD
    bool SetOutputVoltage(float voltage, float vdd);
    bool SetOutputVoltage(float voltage, float vdd, PowerDownMode pd);

    // ====== 原始 12-bit 输出 ======
    bool SetOutputRaw(uint16_t code);                              // 正常模式（volatile）
    bool SetOutputRaw(uint16_t code, PowerDownMode pd);            // 指定模式（volatile）

    // ====== EEPROM（掉电保存） ======
    // 写入 EEPROM 后，DAC 上电自动恢复该值
    bool SaveToEEPROM();                                           // 保存当前输出到 EEPROM
    bool SaveToEEPROM(uint16_t code, PowerDownMode pd);            // 保存指定值到 EEPROM

    // ====== 掉电 / 唤醒 ======
    bool PowerDown(PowerDownMode pd);
    bool WakeUp();                                                 // 恢复正常输出

   private:
    // 2 字节 volatile 写：Byte1=[PD1 PD0 D11..D6], Byte2=[D5..D0 X X]
    esp_err_t WriteVolatile(uint16_t code, PowerDownMode pd);

    // 3 字节 EEPROM 写：Byte1=[011xxxxx], Byte2=[PD1 PD0 D11..D6], Byte3=[D5..D0 X X]
    esp_err_t WriteEEPROM(uint16_t code, PowerDownMode pd);

    // 上次写入的 DAC 值（缓存，用于 SaveToEEPROM 无参调用）
    uint16_t last_code_ = 0;
    PowerDownMode last_pd_ = PowerDownMode::NORMAL;

    static constexpr uint16_t MAX_CODE = 0xFFF;  // 12-bit
};
