#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bridge/model/bridge_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAVLINK_TELEM1_ENABLED
#define MAVLINK_TELEM1_ENABLED 1
#endif
#ifndef MAVLINK_TELEM1_BAUD
#define MAVLINK_TELEM1_BAUD 115200
#endif
#ifndef MAVLINK_TELEM1_TX_PIN
#define MAVLINK_TELEM1_TX_PIN 20
#endif
#ifndef MAVLINK_TELEM1_RX_PIN
#define MAVLINK_TELEM1_RX_PIN 21
#endif

#ifndef MAVLINK_TELEM2_ENABLED
#define MAVLINK_TELEM2_ENABLED 1
#endif
#ifndef MAVLINK_TELEM2_BAUD
#define MAVLINK_TELEM2_BAUD 115200
#endif
#ifndef MAVLINK_TELEM2_TX_PIN
#define MAVLINK_TELEM2_TX_PIN 10
#endif
#ifndef MAVLINK_TELEM2_RX_PIN
#define MAVLINK_TELEM2_RX_PIN 11
#endif

#ifndef MAVLINK_DEBUG_ENABLED
#define MAVLINK_DEBUG_ENABLED 1
#endif
#ifndef MAVLINK_DEBUG_BAUD
#define MAVLINK_DEBUG_BAUD 115200
#endif
#ifndef MAVLINK_DEBUG_TX_PIN
#define MAVLINK_DEBUG_TX_PIN 22
#endif
#ifndef MAVLINK_DEBUG_RX_PIN
#define MAVLINK_DEBUG_RX_PIN 23
#endif

#ifndef MAVLINK_UDP_PORT
#define MAVLINK_UDP_PORT 8888
#endif

#ifndef MAVLINK_TCP_PORT
#define MAVLINK_TCP_PORT 8889
#endif

#ifndef MAVLINK_MAX_TCP_CLIENTS
#define MAVLINK_MAX_TCP_CLIENTS 5
#endif

#ifndef MAVLINK_QUEUE_SIZE
#define MAVLINK_QUEUE_SIZE 32
#endif

#ifndef MAVLINK_SEQ_WINDOW
#define MAVLINK_SEQ_WINDOW 10
#endif

#define MAVLINK_STX_V1 0xFE
#define MAVLINK_HEADER_LEN 6
#define MAVLINK_FOOTER_LEN 2
#define MAVLINK_MIN_FRAME_LEN (MAVLINK_HEADER_LEN + MAVLINK_FOOTER_LEN)
#define MAVLINK_MAX_PAYLOAD_LEN 255
#define MAVLINK_MAX_FRAME_LEN (MAVLINK_MAX_PAYLOAD_LEN + MAVLINK_MIN_FRAME_LEN)

#ifndef MAVLINK_DEBUG_LOG
#define MAVLINK_DEBUG_LOG 1
#endif

#ifndef MAVLINK_LOG_HEXDUMP
#define MAVLINK_LOG_HEXDUMP 0
#endif

#define MAVLINK_MAX_UARTS 3

typedef struct {
    uint16_t udp_port;
    uint16_t tcp_port;
    uint8_t queue_size;
    bool ordered_mode;
} mavlink_bridge_config_t;

#define MAVLINK_UART_CONFIG_DEFAULT(_id, _name) \
    { \
        .id = _id, \
        .baud_rate = 115200, \
        .tx_pin = UART_PIN_NO_CHANGE, \
        .rx_pin = UART_PIN_NO_CHANGE, \
        .enabled = true, \
        .name = _name, \
        .data_callback = NULL, \
        .callback_ctx = NULL, \
        .mavlink_mode = true, \
        .sysid_filter = 0, \
    }

#define MAVLINK_BRIDGE_CONFIG_DEFAULT() \
    { \
        .udp_port = MAVLINK_UDP_PORT, \
        .tcp_port = MAVLINK_TCP_PORT, \
        .queue_size = MAVLINK_QUEUE_SIZE, \
        .ordered_mode = false, \
    }

#ifdef __cplusplus
}
#endif
