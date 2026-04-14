#include "network/infra/network_internal.hpp"

#include "esp_log.h"

static const char *TAG = "network_udp";

std::optional<std::vector<uint8_t>> network_udp_receive_callback(
    std::vector<uint8_t> &data,
    const espp::Socket::Info &sender_info) {
    network_internal_note_receive("UDP", data, sender_info);
    if (g_network_state.recv_callback) {
        g_network_state.recv_callback(
            data.data(),
            data.size(),
            sender_info.address.c_str(),
            (uint16_t)sender_info.port,
            g_network_state.recv_ctx);
    }
    return std::nullopt;
}

esp_err_t network_start_udp_session(void) {
    if (g_network_state.config.mode == NETWORK_MODE_SERVER) {
        g_network_state.udp_socket = std::make_unique<espp::UdpSocket>(
            espp::UdpSocket::Config{.log_level = espp::Logger::Verbosity::INFO});

        auto task_config = espp::Task::BaseConfig{
            .name = "UdpServer",
            .stack_size_bytes = 8 * 1024,
        };
        auto recv_config = espp::UdpSocket::ReceiveConfig{
            .port = g_network_state.config.local_port,
            .buffer_size = 1024,
            .on_receive_callback = network_udp_receive_callback,
        };

        if (!g_network_state.udp_socket->start_receiving(task_config, recv_config)) {
            ESP_LOGE(TAG, "UDP 服务器启动失败");
            g_network_state.udp_socket.reset();
            return ESP_FAIL;
        }
    } else {
        if (g_network_state.config.target_ip[0] == '\0' || g_network_state.config.target_port == 0) {
            return ESP_ERR_INVALID_STATE;
        }
    }

    g_network_state.config.is_running = true;
    return ESP_OK;
}
