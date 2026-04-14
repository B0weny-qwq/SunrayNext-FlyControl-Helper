#include "bridge/infra/bridge_internal.h"

#include "esp_netif.h"
#include "wifi_app.h"
#include "transport/adapter/uart_port.h"

bool mavlink_bridge_is_initialized(mavlink_bridge_handle_t handle) {
    return handle && handle->initialized;
}

mavlink_state_t mavlink_bridge_get_state(mavlink_bridge_handle_t handle) {
    return handle ? handle->state : MAVLINK_STATE_IDLE;
}

esp_err_t mavlink_bridge_net_to_all_uarts(mavlink_bridge_handle_t handle, const uint8_t *data, size_t len) {
    if (!handle || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(handle->uart_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    for (int i = 0; i < handle->uart_count; i++) {
        if (handle->uarts[i].enabled) {
            bridge_uart_send(handle, &handle->uarts[i], data, len);
        }
    }

    xSemaphoreGive(handle->uart_mutex);
    return ESP_OK;
}

esp_err_t mavlink_bridge_net_to_uart(
    mavlink_bridge_handle_t handle,
    uint8_t uart_id,
    const uint8_t *data,
    size_t len) {
    if (!handle || !data || uart_id >= handle->uart_count) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(handle->uart_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = handle->uarts[uart_id].enabled
                        ? bridge_uart_send(handle, &handle->uarts[uart_id], data, len)
                        : ESP_ERR_INVALID_STATE;
    xSemaphoreGive(handle->uart_mutex);
    return ret;
}

esp_err_t mavlink_bridge_uart_to_net(
    mavlink_bridge_handle_t handle,
    uint8_t uart_id,
    const uint8_t *data,
    size_t len) {
    (void)uart_id;
    if (!handle || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    return bridge_tcp_broadcast(handle, data, len);
}

void mavlink_bridge_set_net_broadcast(mavlink_bridge_handle_t handle, bool enable) {
    if (handle) {
        handle->net_broadcast = enable;
    }
}

esp_err_t mavlink_bridge_send_to_uart(
    mavlink_bridge_handle_t handle,
    uint8_t uart_id,
    const uint8_t *data,
    size_t len) {
    return mavlink_bridge_net_to_uart(handle, uart_id, data, len);
}

esp_err_t mavlink_bridge_set_baudrate(mavlink_bridge_handle_t handle, uint8_t uart_id, int baud_rate) {
    if (!handle || uart_id >= handle->uart_count) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(handle->uart_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = uart_set_baudrate(handle->uarts[uart_id].port_num, baud_rate);
    if (ret == ESP_OK) {
        handle->uarts[uart_id].baud_rate = baud_rate;
    }
    xSemaphoreGive(handle->uart_mutex);
    return ret;
}

esp_err_t mavlink_bridge_enable_uart(mavlink_bridge_handle_t handle, uint8_t uart_id, bool enable) {
    if (!handle || uart_id >= handle->uart_count) {
        return ESP_ERR_INVALID_ARG;
    }
    handle->uarts[uart_id].enabled = enable;
    return ESP_OK;
}

void mavlink_bridge_on_net_data(mavlink_bridge_handle_t handle, net_data_callback_t callback, void *ctx) {
    if (handle) {
        handle->net_callback = callback;
        handle->net_callback_ctx = ctx;
    }
}

void mavlink_bridge_on_uart_data(
    mavlink_bridge_handle_t handle,
    uint8_t uart_id,
    uart_data_callback_t callback,
    void *ctx) {
    if (handle && uart_id < handle->uart_count) {
        handle->uarts[uart_id].callback = callback;
        handle->uarts[uart_id].callback_ctx = ctx;
    }
}

esp_err_t mavlink_bridge_get_local_ip(mavlink_bridge_handle_t handle, char *ip_str, size_t len) {
    if (!handle || !ip_str) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_netif_ip_info_t ip_info;
    esp_err_t ret = wifi_app_get_ip_info(&ip_info);
    if (ret == ESP_OK) {
        esp_ip4addr_ntoa(&ip_info.ip, ip_str, len);
    }
    return ret;
}

mavlink_bridge_handle_t mavlink_bridge_get_handle(void) {
    return g_bridge_handle;
}

void bridge_runtime_handle_uart_rx(mavlink_uart_port_info_t *port_info, const uint8_t *data, size_t len) {
    if (!port_info || !data || !g_bridge_handle) {
        return;
    }

    port_info->rx_bytes += (uint32_t)len;
    g_bridge_handle->stats.uart_rx_bytes[port_info->id] += (uint32_t)len;

    if (port_info->callback) {
        port_info->callback(port_info->id, data, len, port_info->callback_ctx);
    }
    if (g_bridge_handle->net_broadcast) {
        mavlink_bridge_uart_to_net(g_bridge_handle, port_info->id, data, len);
    }
}
