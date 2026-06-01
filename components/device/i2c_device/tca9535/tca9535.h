#pragma once
#include <cstdint>

#include "i2c_device.h"

class TCA9535 : public I2CDevice
{
   public:
    explicit TCA9535(i2c_master_bus_handle_t bus, uint16_t addr = 0x20);

    // 方向配置（1=输入, 0=输出），port=0/1，mask 的每个 bit 对应一个引脚
    bool SetDirection(uint8_t port, uint8_t mask);
    bool SetDirection16(uint16_t mask);  // 低 8 位 Port0, 高 8 位 Port1

    // 读取当前方向配置
    uint8_t GetDirection(uint8_t port);
    uint16_t GetDirection16();

    // 输出写入（仅对配置为输出的引脚有效）
    bool WriteOutput(uint8_t port, uint8_t val);
    bool WriteOutput16(uint16_t val);

    // 读取输出寄存器
    uint8_t ReadOutput(uint8_t port);
    uint16_t ReadOutput16();

    // 输入读取（读取引脚实际电平）
    uint8_t ReadInput(uint8_t port);
    uint16_t ReadInput16();

    // 极性反转（1=反转, 0=不反转）
    bool SetPolarity(uint8_t port, uint8_t mask);
    bool SetPolarity16(uint16_t mask);

    // 单引脚操作（pin: 0~15）
    bool PinMode(uint8_t pin, bool is_input);
    bool DigitalWrite(uint8_t pin, bool level);
    bool DigitalRead(uint8_t pin);

   private:
    bool WriteReg(uint8_t reg, uint8_t val);
    uint8_t ReadReg(uint8_t reg);

    static constexpr uint8_t REG_INPUT0 = 0x00;
    static constexpr uint8_t REG_INPUT1 = 0x01;
    static constexpr uint8_t REG_OUTPUT0 = 0x02;
    static constexpr uint8_t REG_OUTPUT1 = 0x03;
    static constexpr uint8_t REG_POLARITY0 = 0x04;
    static constexpr uint8_t REG_POLARITY1 = 0x05;
    static constexpr uint8_t REG_CONFIG0 = 0x06;
    static constexpr uint8_t REG_CONFIG1 = 0x07;
};
