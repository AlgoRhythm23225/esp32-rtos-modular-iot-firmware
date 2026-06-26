#include <stdint.h>
#include "i2c_error.h"

#define I2C_RETRY_MAX_MS 30000U

static uint16_t i2c_retry_interval = 1500;

// Increase the retry interval exponentially after each failed attempt
uint16_t i2c_next_exponential_backoff() {
    i2c_retry_interval = ((i2c_retry_interval * 2) > I2C_RETRY_MAX_MS) 
                        ? I2C_RETRY_MAX_MS 
                        : i2c_retry_interval * 2;
    
    return i2c_retry_interval;
}

// Reset the exponential backoff interval for each new sensor initialization
uint16_t i2c_reset_exponential_backoff() {
    i2c_retry_interval = 1500;

    return i2c_retry_interval;
}

// Convert I2C device address to device name
const char *i2c_addr_to_name(uint8_t dev_addr) {
    switch(dev_addr) {
        case 0x38:
            return "AHT20";
        case 0x23:
            return "BH1750";
        case 0x76:
            return "BMP280";
        case 0x29:
            return "BNO055";
        case 0x40:
            return "INA226";
        default:
            return "Unknown";
    }
}