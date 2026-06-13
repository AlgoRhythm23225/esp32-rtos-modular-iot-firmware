#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#define NUM_PHILOSOPHERS 5

static const char *TAG = "DINING";

SemaphoreHandle_t chopstick[NUM_PHILOSOPHERS];

typedef struct
{
    int id;
} philosopher_t;

void philosopher_task(void *arg)
{
    philosopher_t *phil = (philosopher_t *)arg;

    int left = phil->id;
    int right = (phil->id + 1) % NUM_PHILOSOPHERS;

    int first = (left < right) ? left : right;
    int second = (left < right) ? right : left;

    while (1)
    {
        ESP_LOGI(TAG, "P%d thinking", phil->id);

        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "P%d wants chopsticks %d and %d",
                 phil->id,
                 left,
                 right);

        xSemaphoreTake(chopstick[first], portMAX_DELAY);

        /*
         * Giữ nguyên delay để cố tình tạo điều kiện deadlock
         * giống challenge của Shawn Hymel
         */
        vTaskDelay(pdMS_TO_TICKS(100));

        xSemaphoreTake(chopstick[second], portMAX_DELAY);

        ESP_LOGI(TAG, "P%d eating", phil->id);

        vTaskDelay(pdMS_TO_TICKS(500));

        xSemaphoreGive(chopstick[second]);
        xSemaphoreGive(chopstick[first]);

        ESP_LOGI(TAG, "P%d done eating", phil->id);
    }
}

// void app_main(void)
// {
//     static philosopher_t philosophers[NUM_PHILOSOPHERS];

//     for (int i = 0; i < NUM_PHILOSOPHERS; i++)
//     {
//         chopstick[i] = xSemaphoreCreateMutex();
//     }

//     for (int i = 0; i < NUM_PHILOSOPHERS; i++)
//     {
//         philosophers[i].id = i;

//         xTaskCreate(
//             philosopher_task,
//             "philosopher",
//             4096,
//             &philosophers[i],
//             5,
//             NULL);
//     }
// }