#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UART_ID_TELEM1 = 1,
    UART_ID_TELEM2 = 2,
    UART_ID_DEBUG = 3,
    UART_ID_MAX
} mavlink_uart_id_t;

typedef enum {
    MAVLINK_STATE_IDLE = 0,
    MAVLINK_STATE_INITIALIZED,
    MAVLINK_STATE_RUNNING,
    MAVLINK_STATE_ERROR
} mavlink_state_t;

typedef struct mavlink_bridge_t *mavlink_bridge_handle_t;

typedef void (*net_data_callback_t)(uint8_t uart_id, const uint8_t *data, size_t len, void *ctx);
typedef void (*uart_data_callback_t)(uint8_t uart_id, const uint8_t *data, size_t len, void *ctx);

typedef struct {
    mavlink_uart_id_t id;
    int baud_rate;
    uint8_t tx_pin;
    uint8_t rx_pin;
    bool enabled;
    const char *name;
    uart_data_callback_t data_callback;
    void *callback_ctx;
    bool mavlink_mode;
    uint8_t sysid_filter;
} mavlink_uart_config_t;

#ifdef __cplusplus
}
#endif
