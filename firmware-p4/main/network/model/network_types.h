#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NETWORK_TYPE_NONE = 0,
    NETWORK_TYPE_UDP,
    NETWORK_TYPE_TCP
} network_type_t;

typedef enum {
    NETWORK_MODE_NONE = 0,
    NETWORK_MODE_CLIENT,
    NETWORK_MODE_SERVER
} network_mode_t;

typedef struct {
    network_type_t type;
    network_mode_t mode;
    char target_ip[16];
    uint16_t target_port;
    uint16_t local_port;
    bool is_running;
    uint32_t recv_count;
    uint32_t send_count;
} network_config_t;

typedef void (*network_recv_callback_t)(
    const uint8_t *data,
    size_t len,
    const char *ip,
    uint16_t port,
    void *ctx);

#ifdef __cplusplus
}
#endif
