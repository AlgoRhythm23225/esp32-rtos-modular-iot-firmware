#include "bme280.h"

static bme280_handle_t gs_handle;               // bme280 handle

/**
 * @brief     init
 * @param[in] interface chip interface
 * @param[in] addr_pin chip address pin
 * @return    status code
 *            - 0 success
 *            - 1 init failed
 * @note      none
 */
uint8_t bme280_app_init(bme280_interface_t interface, bme280_address_t addr_pin) {
    uint8_t res;
    uint8_t check = 0;
    // link interface function
    DRIVER_BME280_LINK_INIT(&gs_handle, bme280_handle_t); 
    DRIVER_BME280_LINK_IIC_INIT(&gs_handle, bme280_interface_iic_init);
    DRIVER_BME280_LINK_IIC_DEINIT(&gs_handle, bme280_interface_iic_deinit);
    DRIVER_BME280_LINK_IIC_READ(&gs_handle, bme280_interface_iic_read);
    DRIVER_BME280_LINK_IIC_WRITE(&gs_handle, bme280_interface_iic_write);
    DRIVER_BME280_LINK_SPI_INIT(&gs_handle, bme280_interface_spi_init);
    DRIVER_BME280_LINK_SPI_DEINIT(&gs_handle, bme280_interface_spi_deinit);
    DRIVER_BME280_LINK_SPI_READ(&gs_handle, bme280_interface_spi_read);
    DRIVER_BME280_LINK_SPI_WRITE(&gs_handle, bme280_interface_spi_write);
    DRIVER_BME280_LINK_DELAY_MS(&gs_handle, bme280_interface_delay_ms);
    DRIVER_BME280_LINK_DEBUG_PRINT(&gs_handle, bme280_interface_debug_print);
    bme280_interface_debug_print("LINKING SUCCESS!\n");
    
    // set interface
    res = bme280_set_interface(&gs_handle, interface);
    if (res != 0) {
        bme280_interface_debug_print("bme280: set addr pin failed.\n");
        
        return 1;
    }
    bme280_interface_debug_print("INTERFACE SUCCESS!\n");
    check++;

    // set interface
    res = bme280_set_addr_pin(&gs_handle, addr_pin);
    if (res != 0) {
        bme280_interface_debug_print("bme280: set addr pin failed.\n");

        return 1;
    }
    bme280_interface_debug_print("SET ADDR PIN SUCCESS!\n");
    check++;

    // bme280 init
    res = bme280_init(&gs_handle);
    if (res != 0) {
        bme280_interface_debug_print("bme280: init failed.\n");

        return 1;
    }
    bme280_interface_debug_print("BME280 INIT SUCCESS!\n");
    check++;

    // set defaut temperature oversampling
    res = bme280_set_temperatue_oversampling(&gs_handle, BME280_DEFAULT_TEMPERATURE_OVERSAMPLING);
    if (res != 0) {
        bme280_interface_debug_print("bme280: set temperature oversampling failed.\n");
        (void)bme280_deinit(&gs_handle);

        return 1;
    }    
    bme280_interface_debug_print("TEMP OVERSAMPLING SUCCESS!\n");
    check++;

    // set defaut pressure oversampling
    res = bme280_set_pressure_oversampling(&gs_handle, BME280_DEFAULT_PRESSURE_OVERSAMPLING);
    if (res != 0) {
        bme280_interface_debug_print("bme280: set pressure oversampling failed.\n");
        (void)bme280_deinit(&gs_handle);

        return 1;
    }   
    bme280_interface_debug_print("PRESS OVERSAMPLING SUCCESS!\n");
    check++;

    // set defaut humidity oversampling
    res = bme280_set_humidity_oversampling(&gs_handle, BME280_DEFAULT_HUMIDITY_OVERSAMPLING);
    if (res != 0) {
        bme280_interface_debug_print("bme280: set humidity oversampling failed.\n");
        (void)bme280_deinit(&gs_handle);

        return 1;
    }  
    bme280_interface_debug_print("HUM OVERSAMPLING SUCCESS!\n");
    check++; 

    // set defaut filter
    res = bme280_set_filter(&gs_handle, BME280_DEFAULT_FILTER);
    if (res != 0) {
        bme280_interface_debug_print("bme280: set filter failed.\n");
        (void)bme280_deinit(&gs_handle);

        return 1;
    }   
    bme280_interface_debug_print("SET FILTER SUCCESS!\n");
    check++;

    // set defaut humidity oversampling
    res = bme280_set_mode(&gs_handle, BME280_MODE_NORMAL);
    if (res != 0) {
        bme280_interface_debug_print("bme280: set mode failed.\n");
        (void)bme280_deinit(&gs_handle);

        return 1;
    }  
    bme280_interface_debug_print("SET MODE SUCCESS!\n");
    check++; 

    ESP_LOGI("BME280", "%d/8", check);
    ESP_LOGI("bme280", "Init SUCCESS!");

    return 0;
}

/**
 * @brief      read
 * @param[out] *temperature pointer to a converted temperature buffer
 * @param[out] *pressure pointer to a converted pressure buffer
 * @param[out] *humidity_percentage pointer to a converted humidity percentage buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
uint8_t bme280_app_read(float *temperature, float *pressure, float *humidity_percentage) {
    uint32_t temperature_raw;
    uint32_t pressure_raw;
    uint32_t humidity_raw;

    // read temperature, pressure and humidity
    if (bme280_read_temperature_pressure_humidity(&gs_handle, 
                                                (uint32_t*)&temperature_raw, temperature, 
                                                (uint32_t*)&pressure_raw, pressure, 
                                                (uint32_t*)&humidity_raw, humidity_percentage) != 0)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief  deinit
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
uint8_t bme280_app_deinit(void) {
    if (bme280_deinit(&gs_handle) != 0) {
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
uint8_t bme280_I2C_init(void) {
    uint8_t res;
    res = bme280_app_init(BME280_INTERFACE_IIC, 0x77);
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
void bme280_app_test(void) {
    bme280_I2C_init();

    float temperature;
    float pressure;
    float humidity;

    bme280_app_read(&temperature, &pressure, &humidity);
    
    printf("Temperature: %f.2\n", temperature);
    printf("Pressure: %f.2\n", pressure);
    printf("humidity: %f.2%%\n", humidity);
}