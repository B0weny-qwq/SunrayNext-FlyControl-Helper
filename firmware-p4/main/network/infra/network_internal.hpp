#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "network/service/network_runtime.h"
#include "tcp_socket.hpp"
#include "udp_socket.hpp"

struct network_runtime_state_t {
    network_config_t config;
    std::unique_ptr<espp::UdpSocket> udp_socket;
    std::unique_ptr<espp::TcpSocket> tcp_socket;
    std::unique_ptr<espp::TcpSocket> tcp_client;
    network_recv_callback_t recv_callback;
    void *recv_ctx;
    SemaphoreHandle_t config_mutex;
    TaskHandle_t server_task_handle;
};

extern network_runtime_state_t g_network_state;

bool network_internal_is_wifi_connected(void);
esp_err_t network_internal_init_mutex(void);
void network_internal_reset_config(void);
void network_internal_note_receive(const char *proto, const std::vector<uint8_t> &data, const espp::Socket::Info &info);
std::optional<std::vector<uint8_t>> network_udp_receive_callback(
    std::vector<uint8_t> &data,
    const espp::Socket::Info &sender_info);
void network_tcp_server_loop(void *arg);
void network_tcp_client_loop(void *arg);
esp_err_t network_start_udp_session(void);
esp_err_t network_start_tcp_session(void);
bool network_send_udp_packet(const uint8_t *data, size_t len);
