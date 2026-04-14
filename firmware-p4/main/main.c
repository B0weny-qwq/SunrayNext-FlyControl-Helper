#include "app/service/app_boot.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

void app_main(void) {
    ESP_LOGI(TAG, "P4 WiFi Remote startup");
    ESP_ERROR_CHECK(app_boot_init());
    ESP_LOGI(TAG, "Console ready, input help for commands");
    vTaskSuspend(NULL);
}
