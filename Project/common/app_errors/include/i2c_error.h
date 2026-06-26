#ifndef I2C_ERROR_H
#define I2C_ERROR_H

// Increase the retry interval exponentially after each failed attempt
uint16_t i2c_next_exponential_backoff();

// Reset the exponential backoff interval for each new sensor initialization
uint16_t i2c_reset_exponential_backoff();

// Convert I2C device address to device name
const char *i2c_addr_to_name(uint8_t dev_addr);

#endif