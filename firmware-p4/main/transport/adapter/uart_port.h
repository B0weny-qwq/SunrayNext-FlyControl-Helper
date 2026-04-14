#pragma once

#include "bridge/infra/bridge_internal.h"

esp_err_t bridge_uart_init(const mavlink_uart_config_t *config, mavlink_uart_port_info_t *port_info);
esp_err_t bridge_uart_start_rx_task(mavlink_uart_port_info_t *port_info);
esp_err_t bridge_uart_send(
    mavlink_bridge_handle_t handle,
    mavlink_uart_port_info_t *port_info,
    const uint8_t *data,
    size_t len);
void bridge_uart_shutdown(mavlink_uart_port_info_t *port_info);
