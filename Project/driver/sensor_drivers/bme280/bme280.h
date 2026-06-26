#include "driver_bme280_interface.h"

/**
 * @brief bme280 basic example default definition
 */
#define BME280_DEFAULT_TEMPERATURE_OVERSAMPLING        BME280_OVERSAMPLING_x2            /**< temperatue oversampling x2 */
#define BME280_DEFAULT_PRESSURE_OVERSAMPLING           BME280_OVERSAMPLING_x16           /**< pressure oversampling x16 */
#define BME280_DEFAULT_HUMIDITY_OVERSAMPLING           BME280_OVERSAMPLING_x1            /**< humidity oversampling x1 */
#define BME280_DEFAULT_STANDBY_TIME                    BME280_STANDBY_TIME_0P5_MS        /**< standby time 0.5ms */
#define BME280_DEFAULT_FILTER                          BME280_FILTER_COEFF_16            /**< filter coeff 16 */
#define BME280_DEFAULT_SPI_WIRE                        BME280_SPI_WIRE_4                 /**< spi wire 4 */

/**
 * @brief     basic example init
 * @param[in] interface chip interface
 * @param[in] addr_pin chip address pin
 * @return    status code
 *            - 0 success
 *            - 1 init failed
 * @note      none
 */
esp_err_t bme280_full_init(bme280_interface_t interface, bme280_address_t addr_pin);

/**
 * @brief      basic example read
 * @param[out] *temperature pointer to a converted temperature buffer
 * @param[out] *pressure pointer to a converted pressure buffer
 * @param[out] *humidity_percentage pointer to a converted humidity percentage buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
uint8_t bme280_app_read(float *temperature, float *pressure, float *humidity_percentage);

/**
 * @brief  basic example deinit
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
uint8_t bme280_app_deinit(void);

/**         
 * @brief  init I2C interface
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
esp_err_t bme280_I2C_init(void);

/**         
 * @brief  test if the app work
 * @return none
 * @note   none
 */
void bme280_app_test(void);