set(src_dirs
    device
    device/i2c_device
    device/i2c_device/ina226
    device/i2c_device/tca9535
    wifi_manager
    key
    lcd
    lcd/font
    power_monitor
)

set(include_dirs
    device
    device/i2c_device
    device/i2c_device/ina226
    device/i2c_device/tca9535
    wifi_manager
    key
    lcd
    lcd/font
    power_monitor
)

set(required_components
    driver
    esp_timer
    esp_driver_i2c
    esp_driver_spi
    esp_event
    esp_wifi
    esp_http_server
    esp_lcd
    spiffs
    nvs_flash
    json
    esp_lvgl_port
    esp_driver_ppa
)

idf_component_register(
    SRC_DIRS ${src_dirs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${required_components}
)

component_compile_options(
    -ffast-math -O3 -Wno-error=format=-Wno-format
)
