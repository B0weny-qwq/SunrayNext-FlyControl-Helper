#include "app/service/app_boot.h"

#include <stdio.h>

#include "console_app.h"
#include "bridge/adapter/mavlink_parser.h"
#include "bridge/service/bridge_runtime.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_app.h"

static const char *TAG = "app_boot";
static mavlink_bridge_handle_t s_bridge = NULL;

static void uart_data_callback(uint8_t uart_id, const uint8_t *data, size_t len, void *ctx) {
    (void)ctx;
    ESP_LOGI(TAG, "[%s] Rx: %d bytes", mavlink_get_uart_name(uart_id), len);

    if (MAVLINK_LOG_HEXDUMP && len > 0) {
        printf("RAW: ");
        for (size_t i = 0; i < len && i < 32; i++) {
            printf("%02X ", data[i]);
        }
        printf("\r\n");
    }

    if (mavlink_is_frame(data, len)) {
        ESP_LOGI(
            TAG,
            "  MAVLink: SEQ=%d, MSG=0x%02X, LEN=%d",
            mavlink_get_sequence(data),
            mavlink_get_msg_id(data),
            mavlink_get_payload_len(data));
    }
}

static void net_data_callback(uint8_t uart_id, const uint8_t *data, size_t len, void *ctx) {
    (void)uart_id;
    (void)ctx;
    ESP_LOGI(TAG, "[NET] Rx: %d bytes", len);
    if (mavlink_is_frame(data, len)) {
        ESP_LOGI(TAG, "  MAVLink: SEQ=%d, MSG=0x%02X", mavlink_get_sequence(data), mavlink_get_msg_id(data));
    }
}

static esp_err_t init_bridge(void) {
    mavlink_bridge_config_t config = MAVLINK_BRIDGE_CONFIG_DEFAULT();
    esp_err_t err = mavlink_bridge_init(&config, &s_bridge);
    if (err != ESP_OK) {
        return err;
    }

    mavlink_bridge_on_net_data(s_bridge, net_data_callback, NULL);
    for (int i = 0; i < UART_ID_MAX; i++) {
        mavlink_bridge_on_uart_data(s_bridge, i, uart_data_callback, NULL);
    }

    char local_ip[16];
    if (mavlink_bridge_get_local_ip(s_bridge, local_ip, sizeof(local_ip)) == ESP_OK) {
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, " P4 WiFi Remote - MAVLink Bridge");
        ESP_LOGI(TAG, " Listen: UDP %s:%d, TCP %s:%d", local_ip, MAVLINK_UDP_PORT, local_ip, MAVLINK_TCP_PORT);
#if MAVLINK_TELEM1_ENABLED
        ESP_LOGI(
            TAG,
            " TELEM1: %d baud, TX=%d, RX=%d",
            MAVLINK_TELEM1_BAUD,
            MAVLINK_TELEM1_TX_PIN,
            MAVLINK_TELEM1_RX_PIN);
#endif
#if MAVLINK_TELEM2_ENABLED
        ESP_LOGI(
            TAG,
            " TELEM2: %d baud, TX=%d, RX=%d",
            MAVLINK_TELEM2_BAUD,
            MAVLINK_TELEM2_TX_PIN,
            MAVLINK_TELEM2_RX_PIN);
#endif
#if MAVLINK_DEBUG_ENABLED
        ESP_LOGI(
            TAG,
            " DEBUG:  %d baud, TX=%d, RX=%d",
            MAVLINK_DEBUG_BAUD,
            MAVLINK_DEBUG_TX_PIN,
            MAVLINK_DEBUG_RX_PIN);
#endif
        ESP_LOGI(TAG, "========================================");
    }

    return ESP_OK;
}

esp_err_t app_boot_init(void) {
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_ERROR_CHECK(wifi_app_init());
    ESP_ERROR_CHECK(init_bridge());
    console_app_init();
    return ESP_OK;
}
