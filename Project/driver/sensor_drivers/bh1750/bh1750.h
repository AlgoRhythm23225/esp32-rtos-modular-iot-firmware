#ifndef BH1750_H
#define BH1750_H

#include "driver_bh1750fvi_interface.h"

/**
 * @brief bh1750fvi basic example default definition
 */
#define BH1750FVI_BASIC_DEFAULT_MODE                    BH1750FVI_MODE_HIGH_RESOLUTION_MODE        /**< high resolution mode */
#define BH1750FVI_BASIC_DEFAULT_MEASUREMENT_TIME        69                                         /**< measurement time 69 */

/**
 * @brief     basic example init
 * @param[in] addr_pin iic device address
 * @return    status code
 *            - 0 success
 *            - 1 init failed
 * @note      none
 */
uint8_t bh1750fvi_basic_init(bh1750fvi_address_t addr_pin);

/**
 * @brief      basic example read
 * @param[out] *lux pointer to a converted lux buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
uint8_t bh1750fvi_basic_read(float *lux);

/**
 * @brief  basic example deinit
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
uint8_t bh1750fvi_basic_deinit(void);

/**         
 * @brief  test if the app work
 * @return none
 * @note   none
 */
void bh1750_app_test(void *pvParameter);

#endif