# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22...3.28)
if(POLICY CMP0175)
    cmake_policy(SET CMP0175 OLD)
endif()

set(EXTRA_COMPONENT_DIRS
    common/app_errors
    common/app_types
    config
    driver/i2c_bus
    driver/sensor_drivers/aht20
    driver/sensor_drivers/bh1750
    driver/sensor_drivers/bme280
    driver/sensor_drivers/bmp280
    driver/sensor_drivers/bno055
    driver/sensor_drivers/ina226
    driver/sensor_drivers/sensor_driver_interface_manager
    driver/sensor_drivers/sensors_manager
    modules/BLE
    modules/ota
    modules/storage
    modules/Wifi
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(iot_firmware)
