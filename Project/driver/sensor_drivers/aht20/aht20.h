#include "driver_aht20_interface.h"

/**
 * @defgroup aht20_example_driver aht20 example driver function
 * @brief    aht20 example driver modules
 * @ingroup  aht20_driver
 * @{
 */

/**
 * @brief  basic example init
 * @return status code
 *         - 0 success
 *         - 1 init failed
 * @note   none
 */
esp_err_t aht20_full_init(void);

/**
 * @brief  basic example deinit
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
uint8_t aht20_app_deinit(void);

/**
 * @brief      basic example read
 * @param[out] *temperature pointer to a converted temperature buffer
 * @param[out] *humidity pointer to a converted humidity buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
uint8_t aht20_app_read(float *temperature, uint8_t *humidity);

/**         
 * @brief  test if the app work
 * @return none
 * @note   none
 */
void aht20_app_test(void *pvParameter);

/**
 * @brief      read humidity
 * @param[out] humidity pointer to a converted humidity buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 */
uint8_t aht20_app_read_hum(uint8_t *humidity);