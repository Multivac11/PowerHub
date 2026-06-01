#pragma once
#include "device.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

class I2CDevice : public Device
{
   public:
    I2CDevice(i2c_master_bus_handle_t bus, uint16_t addr, const char* tag, uint32_t freq_hz = 100000);

    ~I2CDevice() override;

    bool Probe() override;

    bool Init() override;

    void Deinit() override;

    uint16_t GetAddress() const { return device_address_; }

   protected:
    // 派生类可直接用的底层读写
    esp_err_t Write(const uint8_t* data, size_t len);

    esp_err_t Read(uint8_t* data, size_t len);

    esp_err_t WriteThenRead(const uint8_t* tx, size_t tx_len, uint8_t* rx, size_t rx_len);

    i2c_master_bus_handle_t bus_handle_ = nullptr;

    i2c_master_dev_handle_t dev_handle_ = nullptr;

    uint16_t device_address_;

    uint32_t freq_hz_;

    const char* tag_;
};