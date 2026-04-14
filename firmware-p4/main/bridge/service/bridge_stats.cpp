#include "bridge/infra/bridge_internal.h"

#include <string.h>

const char *mavlink_get_uart_name(uint8_t uart_id) {
    if (uart_id == UART_ID_TELEM1) {
        return "TELEM1";
    }
    if (uart_id == UART_ID_TELEM2) {
        return "TELEM2";
    }
    if (uart_id == UART_ID_DEBUG) {
        return "DEBUG";
    }
    return "UNKNOWN";
}

uint8_t mavlink_bridge_get_count(mavlink_bridge_handle_t handle) {
    return handle ? handle->uart_count : 0;
}

const char *mavlink_bridge_get_uart_name(mavlink_bridge_handle_t handle, uint8_t uart_id) {
    if (!handle || uart_id >= handle->uart_count) {
        return "INVALID";
    }
    return handle->uarts[uart_id].name ? handle->uarts[uart_id].name : mavlink_get_uart_name(uart_id);
}

int mavlink_bridge_get_baudrate(mavlink_bridge_handle_t handle, uint8_t uart_id) {
    if (!handle || uart_id >= handle->uart_count) {
        return 0;
    }
    return handle->uarts[uart_id].baud_rate;
}

void mavlink_bridge_get_stats(mavlink_bridge_handle_t handle, mavlink_stats_t *stats) {
    if (handle && stats) {
        memcpy(stats, &handle->stats, sizeof(*stats));
    }
}

void mavlink_bridge_reset_stats(mavlink_bridge_handle_t handle) {
    if (!handle) {
        return;
    }

    memset(&handle->stats, 0, sizeof(handle->stats));
    for (int i = 0; i < handle->uart_count; i++) {
        handle->uarts[i].tx_bytes = 0;
        handle->uarts[i].rx_bytes = 0;
    }
}
