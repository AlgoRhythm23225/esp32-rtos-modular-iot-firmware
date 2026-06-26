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
 * @file      driver_aht20_interface_template.c
 * @brief     driver aht20 interface template source file
 * @version   1.0.0
 * @author    Shifeng Li
 * @date      2022-10-31
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2022/10/31  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */

#include "driver_aht20_interface.h"

#define AHT20_ADDRESS      0x38

static i2c_master_dev_handle_t aht20_handle = NULL;
/**
 * @brief  interface iic bus init
 * @return status code
 *         - SENSOR_OK
 *         - SENSOR_BUS_ERR
 *         - SENSOR_HANDLE_NULL
 *         - SENSOR_ALREADY_INITIALIZED
 */
uint8_t aht20_interface_iic_init(void)
{
    return sensor_driver_interface_init(&aht20_handle, SENSOR_AHT20); 
}

/**
 * @brief  interface iic bus deinit
 * @return status code
 *         - SENSOR_OK
 *         - SENSOR_HANDLE_NULL
 *         - SENSOR_DEINIT_FAIL
 */
uint8_t aht20_interface_iic_deinit(void)
{
    return sensor_driver_interface_deinit(&aht20_handle, SENSOR_AHT20);
}

/**
 * @brief      interface iic bus read
 * @param[in]  addr iic device write address
 * @param[out] *buf pointer to a data buffer
 * @param[in]  len length of the data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
uint8_t aht20_interface_iic_read_cmd(uint8_t addr, uint8_t *buf, uint16_t len)
{
    return (i2c_read_sensor(aht20_handle, buf, len) == ESP_OK) ? SENSOR_OK : SENSOR_READ_FAIL;
}

/**
 * @brief     interface iic bus write
 * @param[in] addr iic device write address
 * @param[in] *buf pointer to a data buffer
 * @param[in] len length of the data buffer
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      none
 */
uint8_t aht20_interface_iic_write_cmd(uint8_t addr, uint8_t *buf, uint16_t len)
{
    return (i2c_write_sensor(aht20_handle, buf, len) == ESP_OK) ? SENSOR_OK : SENSOR_WRITE_FAIL;
}

/**
 * @brief     interface delay ms
 * @param[in] ms time
 * @note      none
 */
void aht20_interface_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/**
 * @brief     interface print format data
 * @param[in] fmt format data
 * @note      none
 */
void aht20_interface_debug_print(const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    esp_log_writev(ESP_LOG_ERROR, "BME280", fmt, args);

    va_end(args);
}
