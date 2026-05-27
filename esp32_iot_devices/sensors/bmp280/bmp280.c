#include "bmp280.h"

static bmp280_handle_t gs_handle;        //bmp280 handle

/**
 * @brief     basic example init
 * @param[in] interface chip interface
 * @param[in] addr_pin chip address pin
 * @return    status code
 *            - 0 success
 *            - 1 init failed
 * @note      none
 */
uint8_t bmp280_app_init(bmp280_interface_t interface, bmp280_address_t addr_pin)
{
    uint8_t res;
    
    /* link interface function */
    DRIVER_BMP280_LINK_INIT(&gs_handle, bmp280_handle_t); 
    DRIVER_BMP280_LINK_IIC_INIT(&gs_handle, bmp280_interface_iic_init);
    DRIVER_BMP280_LINK_IIC_DEINIT(&gs_handle, bmp280_interface_iic_deinit);
    DRIVER_BMP280_LINK_IIC_READ(&gs_handle, bmp280_interface_iic_read);
    DRIVER_BMP280_LINK_IIC_WRITE(&gs_handle, bmp280_interface_iic_write);
    DRIVER_BMP280_LINK_SPI_INIT(&gs_handle, bmp280_interface_spi_init);
    DRIVER_BMP280_LINK_SPI_DEINIT(&gs_handle, bmp280_interface_spi_deinit);
    DRIVER_BMP280_LINK_SPI_READ(&gs_handle, bmp280_interface_spi_read);
    DRIVER_BMP280_LINK_SPI_WRITE(&gs_handle, bmp280_interface_spi_write);
    DRIVER_BMP280_LINK_DELAY_MS(&gs_handle, bmp280_interface_delay_ms);
    DRIVER_BMP280_LINK_DEBUG_PRINT(&gs_handle, bmp280_interface_debug_print);
    
    /* set interface */
    res = bmp280_set_interface(&gs_handle, interface);
    if (res != 0)
    {
        bmp280_interface_debug_print("bmp280: set interface failed.\n");
       
        return 1;
    }
    
    /* set addr pin */
    res = bmp280_set_addr_pin(&gs_handle, addr_pin);
    if (res != 0)
    {
        bmp280_interface_debug_print("bmp280: set addr pin failed.\n");
       
        return 1;
    }
    
    /* bmp280 init */
    res = bmp280_init(&gs_handle);
    if (res != 0)
    {
        bmp280_interface_debug_print("bmp280: init failed.\n");
        
        return 1;
    }
    
    /* set default temperatue oversampling */
    res = bmp280_set_temperatue_oversampling(&gs_handle, BMP280_DEFAULT_TEMPERATURE_OVERSAMPLING);
    if (res != 0)
    {
        bmp280_interface_debug_print("bmp280: set temperatue oversampling failed.\n");
        (void)bmp280_deinit(&gs_handle);
        
        return 1;
    }
    
    /* set default pressure oversampling */
    res = bmp280_set_pressure_oversampling(&gs_handle, BMP280_DEFAULT_PRESSURE_OVERSAMPLING);
    if (res != 0)
    {
        bmp280_interface_debug_print("bmp280: set pressure oversampling failed.\n");
        (void)bmp280_deinit(&gs_handle);
        
        return 1;
    }
    
    /* set default standby time */
    res = bmp280_set_standby_time(&gs_handle, BMP280_DEFAULT_STANDBY_TIME);
    if (res != 0)
    {
        bmp280_interface_debug_print("bmp280: set standby time failed.\n");
        (void)bmp280_deinit(&gs_handle);
        
        return 1;
    }
    
    /* set default filter */
    res = bmp280_set_filter(&gs_handle, BMP280_DEFAULT_FILTER);
    if (res != 0)
    {
        bmp280_interface_debug_print("bmp280: set filter failed.\n");
        (void)bmp280_deinit(&gs_handle);
        
        return 1;
    }
    
    /* set default spi wire */
    res = bmp280_set_spi_wire(&gs_handle, BMP280_DEFAULT_SPI_WIRE);
    if (res != 0)
    {
        bmp280_interface_debug_print("bmp280: set spi wire failed.\n");
        (void)bmp280_deinit(&gs_handle);
        
        return 1;
    }
    
    /* set normal mode */
    res = bmp280_set_mode(&gs_handle, BMP280_MODE_NORMAL);
    if (res != 0)
    {
        bmp280_interface_debug_print("bmp280: set mode failed.\n");
        (void)bmp280_deinit(&gs_handle);
        
        return 1;
    }
    
    return 0;
}

/**
 * @brief      basic example read
 * @param[out] *temperature pointer to a converted temperature buffer
 * @param[out] *pressure pointer to a converted pressure buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
uint8_t bmp280_app_read(float *temperature, float *pressure)
{
    uint32_t temperature_yaw;
    uint32_t pressure_yaw;
    
    /* read temperature and pressure */
    if (bmp280_read_temperature_pressure(&gs_handle, (uint32_t *)&temperature_yaw, 
                                         temperature, (uint32_t *)&pressure_yaw, pressure) != 0)
    {
        return 1;
    }
    
    return 0;
}

/**
 * @brief  basic example deinit
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
uint8_t bmp280_app_deinit(void)
{
    /* close bmp280 */
    if (bmp280_deinit(&gs_handle) != 0)
    {
        return 1;
    }
    
    return 0;
}

/**         
 * @brief  init I2C interface
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
uint8_t bmp280_I2C_init(void) {
    uint8_t res;
    res = bmp280_app_init(BMP280_INTERFACE_IIC, 0x76);
    if (res != 0) {
        return 1;
    }
    
    return 0;
}

/**         
 * @brief  test if the app work
 * @return none
 * @note   none
 */
void bmp280_app_test(void *pvParameter) {
    bmp280_I2C_init();

    float temperature;
    float pressure;

    while (1) {     
        bmp280_app_read(&temperature, &pressure);
        
        printf("Temperature: %.2f\n", temperature);
        printf("Pressure: %.2f\n", pressure);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}