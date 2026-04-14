#pragma once

#include <stdint.h>

#include "bridge/model/bridge_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t udp_rx_count;
    uint32_t tcp_rx_count;
    uint32_t udp_drop_count;
    uint32_t uart_tx_bytes[UART_ID_MAX];
    uint32_t uart_rx_bytes[UART_ID_MAX];
    uint32_t error_count;
} mavlink_stats_t;

#ifdef __cplusplus
}
#endif
