#include "i2c_device.h"

I2CDevice::I2CDevice(i2c_master_bus_handle_t bus, uint16_t addr, const char* tag, uint32_t freq_hz)
    : bus_handle_(bus), device_address_(addr), freq_hz_(freq_hz), tag_(tag)
{
}

I2CDevice::~I2CDevice()
{
    Deinit();
}

bool I2CDevice::Probe()
{
    esp_err_t ret = i2c_master_probe(bus_handle_, device_address_, -1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(tag_, "Not found at 0x%02X", device_address_);
        return false;
    }
    ESP_LOGI(tag_, "Found at 0x%02X", device_address_);
    return true;
}

bool I2CDevice::Init()
{
    if (!Probe()) return false;

    i2c_device_config_t dev_config = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
                                      .device_address = device_address_,
                                      .scl_speed_hz = freq_hz_,
                                      .scl_wait_us = 100,
                                      .flags = {.disable_ack_check = false}};

    esp_err_t ret = i2c_master_bus_add_device(bus_handle_, &dev_config, &dev_handle_);
    if (ret != ESP_OK)
    {
        ESP_LOGE(tag_, "Failed to add to bus ");
        return false;
    }
    ESP_LOGI(tag_, "Registered on bus");
    return true;
}

void I2CDevice::Deinit()
{
    if (dev_handle_)
    {
        i2c_master_bus_rm_device(dev_handle_);
        dev_handle_ = nullptr;
    }
}

esp_err_t I2CDevice::Write(const uint8_t* data, size_t len)
{
    return i2c_master_transmit(dev_handle_, data, len, -1);
}

esp_err_t I2CDevice::Read(uint8_t* data, size_t len)
{
    return i2c_master_receive(dev_handle_, data, len, -1);
}

esp_err_t I2CDevice::WriteThenRead(const uint8_t* tx, size_t tx_len, uint8_t* rx, size_t rx_len)
{
    return i2c_master_transmit_receive(dev_handle_, tx, tx_len, rx, rx_len, -1);
}