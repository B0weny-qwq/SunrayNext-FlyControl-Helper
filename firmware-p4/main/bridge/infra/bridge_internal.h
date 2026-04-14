#pragma once

#include <list>
#include <memory>
#include <vector>

#include "bridge/service/bridge_runtime.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "tcp_socket.hpp"
#include "udp_socket.hpp"

typedef struct {
    uint32_t sequence;
    uint64_t timestamp;
    uint8_t data[MAVLINK_MAX_FRAME_LEN];
    size_t len;
    bool used;
} ordered_packet_t;

typedef struct {
    ordered_packet_t *packets;
    size_t size;
    uint32_t next_seq;
    SemaphoreHandle_t mutex;
} ordered_buffer_t;

typedef struct {
    std::unique_ptr<espp::TcpSocket> socket;
    char ip_str[16];
    uint16_t port;
    bool connected;
    TaskHandle_t task_handle;
} bridge_tcp_client_t;

typedef struct {
    mavlink_uart_id_t id;
    uart_port_t port_num;
    int baud_rate;
    uint8_t tx_pin;
    uint8_t rx_pin;
    bool enabled;
    const char *name;
    uart_data_callback_t callback;
    void *callback_ctx;
    bool mavlink_mode;
    uint8_t sysid_filter;
    TaskHandle_t rx_task;
    uint32_t tx_bytes;
    uint32_t rx_bytes;
} mavlink_uart_port_info_t;

struct mavlink_bridge_t {
    uint16_t udp_port;
    uint16_t tcp_port;
    uint8_t queue_size;
    bool ordered_mode;
    mavlink_state_t state;
    bool initialized;
    mavlink_uart_port_info_t uarts[MAVLINK_MAX_UARTS];
    uint8_t uart_count;
    ordered_buffer_t ordered_buf;
    std::unique_ptr<espp::UdpSocket> udp_socket;
    std::unique_ptr<espp::TcpSocket> tcp_server;
    std::list<bridge_tcp_client_t> tcp_clients;
    TaskHandle_t tcp_server_task;
    TaskHandle_t ordered_send_task;
    bool tcp_running;
    bool net_broadcast;
    net_data_callback_t net_callback;
    void *net_callback_ctx;
    SemaphoreHandle_t uart_mutex;
    SemaphoreHandle_t tcp_mutex;
    SemaphoreHandle_t net_mutex;
    mavlink_stats_t stats;
};

extern mavlink_bridge_handle_t g_bridge_handle;

void bridge_ordered_buffer_init(ordered_buffer_t *buf, size_t size);
void bridge_ordered_buffer_deinit(ordered_buffer_t *buf);
bool bridge_ordered_buffer_push(ordered_buffer_t *buf, const uint8_t *data, size_t len, uint32_t sequence);
bool bridge_ordered_buffer_pop(ordered_buffer_t *buf, uint8_t *data, size_t *len);

esp_err_t bridge_network_start(mavlink_bridge_handle_t handle);
void bridge_network_stop(mavlink_bridge_handle_t handle);
esp_err_t bridge_tcp_broadcast(mavlink_bridge_handle_t handle, const uint8_t *data, size_t len);

void bridge_runtime_handle_uart_rx(mavlink_uart_port_info_t *port_info, const uint8_t *data, size_t len);
