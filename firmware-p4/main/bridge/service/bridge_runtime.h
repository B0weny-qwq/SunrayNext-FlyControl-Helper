#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "bridge/model/bridge_config.h"
#include "bridge/model/bridge_stats.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t mavlink_bridge_init(const mavlink_bridge_config_t *config, mavlink_bridge_handle_t *handle);
esp_err_t mavlink_bridge_deinit(mavlink_bridge_handle_t handle);
bool mavlink_bridge_is_initialized(mavlink_bridge_handle_t handle);
mavlink_state_t mavlink_bridge_get_state(mavlink_bridge_handle_t handle);

esp_err_t mavlink_bridge_net_to_all_uarts(mavlink_bridge_handle_t handle, const uint8_t *data, size_t len);
esp_err_t mavlink_bridge_net_to_uart(
    mavlink_bridge_handle_t handle,
    uint8_t uart_id,
    const uint8_t *data,
    size_t len);
esp_err_t mavlink_bridge_uart_to_net(
    mavlink_bridge_handle_t handle,
    uint8_t uart_id,
    const uint8_t *data,
    size_t len);
void mavlink_bridge_set_net_broadcast(mavlink_bridge_handle_t handle, bool enable);

esp_err_t mavlink_bridge_send_to_uart(
    mavlink_bridge_handle_t handle,
    uint8_t uart_id,
    const uint8_t *data,
    size_t len);
esp_err_t mavlink_bridge_set_baudrate(mavlink_bridge_handle_t handle, uint8_t uart_id, int baud_rate);
int mavlink_bridge_get_baudrate(mavlink_bridge_handle_t handle, uint8_t uart_id);
esp_err_t mavlink_bridge_enable_uart(mavlink_bridge_handle_t handle, uint8_t uart_id, bool enable);

void mavlink_bridge_on_net_data(mavlink_bridge_handle_t handle, net_data_callback_t callback, void *ctx);
void mavlink_bridge_on_uart_data(
    mavlink_bridge_handle_t handle,
    uint8_t uart_id,
    uart_data_callback_t callback,
    void *ctx);

void mavlink_bridge_get_stats(mavlink_bridge_handle_t handle, mavlink_stats_t *stats);
void mavlink_bridge_reset_stats(mavlink_bridge_handle_t handle);
esp_err_t mavlink_bridge_get_local_ip(mavlink_bridge_handle_t handle, char *ip_str, size_t len);

const char *mavlink_get_uart_name(uint8_t uart_id);
uint8_t mavlink_bridge_get_count(mavlink_bridge_handle_t handle);
const char *mavlink_bridge_get_uart_name(mavlink_bridge_handle_t handle, uint8_t uart_id);
mavlink_bridge_handle_t mavlink_bridge_get_handle(void);

#ifdef __cplusplus
}
#endif
