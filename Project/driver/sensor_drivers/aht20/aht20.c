#include "aht20.h"

static aht20_handle_t gs_handle;        /**< aht20 handle */

/**
 * @brief  basic example init
 * @return status code
 *         - 0 success
 *         - 1 init failed
 */
esp_err_t aht20_full_init(void)
{
    uint8_t res;
    
    /* link interface function */
    DRIVER_AHT20_LINK_INIT(&gs_handle, aht20_handle_t);
    DRIVER_AHT20_LINK_IIC_INIT(&gs_handle, aht20_interface_iic_init);
    DRIVER_AHT20_LINK_IIC_DEINIT(&gs_handle, aht20_interface_iic_deinit);
    DRIVER_AHT20_LINK_IIC_READ_CMD(&gs_handle, aht20_interface_iic_read_cmd);
    DRIVER_AHT20_LINK_IIC_WRITE_CMD(&gs_handle, aht20_interface_iic_write_cmd);
    DRIVER_AHT20_LINK_DELAY_MS(&gs_handle, aht20_interface_delay_ms);
    DRIVER_AHT20_LINK_DEBUG_PRINT(&gs_handle, aht20_interface_debug_print);
    
    /* aht20 init */
    res = aht20_init(&gs_handle);
    if (res != 0)
    {
        aht20_interface_debug_print("aht20: init failed, res = %u.\n", res); 
        
        return SENSOR_BUS_ERR;
    }
    
    return SENSOR_OK;
}

/**
 * @brief      basic example read
 * @param[out] temperature pointer to a converted temperature buffer
 * @param[out] humidity pointer to a converted humidity buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 */
uint8_t aht20_app_read_all(float *temperature, uint8_t *humidity)
{
    uint32_t temperature_raw;
    uint32_t humidity_raw;
    /* read temperature and humidity */
    if (aht20_read_temperature_humidity(&gs_handle, (uint32_t *)&temperature_raw, temperature, 
                                       (uint32_t *)&humidity_raw, humidity) != 0)
    {
        return SENSOR_READ_FAIL;
    }
    else
    {
        return SENSOR_OK;
    }
}

/**
 * @brief      read humidity
 * @param[out] humidity pointer to a converted humidity buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 */
uint8_t aht20_app_read_hum(uint8_t *humidity)
{
    uint32_t humidity_raw;
    /* read humidity */
    if (aht20_read_humidity(&gs_handle, &humidity_raw, humidity) != 0) {
        return SENSOR_READ_FAIL;
    }
    else {
        return SENSOR_OK;
    }
}

/**
 * @brief  basic example deinit
 * @return status code
 *         - 0 success
 * @note   none
 */
uint8_t aht20_app_deinit(void)
{
    /* deinit aht20 and close bus */
    if (aht20_deinit(&gs_handle) != 0)
    {
        return SENSOR_DEINIT_FAIL;
    }
    else
    {
        return SENSOR_OK;
    }
}

/**         
 * @brief  test if the app work
 * @return none
 * @note   none
 */
void aht20_app_test(void *pvParameter) {
    aht20_full_init();

    float temperature;
    uint8_t humidity;

    while(1) {
        if (aht20_app_read_all(&temperature, &humidity) == 0) {
            // printf("aht20: Temperature: %.2f\n", temperature);
            printf("aht20: Humidity: %d %%\n", humidity);
        } else {
            printf("Read data from AHT20 failed!\n");
        }

        aht20_interface_delay_ms(1000);
    }
}