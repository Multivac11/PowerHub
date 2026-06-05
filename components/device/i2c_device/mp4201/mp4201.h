#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_device.h"

// MP4201 — 100V/25A 同步双向升降压控制器，PMBus 接口
class MP4201 : public I2CDevice
{
   public:
    // 功率方向
    enum class Direction : uint8_t
    {
        OUT_TO_VIN = 0,  // 放电模式：电流 OUT → VIN
        VIN_TO_OUT = 1,  // 充电模式：电流 VIN → OUT
    };

    // UVLO 阈值
    enum class UVLOThreshold : uint8_t
    {
        MIN = 0,   // 最小值（参见 EC 表）
        V8_5 = 1,  // 8.5V 上升阈值
        V11 = 2,   // 11V 上升阈值
        V20 = 3,   // 20V 上升阈值
    };

    // OCP 保护模式
    enum class OCPMode : uint8_t
    {
        HICCUP = 0,            // 打嗝模式
        CONSTANT_CURRENT = 1,  // 恒流模式
    };

    // 死区时间
    enum class DeadTime : uint8_t
    {
        NS_15 = 0,
        NS_20 = 1,
        NS_25 = 2,
        NS_30 = 3,
    };

    // 开关电流限制
    enum class SwitchingCurrentLimit : uint8_t
    {
        A10_PK_A8_VAL = 0,   // 峰值 10A / 谷值 8A
        A15_PK_A12_VAL = 1,  // 峰值 15A / 谷值 12A
        A20_PK_A17_VAL = 2,  // 峰值 20A / 谷值 17A
        A25_PK_A22_VAL = 3,  // 峰值 25A / 谷值 22A（默认）
        A30_PK_A26_VAL = 4,  // 峰值 30A / 谷值 26A
        A35_PK_A30_VAL = 5,  // 峰值 35A / 谷值 30A
    };

    // 电流采样电阻值
    enum class SenseResistor : uint8_t
    {
        R5_MOHM = 0,  // 5mΩ
        R2_MOHM = 1,  // 2mΩ
    };

    // FREQ 引脚 — 开关频率（由外部 DAC 电压设定，VCC=5V）
    enum class SwitchingFrequency : uint8_t
    {
        KHZ_200 = 0,   // GND 或悬空
        KHZ_400 = 1,   // 1/3 × VCC ≈ 1.667V
        KHZ_600 = 2,   // 2/3 × VCC ≈ 3.333V
        KHZ_1000 = 3,  // VCC = 5V
    };

    // MODE 引脚 — 工作模式（由外部 DAC 电压设定，VCC=5V）
    enum class OperationMode : uint8_t
    {
        PFM = 0,       // GND — PFM，无展频
        PFM_FSS = 1,   // 1/3 × VCC ≈ 1.667V — PFM，带展频
        FCCM = 2,      // 2/3 × VCC ≈ 3.333V — FCCM，无展频
        FCCM_FSS = 3,  // VCC = 5V — FCCM，带展频
    };

    // 将枚举值转换为 FREQ/MODE 引脚电压（VCC 默认 5V）
    static float FreqToVoltage(SwitchingFrequency freq, float vcc = 5.0f);
    static float ModeToVoltage(OperationMode mode, float vcc = 5.0f);

    // 输出电压负载线补偿
    enum class LineDropCompensation : uint8_t
    {
        NONE = 0,         // 无补偿
        MV80_AT_5A = 1,   // 5A 输出时补偿 80mV
        MV160_AT_5A = 2,  // 5A 输出时补偿 160mV
        MV240_AT_5A = 3,  // 5A 输出时补偿 240mV
        MV360_AT_5A = 4,  // 5A 输出时补偿 360mV
        MV480_AT_5A = 5,  // 5A 输出时补偿 480mV
        MV600_AT_5A = 6,  // 5A 输出时补偿 600mV
        MV720_AT_5A = 7,  // 5A 输出时补偿 720mV
    };

    // STATUS_WORD 位定义（High Byte）
    struct StatusWord
    {
        uint16_t raw;
        bool scp_fault() const { return raw & (1 << 14); }
        bool input_ov_fault() const { return raw & (1 << 13); }
        bool pg_fault() const { return raw & (1 << 11); }  // POWER_GOOD 取反
        bool charge_complete() const { return raw & (1 << 9); }
        bool iin_oc_fault() const { return raw & (1 << 8); }
        bool vout_ov_fault() const { return raw & (1 << 5); }
        bool iout_oc_fault() const { return raw & (1 << 4); }
        bool temperature_fault() const { return raw & (1 << 2); }
        bool crc_error() const { return raw & (1 << 0); }
    };

    // STATUS_TEMPERATURE 位定义
    struct TemperatureStatus
    {
        uint8_t raw;
        bool ot_fault() const { return raw & (1 << 7); }    // 过温故障 165°C
        bool ot_warning() const { return raw & (1 << 6); }  // 过温警告 135°C
        bool ntc_fault() const { return raw & (1 << 5); }   // NTC 故障
    };

    // 10-bit ADC 实时读数
    struct ADCReadings
    {
        float Vin;   // 输入电压 (V)
        float Iin;   // 输入电流 (A)
        float Vout;  // 输出电压 (V)
        float Iout;  // 输出电流 (A)
        float Temp;  // 芯片温度 (°C)
    };

    explicit MP4201(i2c_master_bus_handle_t bus, uint16_t addr);

    // Device 接口
    bool Init() override;

    // 使能/禁用输出
    bool EnableOutput();
    bool DisableOutput();
    bool IsEnabled(uint8_t& en_state);

    // 输出电压设定（VOUT_COMMAND，内部反馈比 32，20mV/step，范围 3.2～81.92V）
    bool SetOutputVoltage(float voltage);
    bool GetOutputVoltage(float& voltage);

    // 输入电压调节（VIN_REG_THLD，用于 MPPT / 输入功率限制，20mV/step）
    bool SetInputRegulation(float voltage);
    bool GetInputRegulation(float& voltage);

    // 保持兼容旧 API
    bool SetVoltageRegulation(float voltage) { return SetInputRegulation(voltage); }
    bool GetVoltageRegulation(float& voltage) { return GetInputRegulation(voltage); }

    // 清除所有锁存故障位
    bool ClearFaults();

    // 输出过流限制（50mA/step，范围 0.5～25A）
    bool SetOutputCurrentLimit(float current);
    bool GetOutputCurrentLimit(float& current);

    // 输入过流限制（50mA/step，范围 0.5～25A）
    bool SetInputCurrentLimit(float current);
    bool GetInputCurrentLimit(float& current);

    // 功率方向
    bool SetDirection(Direction dir);
    bool GetDirection(Direction& dir);

    // UVLO 阈值
    bool SetUVLOThreshold(UVLOThreshold threshold);

    // 反馈模式（内部/外部）
    bool SetFeedbackMode(bool internal);  // true=内部反馈(反馈比32)

    // OCP 模式
    bool SetOCPMode(OCPMode mode);

    // 死区时间
    bool SetDeadTime(DeadTime dt);

    // 开关电流限制
    bool SetSwitchingCurrentLimit(SwitchingCurrentLimit limit);

    // 采样电阻配置
    bool SetSenseResistors(SenseResistor rs1, SenseResistor rs2);

    // 负载线补偿
    bool SetLineDropCompensation(LineDropCompensation comp);

    // 输出 CC 使能
    bool SetOutputCCEnable(bool enable);

    // 输入 CC 使能
    bool SetInputCCEnable(bool enable);

    // 状态读取
    bool ReadStatusWord(StatusWord& status);
    bool ReadTemperatureStatus(TemperatureStatus& status);

    // ADC 实时读数（10-bit）
    bool ReadADC(ADCReadings& adc);

    // OTP 信息
    bool ReadOTPConfigCode(uint8_t& code);
    bool ReadOTPVersion(uint8_t& version);

   private:
    // PMBus 读写辅助（寄存器地址 1 字节）
    esp_err_t SendByte(uint8_t reg_addr);           // S+AddrW+A+Cmd+A+P（无数据字节）
    esp_err_t WriteByte(uint8_t reg_addr, uint8_t data);
    esp_err_t WriteWord(uint8_t reg_addr, uint16_t data);
    esp_err_t ReadByte(uint8_t reg_addr, uint8_t& data);
    esp_err_t ReadWord(uint8_t reg_addr, uint16_t& data);

    // PMBus 寄存器地址
    static constexpr uint8_t OPERATION = 0x01;
    static constexpr uint8_t CLEAR_FAULTS = 0x03;
    static constexpr uint8_t VOUT_COMMAND = 0x21;
    static constexpr uint8_t IOUT_OC_FAULT_LIMIT = 0x46;
    static constexpr uint8_t VIN_REG_THLD = 0x59;
    static constexpr uint8_t IIN_OC_FAULT_LIMIT = 0x5B;
    static constexpr uint8_t STATUS_WORD = 0x79;
    static constexpr uint8_t STATUS_TEMPERATURE = 0x7D;
    static constexpr uint8_t MFR_CTRL1 = 0xD0;
    static constexpr uint8_t MFR_CTRL2 = 0xD1;
    static constexpr uint8_t MFR_OCP_CTRL = 0xD2;
    static constexpr uint8_t MFR_CTRL3 = 0xD3;
    static constexpr uint8_t MFR_PRE_CURRENT = 0xD7;
    static constexpr uint8_t MFR_STATUS_MASK = 0xD8;
    static constexpr uint8_t MFR_OTP_CFG = 0xD9;
    static constexpr uint8_t OTP_VERSION = 0xDA;

    // ADC 只读寄存器（10-bit）
    static constexpr uint8_t ADC_VIN = 0x88;
    static constexpr uint8_t ADC_IIN = 0x89;
    static constexpr uint8_t ADC_VOUT = 0x8B;
    static constexpr uint8_t ADC_IOUT = 0x8C;
    static constexpr uint8_t ADC_TEMP = 0x8D;

    // 内部反馈比（固定 32，手册 3.1 节）
    static constexpr float INTERNAL_FB_RATIO = 32.0f;
    // VIN_REG_THLD 步进电压 (V/step)
    static constexpr float VIN_REG_STEP = 0.020f;  // 20mV/step
    // 电流限制步进 (A/step)
    static constexpr float CURRENT_STEP = 0.05f;  // 50mA/step
};
