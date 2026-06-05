#pragma once
#include <memory>
#include <vector>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "i2c_device.h"
#include "ina226.h"
#include "mcp4725.h"
#include "mp4201.h"
#include "tca9535.h"

class INA226;
class TCA9535;

#define I2C_MASTER_SCL_IO GPIO_NUM_17
#define I2C_MASTER_SDA_IO GPIO_NUM_16
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000

class I2CBusManager
{
public:
    static I2CBusManager &GetInstance()
    {
        static I2CBusManager instance;
        return instance;
    }

    bool Init();

    void Deinit();

    bool RegisterINA226(uint16_t addr);

    bool RegisterTCA9535(uint16_t addr);

    bool RegisterMP4201(uint16_t addr);

    bool RegisterMCP4725(uint16_t addr);

    template <typename T>
    T *GetDeviceByAddr(uint16_t addr)
    {
        for (auto &dev : devices_)
        {
            auto *i2c = static_cast<I2CDevice *>(dev.get());
            if (i2c->GetAddress() == addr)
                return static_cast<T *>(dev.get());
        }
        return nullptr;
    }

    i2c_master_bus_handle_t GetBusHandle() const { return bus_handle_; }

private:
    I2CBusManager() = default;

    ~I2CBusManager() = default;

    i2c_master_bus_handle_t bus_handle_ = nullptr;

    std::vector<std::unique_ptr<Device>> devices_;
};
