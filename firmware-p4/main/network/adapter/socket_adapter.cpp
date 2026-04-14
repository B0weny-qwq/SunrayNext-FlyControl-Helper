#include "network/infra/network_internal.hpp"

#include <string>

#include "esp_log.h"

static const char *TAG = "network_socket";

void network_internal_note_receive(const char *proto, const std::vector<uint8_t> &data, const espp::Socket::Info &info) {
    g_network_state.config.recv_count++;
    ESP_LOGI(TAG, "%s 收到 %d 字节 from %s:%zu", proto, data.size(), info.address.c_str(), info.port);

    if (data.empty()) {
        return;
    }

    std::string data_str(data.begin(), data.end());
    bool printable = true;
    for (uint8_t c : data) {
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
            printable = false;
            break;
        }
    }

    if (printable) {
        ESP_LOGI(TAG, "  内容: %s", data_str.c_str());
        return;
    }

    char hex_buf[64];
    size_t hex_len = 0;
    for (size_t i = 0; i < data.size() && i < 32; i++) {
        hex_len += snprintf(hex_buf + hex_len, sizeof(hex_buf) - hex_len, "%02X ", data[i]);
    }
    ESP_LOGI(TAG, "  Hex: %s", hex_buf);
}

bool network_send_udp_packet(const uint8_t *data, size_t len) {
    auto send_socket = std::make_unique<espp::UdpSocket>(espp::UdpSocket::Config{});
    return send_socket->send(
        std::vector<uint8_t>(data, data + len),
        espp::UdpSocket::SendConfig{
            .ip_address = g_network_state.config.target_ip,
            .port = g_network_state.config.target_port,
        });
}
