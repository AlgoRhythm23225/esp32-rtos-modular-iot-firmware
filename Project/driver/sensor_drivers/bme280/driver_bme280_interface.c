/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 * 
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. 
 *
 * @file      driver_bme280_interface_template.c
 * @brief     driver bme280 interface template source file
 * @version   1.0.0
 * @author    Shifeng Li
 * @date      2024-01-15
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2024/01/15  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */

/** 
 * @brief I2C hardware defination
 */
#include "driver_bme280_interface.h"

#define BME280_ADDRESS      0x76

static i2c_master_dev_handle_t bme280_handle = NULL;

/**
 * @brief  interface iic bus init
 */
uint8_t bme280_interface_iic_init(void)
{
    // I currently don't have BME280 so I temporary use bmp280 name instead
    return sensor_driver_interface_init(&bme280_handle, SENSOR_BMP280);
}

/**
 * @brief  interface iic bus deinit
 */
uint8_t bme280_interface_iic_deinit(void)
{   
    return sensor_driver_interface_deinit(&bme280_handle, SENSOR_BMP280);
}

/**
 * @brief      interface iic bus read
 * @param[in]  addr iic device write address
 * @param[in]  reg iic register address
 * @param[out] *buf pointer to a data buffer
 * @param[in]  len length of the data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
uint8_t bme280_interface_iic_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint8_t reg_addr = reg;
    return (i2c_write_read_sensor(bme280_handle, &reg_addr, 1, buf, len) == ESP_OK) ? SENSOR_OK : SENSOR_READ_FAIL;
}

/**
 * @brief     interface iic bus write
 * @param[in] addr iic device write address
 * @param[in] reg iic register address
 * @param[in] *buf pointer to a data buffer
 * @param[in] len length of the data buffer
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      none
 */
uint8_t bme280_interface_iic_write(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint8_t write_buf[len + 1];
    write_buf[0] = reg;

    for (uint16_t i = 0; i < len; i++) {
        write_buf[i + 1] = buf[i];
    }

    return (i2c_write_sensor(bme280_handle, write_buf, len + 1) == ESP_OK) ? SENSOR_OK : SENSOR_WRITE_FAIL;
}

/**
 * @brief  interface spi bus init
 * @return status code
 *         - 0 success
 *         - 1 spi init failed
 * @note   none
 */
uint8_t bme280_interface_spi_init(void)
{
    return SENSOR_OK;
}

/**
 * @brief  interface spi bus deinit
 * @return status code
 *         - 0 success
 *         - 1 spi deinit failed
 * @note   none
 */
uint8_t bme280_interface_spi_deinit(void)
{   
    return SENSOR_OK;
}

/**
 * @brief      interface spi bus read
 * @param[in]  reg register address
 * @param[out] *buf pointer to a data buffer
 * @param[in]  len length of data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
uint8_t bme280_interface_spi_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return SENSOR_OK;
}

/**
 * @brief     interface spi bus write
 * @param[in] reg register address
 * @param[in] *buf pointer to a data buffer
 * @param[in] len length of data buffer
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      none
 */
uint8_t bme280_interface_spi_write(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return SENSOR_OK;
}

/**
 * @brief     interface delay ms
 * @param[in] ms time
 * @note      none
 */
void bme280_interface_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/**
 * @brief     interface print format data
 * @param[in] fmt format data
 * @note      none
 */
void bme280_interface_debug_print(const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    esp_log_writev(ESP_LOG_INFO, "BME280", fmt, args);

    va_end(args);
}

