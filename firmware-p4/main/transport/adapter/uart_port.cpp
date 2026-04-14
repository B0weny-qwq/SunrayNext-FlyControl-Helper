#include "transport/adapter/uart_port.h"

#include <stdio.h>

#include "esp_idf_version.h"
#include "esp_log.h"

static const char *TAG = "bridge_uart";

static void uart_rx_task(void *arg) {
    auto *port_info = static_cast<mavlink_uart_port_info_t *>(arg);
    uint8_t buffer[MAVLINK_MAX_FRAME_LEN];

    ESP_LOGI(TAG, "UART%d Rx task started", port_info->id);
    while (true) {
        const int len = uart_read_bytes(port_info->port_num, buffer, sizeof(buffer), pdMS_TO_TICKS(10));
        if (len > 0) {
            bridge_runtime_handle_uart_rx(port_info, buffer, (size_t)len);
        }
        taskYIELD();
    }
}

esp_err_t bridge_uart_init(const mavlink_uart_config_t *config, mavlink_uart_port_info_t *port_info) {
    if (!config || !port_info) {
        return ESP_ERR_INVALID_ARG;
    }

    port_info->id = config->id;
    port_info->port_num = (uart_port_t)config->id;
    port_info->baud_rate = config->baud_rate;
    port_info->tx_pin = config->tx_pin;
    port_info->rx_pin = config->rx_pin;
    port_info->enabled = config->enabled;
    port_info->name = config->name;
    port_info->callback = config->data_callback;
    port_info->callback_ctx = config->callback_ctx;
    port_info->mavlink_mode = config->mavlink_mode;
    port_info->sysid_filter = config->sysid_filter;
    port_info->rx_task = NULL;
    port_info->tx_bytes = 0;
    port_info->rx_bytes = 0;

    uart_config_t uart_cfg = {};
    uart_cfg.baud_rate = config->baud_rate;
    uart_cfg.data_bits = UART_DATA_8_BITS;
    uart_cfg.parity = UART_PARITY_DISABLE;
    uart_cfg.stop_bits = UART_STOP_BITS_1;
    uart_cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    uart_cfg.source_clk = UART_SCLK_DEFAULT;
#endif

    esp_err_t ret = uart_param_config(port_info->port_num, &uart_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART%d", port_info->id);
        return ret;
    }

    if (config->tx_pin != UART_PIN_NO_CHANGE && config->rx_pin != UART_PIN_NO_CHANGE) {
        ret = uart_set_pin(port_info->port_num, config->tx_pin, config->rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set UART%d pins", port_info->id);
            return ret;
        }
    }

    ret = uart_driver_install(port_info->port_num, 4096, 4096, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART%d driver", port_info->id);
        return ret;
    }

    ESP_LOGI(
        TAG,
        "UART%d (%s) init: baud=%d, tx=%d, rx=%d",
        port_info->id,
        port_info->name,
        port_info->baud_rate,
        port_info->tx_pin,
        port_info->rx_pin);
    return ESP_OK;
}

esp_err_t bridge_uart_start_rx_task(mavlink_uart_port_info_t *port_info) {
    char task_name[16];
    snprintf(task_name, sizeof(task_name), "Uart%dRx", port_info->id);

    const BaseType_t res = xTaskCreatePinnedToCore(
        uart_rx_task,
        task_name,
        4096,
        port_info,
        10,
        &port_info->rx_task,
        0);
    return res == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t bridge_uart_send(
    mavlink_bridge_handle_t handle,
    mavlink_uart_port_info_t *port_info,
    const uint8_t *data,
    size_t len) {
    if (!handle || !port_info || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!port_info->enabled) {
        return ESP_ERR_INVALID_STATE;
    }
    if (uart_wait_tx_done(port_info->port_num, pdMS_TO_TICKS(100)) != ESP_OK) {
        handle->stats.error_count++;
        return ESP_ERR_TIMEOUT;
    }

    const int written = uart_write_bytes(port_info->port_num, (const char *)data, len);
    if (written < 0) {
        handle->stats.error_count++;
        return ESP_FAIL;
    }

    port_info->tx_bytes += (uint32_t)written;
    handle->stats.uart_tx_bytes[port_info->id] += (uint32_t)written;
    return ESP_OK;
}

void bridge_uart_shutdown(mavlink_uart_port_info_t *port_info) {
    if (!port_info) {
        return;
    }
    if (port_info->rx_task) {
        vTaskDelete(port_info->rx_task);
        port_info->rx_task = NULL;
    }
    uart_driver_delete(port_info->port_num);
}
