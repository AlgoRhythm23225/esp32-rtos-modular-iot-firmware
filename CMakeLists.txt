# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS
    esp32_iot_devices/drivers/i2c
    esp32_iot_devices/sensors/bmp280
    esp32_iot_devices/sensors/bme280
    esp32_iot_devices/sensors/bh1750
    esp32_iot_devices/sensors/ina226
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp32-modular-iot-firmware)
