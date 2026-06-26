#include "driver_bmp280_interface.h"

/**
 * @brief bmp280 basic example default definition
 */
#define BMP280_DEFAULT_TEMPERATURE_OVERSAMPLING        BMP280_OVERSAMPLING_x2            /**< temperatue oversampling x2 */
#define BMP280_DEFAULT_PRESSURE_OVERSAMPLING           BMP280_OVERSAMPLING_x16           /**< pressure oversampling x16 */
#define BMP280_DEFAULT_STANDBY_TIME                    BMP280_STANDBY_TIME_0P5_MS        /**< standby time 0.5ms */
#define BMP280_DEFAULT_FILTER                          BMP280_FILTER_COEFF_16            /**< filter coeff 16 */
#define BMP280_DEFAULT_SPI_WIRE                        BMP280_SPI_WIRE_4                 /**< spi wire 4 */

/**
 * @brief     basic example init
 * @param[in] interface chip interface
 * @param[in] addr_pin chip address pin
 * @return    status code
 *            - 0 success
 *            - 1 init failed
 * @note      none
 */
esp_err_t bmp280_app_init(bmp280_interface_t interface, bmp280_address_t addr_pin);

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
uint8_t bmp280_app_read(float *temperature, float *pressure);

/**
 * @brief  basic example deinit
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
uint8_t bmp280_app_deinit(void);

/**         
 * @brief  init I2C interface
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
esp_err_t bmp280_full_init(void);

/**         
 * @brief  test if the app work
 * @return none
 * @note   none
 */
void bmp280_app_test(void *pvParameter);