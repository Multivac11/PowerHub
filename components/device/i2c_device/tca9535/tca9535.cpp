#include "tca9535.h"

static const char* TAG = "TCA9535";

TCA9535::TCA9535(i2c_master_bus_handle_t bus, uint16_t addr) : I2CDevice(bus, addr, TAG)
{
}

bool TCA9535::WriteReg(uint8_t reg, uint8_t val)
{
    uint8_t data[2] = {reg, val};
    return Write(data, 2) == ESP_OK;
}

uint8_t TCA9535::ReadReg(uint8_t reg)
{
    uint8_t rx = 0;
    if (WriteThenRead(&reg, 1, &rx, 1) != ESP_OK)
    {
        ESP_LOGE(TAG, "ReadReg 0x%02X failed", reg);
        return 0;
    }
    return rx;
}

/* ---------- 方向配置 ---------- */

bool TCA9535::SetDirection(uint8_t port, uint8_t mask)
{
    uint8_t reg = (port == 0) ? REG_CONFIG0 : REG_CONFIG1;
    ESP_LOGI(TAG, "SetDirection port=%u mask=0x%02X", port, mask);
    return WriteReg(reg, mask);
}

bool TCA9535::SetDirection16(uint16_t mask)
{
    ESP_LOGI(TAG, "SetDirection16 mask=0x%04X", mask);
    if (!WriteReg(REG_CONFIG0, static_cast<uint8_t>(mask & 0xFF))) return false;
    return WriteReg(REG_CONFIG1, static_cast<uint8_t>(mask >> 8));
}

uint8_t TCA9535::GetDirection(uint8_t port)
{
    uint8_t reg = (port == 0) ? REG_CONFIG0 : REG_CONFIG1;
    return ReadReg(reg);
}

uint16_t TCA9535::GetDirection16()
{
    uint8_t lo = ReadReg(REG_CONFIG0);
    uint8_t hi = ReadReg(REG_CONFIG1);
    return (static_cast<uint16_t>(hi) << 8) | lo;
}

/* ---------- 输出写入 ---------- */

bool TCA9535::WriteOutput(uint8_t port, uint8_t val)
{
    uint8_t reg = (port == 0) ? REG_OUTPUT0 : REG_OUTPUT1;
    return WriteReg(reg, val);
}

bool TCA9535::WriteOutput16(uint16_t val)
{
    if (!WriteReg(REG_OUTPUT0, static_cast<uint8_t>(val & 0xFF))) return false;
    return WriteReg(REG_OUTPUT1, static_cast<uint8_t>(val >> 8));
}

uint8_t TCA9535::ReadOutput(uint8_t port)
{
    uint8_t reg = (port == 0) ? REG_OUTPUT0 : REG_OUTPUT1;
    return ReadReg(reg);
}

uint16_t TCA9535::ReadOutput16()
{
    uint8_t lo = ReadReg(REG_OUTPUT0);
    uint8_t hi = ReadReg(REG_OUTPUT1);
    return (static_cast<uint16_t>(hi) << 8) | lo;
}

/* ---------- 输入读取 ---------- */

uint8_t TCA9535::ReadInput(uint8_t port)
{
    uint8_t reg = (port == 0) ? REG_INPUT0 : REG_INPUT1;
    return ReadReg(reg);
}

uint16_t TCA9535::ReadInput16()
{
    uint8_t lo = ReadReg(REG_INPUT0);
    uint8_t hi = ReadReg(REG_INPUT1);
    return (static_cast<uint16_t>(hi) << 8) | lo;
}

/* ---------- 极性反转 ---------- */

bool TCA9535::SetPolarity(uint8_t port, uint8_t mask)
{
    uint8_t reg = (port == 0) ? REG_POLARITY0 : REG_POLARITY1;
    ESP_LOGI(TAG, "SetPolarity port=%u mask=0x%02X", port, mask);
    return WriteReg(reg, mask);
}

bool TCA9535::SetPolarity16(uint16_t mask)
{
    ESP_LOGI(TAG, "SetPolarity16 mask=0x%04X", mask);
    if (!WriteReg(REG_POLARITY0, static_cast<uint8_t>(mask & 0xFF))) return false;
    return WriteReg(REG_POLARITY1, static_cast<uint8_t>(mask >> 8));
}

/* ---------- 单引脚操作 ---------- */

bool TCA9535::PinMode(uint8_t pin, bool is_input)
{
    if (pin > 15) return false;

    uint8_t port = (pin < 8) ? 0 : 1;
    uint8_t bit = (pin < 8) ? pin : (pin - 8);

    uint8_t reg = (port == 0) ? REG_CONFIG0 : REG_CONFIG1;
    uint8_t cur = ReadReg(reg);

    if (is_input)
        cur |= (1 << bit);
    else
        cur &= ~(1 << bit);

    return WriteReg(reg, cur);
}

bool TCA9535::DigitalWrite(uint8_t pin, bool level)
{
    if (pin > 15) return false;

    uint8_t port = (pin < 8) ? 0 : 1;
    uint8_t bit = (pin < 8) ? pin : (pin - 8);

    uint8_t reg = (port == 0) ? REG_OUTPUT0 : REG_OUTPUT1;
    uint8_t cur = ReadReg(reg);

    if (level)
        cur |= (1 << bit);
    else
        cur &= ~(1 << bit);

    return WriteReg(reg, cur);
}

bool TCA9535::DigitalRead(uint8_t pin)
{
    if (pin > 15) return false;

    uint8_t port = (pin < 8) ? 0 : 1;
    uint8_t bit = (pin < 8) ? pin : (pin - 8);

    uint8_t val = ReadInput(port);
    return (val >> bit) & 0x01;
}
