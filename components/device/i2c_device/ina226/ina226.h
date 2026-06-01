#pragma once
#include <cstdint>

#include "i2c_device.h"

class INA226 : public I2CDevice
{
   public:
    explicit INA226(i2c_master_bus_handle_t bus, uint16_t addr = 0x40);

    // 基础配置
    bool Configure(uint16_t config_val = 0x056F);
    bool Reset();

    // 手动写入 Calibration（需同时提供 Current_LSB，单位 A/bit）
    bool SetCalibration(uint16_t cal_val, float current_lsb_a);

    // 自动计算 Calibration：给定采样电阻(Ω)和期望最大电流(A)
    // 例：3mΩ + 19A -> 自动算出 CAL=0x0B7F
    bool SetShuntResistor(float r_shunt_ohm, float max_current_a);

    // 电压 / 电流 / 功率读取
    float ReadBusVoltage();    // 总线电压 (V)
    float ReadShuntVoltage();  // 采样电阻压降 (mV)
    float ReadCurrent();       // 电流 (A)，需先配置 Calibration
    float ReadPower();         // 功率 (W)，需先配置 Calibration

    // 原始 ADC 值读取（调试用）
    int16_t ReadShuntVoltageRaw();
    uint16_t ReadBusVoltageRaw();
    int16_t ReadCurrentRaw();
    uint16_t ReadPowerRaw();

    // Alert 配置（Mask/Enable + Limit）
    bool SetAlert(uint16_t mask_enable, uint16_t limit);
    uint16_t ReadAlertFlags();

    // ID 读取（用于验证 I2C 通信是否正常）
    uint16_t ReadManufacturerID();  // 应为 0x5449 ('TI')
    uint16_t ReadDieID();           // 应为 0x2260

    // 获取当前 LSB（外部手动计算用）
    float GetCurrentLSB() const { return current_lsb_; }
    float GetPowerLSB() const { return power_lsb_; }

   private:
    float current_lsb_ = 0.0f;  // A/bit
    float power_lsb_ = 0.0f;    // W/bit

    bool WriteReg(uint8_t reg, uint16_t val);
    uint16_t ReadReg(uint8_t reg);

    static constexpr uint8_t REG_CONFIG = 0x00;
    static constexpr uint8_t REG_SHUNT_VOLT = 0x01;
    static constexpr uint8_t REG_BUS_VOLT = 0x02;
    static constexpr uint8_t REG_POWER = 0x03;
    static constexpr uint8_t REG_CURRENT = 0x04;
    static constexpr uint8_t REG_CALIBRATION = 0x05;
    static constexpr uint8_t REG_MASK_ENABLE = 0x06;
    static constexpr uint8_t REG_ALERT_LIMIT = 0x07;
    static constexpr uint8_t REG_MANUF_ID = 0xFE;
    static constexpr uint8_t REG_DIE_ID = 0xFF;
};