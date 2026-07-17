#include "bno055.h"
#include "esp_log.h"
#include "rtos_config.h"
#include <stdio.h>
#include <string.h>
#include "math.h"
#include "sensor_types.h"
static const char *TAG = "bno055";

esp_err_t bno055_full_init() {
    bno055_interface_iic_init();
    uint8_t data = OPERATION_MODE_CONFIG;
    esp_err_t err = bno055_interface_iic_write(0x28, BNO055_OPR_MODE_ADDR, &data, 1);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Switch to config mode failed!");
    }

    // Reset
    data = BNO055_SYS_RESET;
    err = bno055_interface_iic_write(0x28, BNO055_SYS_TRIGGER_ADDR, &data, 1);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Reset failed!");
    }
    
    // Set power mode to normal 
    data = POWER_MODE_NORMAL;
    err = bno055_interface_iic_write(0x28, BNO055_PWR_MODE_ADDR, &data, 1);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Normal mode failed!");
    }

    bno055_interface_delay_ms(1000);
    err = bno055_set_opmode(OPERATION_MODE_NDOF);
    bno055_interface_delay_ms(1000);

    return ESP_OK;
}

void bno055_print_app_id() {
    uint8_t reg_val;
    esp_err_t err = bno055_interface_iic_read(0x28, BNO055_CHIP_ID_ADDR, &reg_val, 1);
    if (err == ESP_OK) {
        ESP_LOGI("bno055", "BNO055 ID returned 0x%02X", reg_val);
        if( reg_val == BNO055_ID ) {
            ESP_LOGI("bno055", "BNO055 detected \n");
        } 
        else {
            ESP_LOGE("bno055", "bno055_open() error: BNO055 NOT detected");
        }
    }
}

uint8_t bno055_accel_read(float *accel) {
    esp_err_t err;

    bno055_quaternion_t quat;
    bno055_vec3_t lin_accel;
    bno055_vec3_t gravity;

    err = bno055_get_fusion_data(0, &quat, &lin_accel, &gravity);
    if( err != ESP_OK ) {
        printf("bno055_get_fusion_data() returned error: %02x \n", err);
        return ESP_FAIL;
    }

    *accel = (float)sqrt(lin_accel.x * lin_accel.x +
                    lin_accel.y * lin_accel.y +
                    lin_accel.z * lin_accel.z);

    if(is_mqtt_on) {
        // Publish
        if (g_mqtt_publish_queue != NULL) {
            mqtt_publish_msg_t msg;
            memset(&msg, 0, sizeof(msg));
            strncpy(msg.topic, TOPIC_SENSOR, sizeof(msg.topic) - 1);
            msg.payload_len = (size_t)snprintf(
                msg.payload, sizeof(msg.payload),
                "{\"quat\":{\"w\":%.6f,\"x\":%.6f,\"y\":%.6f,\"z\":%.6f},"
                "\"lin_accel\":{\"x\":%.4f,\"y\":%.4f,\"z\":%.4f},"
                "\"gravity\":{\"x\":%.4f,\"y\":%.4f,\"z\":%.4f}}",
                (double)quat.w, (double)quat.x, (double)quat.y, (double)quat.z,
                (double)lin_accel.x, (double)lin_accel.y, (double)lin_accel.z,
                (double)gravity.x, (double)gravity.y, (double)gravity.z
            );
            xQueueSend(g_mqtt_publish_queue, &msg, 0);
        }
    }
    return ESP_OK;
}
  
 
