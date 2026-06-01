#include "ina226.h"

static const char* TAG = "INA226";

INA226::INA226(i2c_master_bus_handle_t bus, uint16_t addr) : I2CDevice(bus, addr, TAG)
{
}

bool INA226::WriteReg(uint8_t reg, uint16_t val)
{
    uint8_t data[3] = {reg, static_cast<uint8_t>(val >> 8), static_cast<uint8_t>(val)};
    return Write(data, 3) == ESP_OK;
}

uint16_t INA226::ReadReg(uint8_t reg)
{
    uint8_t rx[2] = {0};
    if (WriteThenRead(&reg, 1, rx, 2) != ESP_OK)
    {
        ESP_LOGE(TAG, "ReadReg 0x%02X failed", reg);
        return 0;
    }
    return (static_cast<uint16_t>(rx[0]) << 8) | rx[1];
}

bool INA226::Configure(uint16_t config_val)
{
    ESP_LOGI(TAG, "Configure: 0x%04X", config_val);
    return WriteReg(REG_CONFIG, config_val);
}

bool INA226::Reset()
{
    return WriteReg(REG_CONFIG, 0x8000);  // RST bit
}

bool INA226::SetCalibration(uint16_t cal_val, float current_lsb_a)
{
    if (cal_val == 0)
    {
        ESP_LOGE(TAG, "Calibration value cannot be 0");
        return false;
    }
    current_lsb_ = current_lsb_a;
    power_lsb_ = 25.0f * current_lsb_;

    ESP_LOGI(TAG, "Set CAL=0x%04X, Current_LSB=%.6f A, Power_LSB=%.6f W", cal_val, current_lsb_, power_lsb_);
    return WriteReg(REG_CALIBRATION, cal_val);
}

bool INA226::SetShuntResistor(float r_shunt_ohm, float max_current_a)
{
    if (r_shunt_ohm <= 0.0f || max_current_a <= 0.0f)
    {
        ESP_LOGE(TAG, "Invalid shunt resistor (%.6f) or max current (%.2f)", r_shunt_ohm, max_current_a);
        return false;
    }

    // 检查是否超过 INA226 Shunt 输入满量程 ±81.92mV
    float v_shunt_max = max_current_a * r_shunt_ohm;
    if (v_shunt_max > 0.08192f)
    {
        ESP_LOGW(TAG, "Shunt voltage at max current (%.3f mV) exceeds 81.92 mV limit", v_shunt_max * 1000.0f);
    }

    // Current_LSB = Max Expected Current / 2^15
    float current_lsb = max_current_a / 32768.0f;

    // Calibration = 0.00512 / (Current_LSB * R_shunt)
    float cal_f = 0.00512f / (current_lsb * r_shunt_ohm);
    uint16_t cal = static_cast<uint16_t>(cal_f + 0.5f);

    if (cal < 1) cal = 1;
    if (cal > 65535) cal = 65535;

    // 用实际整数 CAL 反算真实 Current_LSB，消除取整误差
    current_lsb_ = 0.00512f / (cal * r_shunt_ohm);
    power_lsb_ = 25.0f * current_lsb_;

    ESP_LOGI(TAG, "Rshunt=%.4fΩ, Imax=%.2fA -> CAL=0x%04X(%u), Current_LSB=%.6f A, Power_LSB=%.6f W", r_shunt_ohm,
             max_current_a, cal, cal, current_lsb_, power_lsb_);

    return WriteReg(REG_CALIBRATION, cal);
}

/* ---------- 数据读取 ---------- */

float INA226::ReadBusVoltage()
{
    uint16_t raw = ReadReg(REG_BUS_VOLT);
    return raw * 1.25f / 1000.0f;  // 1.25 mV/LSB -> V
}

float INA226::ReadShuntVoltage()
{
    int16_t raw = static_cast<int16_t>(ReadReg(REG_SHUNT_VOLT));
    return raw * 2.5f / 1000.0f;  // 2.5 μV/LSB -> mV
}

float INA226::ReadCurrent()
{
    if (current_lsb_ == 0.0f)
    {
        ESP_LOGW(TAG, "Current LSB not set, call SetShuntResistor() first");
        return 0.0f;
    }
    int16_t raw = static_cast<int16_t>(ReadReg(REG_CURRENT));
    return raw * current_lsb_;  // A
}

float INA226::ReadPower()
{
    if (power_lsb_ == 0.0f)
    {
        ESP_LOGW(TAG, "Power LSB not set, call SetShuntResistor() first");
        return 0.0f;
    }
    uint16_t raw = ReadReg(REG_POWER);
    return raw * power_lsb_;  // W
}

/* ---------- 原始值（调试用） ---------- */

int16_t INA226::ReadShuntVoltageRaw()
{
    return static_cast<int16_t>(ReadReg(REG_SHUNT_VOLT));
}

uint16_t INA226::ReadBusVoltageRaw()
{
    return ReadReg(REG_BUS_VOLT);
}

int16_t INA226::ReadCurrentRaw()
{
    return static_cast<int16_t>(ReadReg(REG_CURRENT));
}

uint16_t INA226::ReadPowerRaw()
{
    return ReadReg(REG_POWER);
}

/* ---------- Alert ---------- */

bool INA226::SetAlert(uint16_t mask_enable, uint16_t limit)
{
    if (!WriteReg(REG_MASK_ENABLE, mask_enable)) return false;
    return WriteReg(REG_ALERT_LIMIT, limit);
}

uint16_t INA226::ReadAlertFlags()
{
    return ReadReg(REG_MASK_ENABLE);
}

/* ---------- ID ---------- */

uint16_t INA226::ReadManufacturerID()
{
    return ReadReg(REG_MANUF_ID);
}

uint16_t INA226::ReadDieID()
{
    return ReadReg(REG_DIE_ID);
}