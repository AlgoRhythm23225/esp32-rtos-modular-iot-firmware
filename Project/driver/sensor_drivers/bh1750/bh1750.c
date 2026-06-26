#include "bh1750.h"

static bh1750fvi_handle_t gs_handle; 

/**
 * @brief     basic example init
 * @param[in] addr_pin iic device address
 * @return    status code
 *            - 0 success
 *            - 1 init failed
 * @note      none
 */
esp_err_t bh1750fvi_full_init() {
    uint8_t res;
    
    /* link interface function */
    DRIVER_BH1750FVI_LINK_INIT(&gs_handle, bh1750fvi_handle_t);
    DRIVER_BH1750FVI_LINK_IIC_INIT(&gs_handle, bh1750fvi_interface_iic_init);
    DRIVER_BH1750FVI_LINK_IIC_DEINIT(&gs_handle, bh1750fvi_interface_iic_deinit);
    DRIVER_BH1750FVI_LINK_IIC_READ_CMD(&gs_handle, bh1750fvi_interface_iic_read_cmd);
    DRIVER_BH1750FVI_LINK_IIC_WRITE_CMD(&gs_handle, bh1750fvi_interface_iic_write_cmd);
    DRIVER_BH1750FVI_LINK_DELAY_MS(&gs_handle, bh1750fvi_interface_delay_ms);
    DRIVER_BH1750FVI_LINK_DEBUG_PRINT(&gs_handle, bh1750fvi_interface_debug_print);

    /* set the addr pin */
    res = bh1750fvi_set_addr_pin(&gs_handle, BH1750FVI_ADDRESS_LOW);
    if (res != 0)
    {
        bh1750fvi_interface_debug_print("bh1750fvi: set addr pin failed.\n");
       
        return SENSOR_WRITE_FAIL;
    }

    /* init */
    res = bh1750fvi_init(&gs_handle);
    if (res != 0)
    {
        bh1750fvi_interface_debug_print("bh1750fvi: init failed.\n");
       
        return SENSOR_HARDWARE_ERR;
    }

    /* power on */
    res = bh1750fvi_power_on(&gs_handle);
    if (res != 0)
    {
        bh1750fvi_interface_debug_print("bh1750fvi: power on failed.\n");
        (void)bh1750fvi_deinit(&gs_handle);
       
        return SENSOR_POWER_CONFIG_FAIL;
    }
    
    /* set mode */
    res = bh1750fvi_set_mode(&gs_handle, BH1750FVI_BASIC_DEFAULT_MODE);
    if (res != 0)
    {
        bh1750fvi_interface_debug_print("bh1750fvi: set mode failed.\n");
        (void)bh1750fvi_deinit(&gs_handle);
       
        return SENSOR_SET_FAIL;
    }

    /* set measurement time */
    res = bh1750fvi_set_measurement_time(&gs_handle, BH1750FVI_BASIC_DEFAULT_MEASUREMENT_TIME);
    if (res != 0)
    {
        bh1750fvi_interface_debug_print("bh1750fvi: set measurement time failed.\n");
        (void)bh1750fvi_deinit(&gs_handle);
       
        return SENSOR_SET_FAIL;
    }
    
    /* start continuous read */
    res = bh1750fvi_start_continuous_read(&gs_handle);
    if (res != 0)
    {
        bh1750fvi_interface_debug_print("bh1750fvi: start continuous read failed.\n");
        (void)bh1750fvi_deinit(&gs_handle);
       
        return SENSOR_SET_FAIL;
    }
    
    return SENSOR_OK;
}

/**
 * @brief      basic example read
 * @param[out] lux pointer to a converted lux buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
uint8_t bh1750fvi_basic_read(float *lux)
{
    uint8_t res;
    uint16_t raw;
    
    /* continuous read */
    res = bh1750fvi_continuous_read(&gs_handle, &raw, lux);
    if (res != 0)
    {
        return SENSOR_READ_FAIL;
    }
    else
    {
        return SENSOR_OK;
    }
}

/**
 * @brief  basic example deinit
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
uint8_t bh1750fvi_basic_deinit(void)
{
    uint8_t res;
    
    /* stop continuous read */
    res = bh1750fvi_stop_continuous_read(&gs_handle);
    if (res != 0)
    {
        return SENSOR_SHUT_FAIL;
    }
    
    /* deinit */
    res = bh1750fvi_deinit(&gs_handle);
    if (res != 0)
    {
        return SENSOR_DEINIT_FAIL;
    }
    
    return SENSOR_OK;
}

/**         
 * @brief  test if the app work
 * @return none
 * @note   none
 */
void bh1750_app_test(void *pvParameter) {
    bh1750fvi_full_init();

    float lux = 0.0f;

    while(1) {
        if (bh1750fvi_basic_read(&lux) == 0) {
            printf("Lightness: %.2f Lux\n", lux);
        } else {
            printf("Doc du lieu BH1750 that bai!\n");
        }

        bh1750fvi_interface_delay_ms(1000);
    }
}