#include "bridge/infra/bridge_internal.h"

#include <new>
#include <stdlib.h>

#include "esp_log.h"
#include "transport/adapter/uart_port.h"

static const char *TAG = "bridge_runtime";

mavlink_bridge_handle_t g_bridge_handle = NULL;

static void bridge_cleanup(mavlink_bridge_handle_t handle) {
    if (!handle) {
        return;
    }

    bridge_network_stop(handle);
    bridge_ordered_buffer_deinit(&handle->ordered_buf);
    for (int i = 0; i < handle->uart_count; i++) {
        bridge_uart_shutdown(&handle->uarts[i]);
    }

    if (handle->tcp_mutex) {
        vSemaphoreDelete(handle->tcp_mutex);
    }
    if (handle->uart_mutex) {
        vSemaphoreDelete(handle->uart_mutex);
    }
    if (handle->net_mutex) {
        vSemaphoreDelete(handle->net_mutex);
    }
    if (g_bridge_handle == handle) {
        g_bridge_handle = NULL;
    }

    delete handle;
}

static int build_default_uart_configs(mavlink_uart_config_t *uart_configs) {
    int count = 0;
#if MAVLINK_TELEM1_ENABLED
    uart_configs[count++] = (mavlink_uart_config_t){
        .id = UART_ID_TELEM1,
        .baud_rate = MAVLINK_TELEM1_BAUD,
        .tx_pin = MAVLINK_TELEM1_TX_PIN,
        .rx_pin = MAVLINK_TELEM1_RX_PIN,
        .enabled = false,
        .name = "TELEM1",
        .data_callback = NULL,
        .callback_ctx = NULL,
        .mavlink_mode = true,
        .sysid_filter = 0,
    };
#endif
#if MAVLINK_TELEM2_ENABLED
    uart_configs[count++] = (mavlink_uart_config_t){
        .id = UART_ID_TELEM2,
        .baud_rate = MAVLINK_TELEM2_BAUD,
        .tx_pin = MAVLINK_TELEM2_TX_PIN,
        .rx_pin = MAVLINK_TELEM2_RX_PIN,
        .enabled = false,
        .name = "TELEM2",
        .data_callback = NULL,
        .callback_ctx = NULL,
        .mavlink_mode = true,
        .sysid_filter = 0,
    };
#endif
#if MAVLINK_DEBUG_ENABLED
    uart_configs[count++] = (mavlink_uart_config_t){
        .id = UART_ID_DEBUG,
        .baud_rate = MAVLINK_DEBUG_BAUD,
        .tx_pin = MAVLINK_DEBUG_TX_PIN,
        .rx_pin = MAVLINK_DEBUG_RX_PIN,
        .enabled = false,
        .name = "DEBUG",
        .data_callback = NULL,
        .callback_ctx = NULL,
        .mavlink_mode = true,
        .sysid_filter = 0,
    };
#endif
    return count;
}

esp_err_t mavlink_bridge_init(const mavlink_bridge_config_t *config, mavlink_bridge_handle_t *handle) {
    if (!config || !handle) {
        return ESP_ERR_INVALID_ARG;
    }

    auto *instance = new (std::nothrow) mavlink_bridge_t();
    if (!instance) {
        return ESP_ERR_NO_MEM;
    }

    instance->udp_port = config->udp_port ? config->udp_port : MAVLINK_UDP_PORT;
    instance->tcp_port = config->tcp_port;
    instance->queue_size = config->queue_size ? config->queue_size : MAVLINK_QUEUE_SIZE;
    instance->ordered_mode = config->ordered_mode;
    instance->state = MAVLINK_STATE_IDLE;
    instance->net_broadcast = true;
    instance->uart_mutex = xSemaphoreCreateMutex();
    instance->tcp_mutex = xSemaphoreCreateMutex();
    instance->net_mutex = xSemaphoreCreateMutex();

    if (!instance->uart_mutex || !instance->tcp_mutex || !instance->net_mutex) {
        bridge_cleanup(instance);
        return ESP_ERR_NO_MEM;
    }

    bridge_ordered_buffer_init(&instance->ordered_buf, instance->queue_size);

    mavlink_uart_config_t uart_configs[MAVLINK_MAX_UARTS];
    const int config_count = build_default_uart_configs(uart_configs);
    for (int i = 0; i < config_count; i++) {
        esp_err_t err = bridge_uart_init(&uart_configs[i], &instance->uarts[instance->uart_count]);
        if (err == ESP_OK) {
            instance->uart_count++;
        } else {
            ESP_LOGW(TAG, "UART%d init failed, skipping", i);
        }
    }

    instance->state = MAVLINK_STATE_RUNNING;
    g_bridge_handle = instance;
    esp_err_t err = bridge_network_start(instance);
    if (err != ESP_OK) {
        bridge_cleanup(instance);
        return err;
    }

    for (int i = 0; i < instance->uart_count; i++) {
        err = bridge_uart_start_rx_task(&instance->uarts[i]);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to start UART%d Rx task", instance->uarts[i].id);
        }
    }

    instance->initialized = true;
    *handle = instance;
    return ESP_OK;
}

esp_err_t mavlink_bridge_deinit(mavlink_bridge_handle_t handle) {
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    handle->state = MAVLINK_STATE_IDLE;
    handle->initialized = false;
    bridge_cleanup(handle);
    return ESP_OK;
}
