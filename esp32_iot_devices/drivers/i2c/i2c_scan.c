#include "i2c_scan.h"

static const char *TAG = "SCAN";
static i2c_master_bus_handle_t bus_handle;

void i2c_scan_init()
{
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM_SCAN,
        .sda_io_num = I2C_MASTER_SDA_IO_SCAN,
        .scl_io_num = I2C_MASTER_SCL_IO_SCAN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(
        i2c_new_master_bus(&bus_cfg, &bus_handle)
    );
}

void i2c_scan()
{
    i2c_scan_init();
    for (int i = 0; i < 127; i++) {
        esp_err_t ret = i2c_master_probe(bus_handle, i, -1);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "BNO055 found at %d", i);
        } else {
            ESP_LOGW(TAG, "Not found at %d", i);
        }
    }
}