#include "driver_ina226_interface.h"

/**
 * @defgroup ina226_example_driver ina226 example driver function
 * @brief    ina226 example driver modules
 * @ingroup  ina226_driver
 * @{
 */

/**
 * @brief ina226 basic example default definition
 */
#define INA226_BASIC_DEFAULT_AVG_MODE                             INA226_AVG_16                        /**< 16 averages */
#define INA226_BASIC_DEFAULT_BUS_VOLTAGE_CONVERSION_TIME          INA226_CONVERSION_TIME_1P1_MS        /**< bus voltage conversion time 1.1 ms */
#define INA226_BASIC_DEFAULT_SHUNT_VOLTAGE_CONVERSION_TIME        INA226_CONVERSION_TIME_1P1_MS        /**< shunt voltage conversion time 1.1 ms */

/**
 * @brief     basic example init
 * @param[in] addr_pin iic address pin
 * @param[in] r reference resistor value
 * @return    status code
 *            - 0 success
 *            - 1 init failed
 * @note      none
 */
uint8_t ina226_app_init(ina226_address_t addr_pin, double r);

/**
 * @brief  basic example deinit
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
uint8_t ina226_app_deinit(void);

/**
 * @brief      basic example read
 * @param[out] *mV pointer to a mV buffer
 * @param[out] *mA pointer to a mA buffer
 * @param[out] *mW pointer to a mW buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
uint8_t ina226_app_read(float *mV, float *mA, float *mW);

/**         
 * @brief  init I2C interface
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
uint8_t ina226_I2C_init(void);

/**         
 * @brief  test if the app work
 * @return none
 * @note   none
 */
void ina226_app_test(void *pvParameter);


