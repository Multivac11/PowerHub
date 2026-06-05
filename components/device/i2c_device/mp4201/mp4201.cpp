#include "mp4201.h"
#include <cmath>

static const char* TAG = "MP4201";

MP4201::MP4201(i2c_master_bus_handle_t bus, uint16_t addr) : I2CDevice(bus, addr, TAG)
{
}

bool MP4201::Init()
{
    if (!I2CDevice::Init())
    {
        return false;
    }

    ESP_LOGI(TAG, "Init MP4201 success");
    return true;
}

// ====== FREQ / MODE 引脚电压计算 ======

float MP4201::FreqToVoltage(SwitchingFrequency freq, float vcc)
{
    switch (freq)
    {
        case SwitchingFrequency::KHZ_200:
            return 0.0f;
        case SwitchingFrequency::KHZ_400:
            return vcc / 3.0f;  // 1/3 × VCC
        case SwitchingFrequency::KHZ_600:
            return vcc * 2.0f / 3.0f;  // 2/3 × VCC
        case SwitchingFrequency::KHZ_1000:
            return vcc;  // VCC
    }
    return 0.0f;
}

float MP4201::ModeToVoltage(OperationMode mode, float vcc)
{
    switch (mode)
    {
        case OperationMode::PFM:
            return 0.0f;
        case OperationMode::PFM_FSS:
            return vcc / 3.0f;
        case OperationMode::FCCM:
            return vcc * 2.0f / 3.0f;
        case OperationMode::FCCM_FSS:
            return vcc;
    }
    return 0.0f;
}

// ====== PMBus 读写辅助 ======

esp_err_t MP4201::WriteByte(uint8_t reg_addr, uint8_t data)
{
    uint8_t buf[] = {reg_addr, data};
    return Write(buf, sizeof(buf));
}

esp_err_t MP4201::WriteWord(uint8_t reg_addr, uint16_t data)
{
    // PMBus 字节序：Data Low 在前，Data High 在后
    uint8_t buf[] = {reg_addr, static_cast<uint8_t>(data & 0xFF), static_cast<uint8_t>(data >> 8)};
    return Write(buf, sizeof(buf));
}

esp_err_t MP4201::ReadByte(uint8_t reg_addr, uint8_t& data)
{
    return WriteThenRead(&reg_addr, 1, &data, 1);
}

esp_err_t MP4201::ReadWord(uint8_t reg_addr, uint16_t& data)
{
    uint8_t rx[2];
    esp_err_t ret = WriteThenRead(&reg_addr, 1, rx, 2);
    if (ret == ESP_OK)
    {
        data = rx[0] | (static_cast<uint16_t>(rx[1]) << 8);  // PMBus：Low Byte 在前
    }
    return ret;
}

// Send Byte: S+AddrW+A+Cmd+A+P（无数据字节，用于 CLEAR_FAULTS 等）
esp_err_t MP4201::SendByte(uint8_t reg_addr)
{
    return Write(&reg_addr, 1);
}

// ====== 使能控制 ======

bool MP4201::EnableOutput()
{
    // PMBus OPERATION: bit7=ON, bits6:0=0x00（默认运行模式）
    return WriteByte(OPERATION, 0x80) == ESP_OK;
}

bool MP4201::DisableOutput()
{
    return WriteByte(OPERATION, 0x00) == ESP_OK;
}

bool MP4201::IsEnabled(uint8_t& en_state)
{
    return ReadByte(OPERATION, en_state) == ESP_OK;
}

// ====== 输出电压设定（VOUT_COMMAND） ======

bool MP4201::SetOutputVoltage(float voltage)
{
    if (voltage < 3.2f || voltage > 81.92f)
    {
        ESP_LOGE(TAG, "Output voltage out of range: %.2fV (3.2~81.92V)", voltage);
        return false;
    }

    // Vout(V) = VOUT_COMMAND × 0.625 × 32 / 1000 = VOUT_COMMAND × 0.020
    // 1.625mV/step (relative to internal reference), 20mV/step (at output with FB ratio 32)
    uint16_t reg_val = static_cast<uint16_t>(std::round(voltage / VIN_REG_STEP));
    if (reg_val > 0xFFF)
    {
        reg_val = 0xFFF;
    }

    return WriteWord(VOUT_COMMAND, reg_val) == ESP_OK;
}

bool MP4201::GetOutputVoltage(float& voltage)
{
    uint16_t reg_val;
    if (ReadWord(VOUT_COMMAND, reg_val) != ESP_OK)
    {
        return false;
    }
    voltage = (reg_val & 0xFFF) * VIN_REG_STEP;
    return true;
}

// ====== 输入电压调节（VIN_REG_THLD，用于 MPPT / 功率限制） ======

bool MP4201::SetInputRegulation(float voltage)
{
    if (voltage < 0.0f || voltage > 81.92f)
    {
        ESP_LOGE(TAG, "Input regulation voltage out of range: %.2fV (0~81.92V)", voltage);
        return false;
    }

    uint16_t reg_val = static_cast<uint16_t>(std::round(voltage / VIN_REG_STEP));
    if (reg_val > 0xFFF)
    {
        reg_val = 0xFFF;
    }

    return WriteWord(VIN_REG_THLD, reg_val) == ESP_OK;
}

bool MP4201::GetInputRegulation(float& voltage)
{
    uint16_t reg_val;
    if (ReadWord(VIN_REG_THLD, reg_val) != ESP_OK)
    {
        return false;
    }
    voltage = (reg_val & 0xFFF) * VIN_REG_STEP;
    return true;
}

// ====== 故障清除 ======

bool MP4201::ClearFaults()
{
    // CLEAR_FAULTS 使用 Send Byte 格式（只发命令码，无数据字节）
    return SendByte(CLEAR_FAULTS) == ESP_OK;
}

// ====== 输出过流限制 ======

bool MP4201::SetOutputCurrentLimit(float current)
{
    if (current < 0.0f || current > 25.0f)
    {
        ESP_LOGE(TAG, "Current out of range: %.2fA (0.5~25A)", current);
        return false;
    }

    // IOUT_OC (A) = IOUT_LIM × 0.05
    uint16_t reg_val = static_cast<uint16_t>(std::round(current / CURRENT_STEP));
    return WriteWord(IOUT_OC_FAULT_LIMIT, reg_val) == ESP_OK;
}

bool MP4201::GetOutputCurrentLimit(float& current)
{
    uint16_t reg_val;
    if (ReadWord(IOUT_OC_FAULT_LIMIT, reg_val) != ESP_OK)
    {
        return false;
    }
    current = (reg_val & 0xFF) * CURRENT_STEP;  // 低 8 位有效
    return true;
}

// ====== 输入过流限制 ======

bool MP4201::SetInputCurrentLimit(float current)
{
    if (current < 0.0f || current > 25.0f)
    {
        ESP_LOGE(TAG, "Current out of range: %.2fA (0.5~25A)", current);
        return false;
    }

    uint16_t reg_val = static_cast<uint16_t>(std::round(current / CURRENT_STEP));
    return WriteWord(IIN_OC_FAULT_LIMIT, reg_val) == ESP_OK;
}

bool MP4201::GetInputCurrentLimit(float& current)
{
    uint16_t reg_val;
    if (ReadWord(IIN_OC_FAULT_LIMIT, reg_val) != ESP_OK)
    {
        return false;
    }
    current = (reg_val & 0xFF) * CURRENT_STEP;
    return true;
}

// ====== 功率方向 ======

bool MP4201::SetDirection(Direction dir)
{
    uint8_t ctrl1;
    if (ReadByte(MFR_CTRL1, ctrl1) != ESP_OK)
    {
        return false;
    }

    if (dir == Direction::VIN_TO_OUT)
    {
        ctrl1 |= (1 << 7);  // DIR 位置 1
    }
    else
    {
        ctrl1 &= ~(1 << 7);  // DIR 位置 0
    }

    return WriteByte(MFR_CTRL1, ctrl1) == ESP_OK;
}

bool MP4201::GetDirection(Direction& dir)
{
    uint8_t ctrl1;
    if (ReadByte(MFR_CTRL1, ctrl1) != ESP_OK)
    {
        return false;
    }
    dir = (ctrl1 & (1 << 7)) ? Direction::VIN_TO_OUT : Direction::OUT_TO_VIN;
    return true;
}

// ====== UVLO 阈值 ======

bool MP4201::SetUVLOThreshold(UVLOThreshold threshold)
{
    uint8_t ctrl1;
    if (ReadByte(MFR_CTRL1, ctrl1) != ESP_OK)
    {
        return false;
    }

    ctrl1 &= ~(0x3 << 5);                             // 清除 bits 6:5
    ctrl1 |= (static_cast<uint8_t>(threshold) << 5);  // 设置新值

    return WriteByte(MFR_CTRL1, ctrl1) == ESP_OK;
}

// ====== 反馈模式 ======

bool MP4201::SetFeedbackMode(bool internal)
{
    uint8_t ctrl1;
    if (ReadByte(MFR_CTRL1, ctrl1) != ESP_OK)
    {
        return false;
    }

    if (internal)
    {
        ctrl1 |= (1 << 3);  // FB_MODE = 1（内部反馈）
    }
    else
    {
        ctrl1 &= ~(1 << 3);  // FB_MODE = 0（外部反馈）
    }

    return WriteByte(MFR_CTRL1, ctrl1) == ESP_OK;
}

// ====== OCP 模式 ======

bool MP4201::SetOCPMode(OCPMode mode)
{
    uint8_t ctrl1;
    if (ReadByte(MFR_CTRL1, ctrl1) != ESP_OK)
    {
        return false;
    }

    if (mode == OCPMode::CONSTANT_CURRENT)
    {
        ctrl1 |= (1 << 2);
    }
    else
    {
        ctrl1 &= ~(1 << 2);
    }

    return WriteByte(MFR_CTRL1, ctrl1) == ESP_OK;
}

// ====== 死区时间 ======

bool MP4201::SetDeadTime(DeadTime dt)
{
    uint8_t ctrl1;
    if (ReadByte(MFR_CTRL1, ctrl1) != ESP_OK)
    {
        return false;
    }

    ctrl1 &= ~0x3;  // 清除 bits 1:0
    ctrl1 |= static_cast<uint8_t>(dt);

    return WriteByte(MFR_CTRL1, ctrl1) == ESP_OK;
}

// ====== 开关电流限制 ======

bool MP4201::SetSwitchingCurrentLimit(SwitchingCurrentLimit limit)
{
    uint8_t ocp_ctrl;
    if (ReadByte(MFR_OCP_CTRL, ocp_ctrl) != ESP_OK)
    {
        return false;
    }

    ocp_ctrl &= ~(0x7 << 5);                         // 清除 bits 7:5
    ocp_ctrl |= (static_cast<uint8_t>(limit) << 5);  // 设置新值

    return WriteByte(MFR_OCP_CTRL, ocp_ctrl) == ESP_OK;
}

// ====== 采样电阻配置 ======

bool MP4201::SetSenseResistors(SenseResistor rs1, SenseResistor rs2)
{
    uint8_t ocp_ctrl;
    if (ReadByte(MFR_OCP_CTRL, ocp_ctrl) != ESP_OK)
    {
        return false;
    }

    ocp_ctrl &= ~(0x3 << 2);  // 清除 RSENS_2 bits 3:2
    ocp_ctrl |= (static_cast<uint8_t>(rs2) << 2);
    ocp_ctrl &= ~0x3;  // 清除 RSENS_1 bits 1:0
    ocp_ctrl |= static_cast<uint8_t>(rs1);

    return WriteByte(MFR_OCP_CTRL, ocp_ctrl) == ESP_OK;
}

// ====== 负载线补偿 ======

bool MP4201::SetLineDropCompensation(LineDropCompensation comp)
{
    uint8_t ctrl2;
    if (ReadByte(MFR_CTRL2, ctrl2) != ESP_OK)
    {
        return false;
    }

    ctrl2 &= ~0x7;  // 清除 bits 2:0
    ctrl2 |= static_cast<uint8_t>(comp);

    return WriteByte(MFR_CTRL2, ctrl2) == ESP_OK;
}

// ====== CC 使能 ======

bool MP4201::SetOutputCCEnable(bool enable)
{
    uint8_t ctrl3;
    if (ReadByte(MFR_CTRL3, ctrl3) != ESP_OK)
    {
        return false;
    }

    if (enable)
    {
        ctrl3 |= (1 << 7);  // OUT_CC_EN = 1
    }
    else
    {
        ctrl3 &= ~(1 << 7);  // OUT_CC_EN = 0
    }

    return WriteByte(MFR_CTRL3, ctrl3) == ESP_OK;
}

bool MP4201::SetInputCCEnable(bool enable)
{
    uint8_t ctrl3;
    if (ReadByte(MFR_CTRL3, ctrl3) != ESP_OK)
    {
        return false;
    }

    if (enable)
    {
        ctrl3 |= (1 << 6);  // IN_CC_EN = 1
    }
    else
    {
        ctrl3 &= ~(1 << 6);  // IN_CC_EN = 0
    }

    return WriteByte(MFR_CTRL3, ctrl3) == ESP_OK;
}

// ====== 状态读取 ======

bool MP4201::ReadStatusWord(StatusWord& status)
{
    uint16_t raw;
    if (ReadWord(STATUS_WORD, raw) != ESP_OK)
    {
        return false;
    }
    status.raw = raw;
    return true;
}

bool MP4201::ReadTemperatureStatus(TemperatureStatus& status)
{
    uint8_t raw;
    if (ReadByte(STATUS_TEMPERATURE, raw) != ESP_OK)
    {
        return false;
    }
    status.raw = raw;
    return true;
}

// ====== ADC 实时读数（10-bit） ======

bool MP4201::ReadADC(ADCReadings& adc)
{
    uint16_t raw;

    if (ReadWord(ADC_VIN, raw) == ESP_OK)
    {
        adc.Vin = (raw & 0x3FF) * 0.0832f;  // VIN(V) = Y × 0.0832
    }
    else
    {
        adc.Vin = 0.0f;
    }

    if (ReadWord(ADC_IIN, raw) == ESP_OK)
    {
        adc.Iin = (raw & 0x3FF) * 0.043f;  // IIN(A) = Y × 0.043
    }
    else
    {
        adc.Iin = 0.0f;
    }

    if (ReadWord(ADC_VOUT, raw) == ESP_OK)
    {
        adc.Vout = (raw & 0x3FF) * 0.0832f;  // VOUT(V) = Y × 0.0832
    }
    else
    {
        adc.Vout = 0.0f;
    }

    if (ReadWord(ADC_IOUT, raw) == ESP_OK)
    {
        adc.Iout = (raw & 0x3FF) * 0.043f;  // IOUT(A) = Y × 0.043
    }
    else
    {
        adc.Iout = 0.0f;
    }

    if (ReadWord(ADC_TEMP, raw) == ESP_OK)
    {
        adc.Temp = -212.48f + 1.07f * (raw & 0x3FF);  // T(°C) = -212.48 + 1.07 × Y
    }
    else
    {
        adc.Temp = 0.0f;
    }

    return true;
}

// ====== OTP 信息 ======

bool MP4201::ReadOTPConfigCode(uint8_t& code)
{
    return ReadByte(MFR_OTP_CFG, code) == ESP_OK;
}

bool MP4201::ReadOTPVersion(uint8_t& version)
{
    return ReadByte(OTP_VERSION, version) == ESP_OK;
}
