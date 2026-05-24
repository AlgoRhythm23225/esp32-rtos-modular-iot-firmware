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
 * @file      driver_bh1750fvi_interface_template.c
 * @brief     driver bh1750fvi interface template source file
 * @version   1.0.0
 * @author    Shifeng Li
 * @date      2022-11-30
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2022/11/30  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */

#include "driver_bh1750fvi_interface.h"

#define I2C_MASTER_SCL_IO   18
#define I2C_MASTER_SDA_IO   19
#define I2C_MASTER_NUM      I2C_NUM_1    
#define BH1750FVI_ADDRESS   BH1750FVI_ADDRESS_LOW

static i2c_master_dev_handle_t dev_handle = NULL;
static const char *TAG_I2C = "I2C";

/**
 * @brief  interface iic bus init
 * @return status code
 *         - 0 success
 *         - 1 iic init failed
 * @note   none
 */
uint8_t bh1750fvi_interface_iic_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_7,
        .device_address = BH1750FVI_ADDRESS,
        .scl_speed_hz = 100000,                                      // 100k Hhz
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));
    ESP_LOGI(TAG_I2C, "I2C initialization success from interface init!");
    return 0;
}

/**
 * @brief  interface iic bus deinit
 * @return status code
 *         - 0 success
 *         - 1 iic deinit failed
 * @note   none
 */
uint8_t bh1750fvi_interface_iic_deinit(void)
{
    return 0;
}

/**
 * @brief     interface iic bus write command
 * @param[in] addr iic device write address
 * @param[in] *buf pointer to a data buffer
 * @param[in] len length of data buffer
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      none
 */
uint8_t bh1750fvi_interface_iic_write_cmd(uint8_t addr, uint8_t *buf, uint16_t len)
{
    // uint8_t reg_addr = buf;
    // esp_err_t err = i2c_master_transmit_receive(dev_handle, &reg_addr, 1, buf, len, -1);
    esp_err_t err = i2c_master_transmit(dev_handle, buf, len, 500);

    return (err == ESP_OK) ? 0 : 1;
}

/**
 * @brief      interface iic bus read command
 * @param[in]  addr iic device write address
 * @param[out] *buf pointer to a data buffer
 * @param[in]  len length of data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
uint8_t bh1750fvi_interface_iic_read_cmd(uint8_t addr, uint8_t *buf, uint16_t len)
{
    esp_err_t err = i2c_master_receive(dev_handle, buf, len, 500);

    return (err == ESP_OK) ? 0 : 1;
}

/**
 * @brief     interface delay ms
 * @param[in] ms time
 * @note      none
 */
void bh1750fvi_interface_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/**
 * @brief     interface print format data
 * @param[in] fmt format data
 * @note      none
 */
void bh1750fvi_interface_debug_print(const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    esp_log_writev(ESP_LOG_INFO, "BME280", fmt, args);

    va_end(args);
}
