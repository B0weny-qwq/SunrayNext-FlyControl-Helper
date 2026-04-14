#include "network/infra/network_internal.hpp"

#include "esp_log.h"

static const char *TAG = "network_tcp";

static void notify_tcp_data(const std::vector<uint8_t> &data) {
    if (!g_network_state.tcp_client) {
        return;
    }
    auto &info = g_network_state.tcp_client->get_remote_info();
    network_internal_note_receive("TCP", data, info);
    if (g_network_state.recv_callback) {
        g_network_state.recv_callback(
            data.data(),
            data.size(),
            info.address.c_str(),
            (uint16_t)info.port,
            g_network_state.recv_ctx);
    }
}

void network_tcp_server_loop(void *arg) {
    (void)arg;
    while (g_network_state.config.is_running) {
        if (!g_network_state.tcp_socket) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!g_network_state.tcp_client) {
            g_network_state.tcp_client = g_network_state.tcp_socket->accept();
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (!g_network_state.tcp_client->is_connected()) {
            g_network_state.tcp_client.reset();
            continue;
        }

        std::vector<uint8_t> data;
        if (g_network_state.tcp_client->receive(data, 1024) && !data.empty()) {
            notify_tcp_data(data);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelete(NULL);
}

void network_tcp_client_loop(void *arg) {
    (void)arg;
    while (g_network_state.config.is_running) {
        if (!g_network_state.tcp_client || !g_network_state.tcp_client->is_connected()) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        std::vector<uint8_t> data;
        if (g_network_state.tcp_client->receive(data, 1024) && !data.empty()) {
            notify_tcp_data(data);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelete(NULL);
}

esp_err_t network_start_tcp_session(void) {
    if (g_network_state.config.mode == NETWORK_MODE_SERVER) {
        g_network_state.tcp_socket = std::make_unique<espp::TcpSocket>(
            espp::TcpSocket::Config{.log_level = espp::Logger::Verbosity::INFO});
        if (!g_network_state.tcp_socket->bind(g_network_state.config.local_port)) {
            g_network_state.tcp_socket.reset();
            return ESP_FAIL;
        }
        if (!g_network_state.tcp_socket->listen(1)) {
            g_network_state.tcp_socket.reset();
            return ESP_FAIL;
        }

        g_network_state.config.is_running = true;
        xTaskCreate(network_tcp_server_loop, "TcpServer", 8 * 1024, NULL, 5, &g_network_state.server_task_handle);
        return ESP_OK;
    }

    if (g_network_state.config.target_ip[0] == '\0' || g_network_state.config.target_port == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    g_network_state.tcp_client = std::make_unique<espp::TcpSocket>(
        espp::TcpSocket::Config{.log_level = espp::Logger::Verbosity::INFO});
    if (!g_network_state.tcp_client->connect(
            {.ip_address = g_network_state.config.target_ip, .port = g_network_state.config.target_port})) {
        g_network_state.tcp_client.reset();
        return ESP_FAIL;
    }

    g_network_state.config.is_running = true;
    xTaskCreate(network_tcp_client_loop, "TcpClient", 8 * 1024, NULL, 5, &g_network_state.server_task_handle);
    ESP_LOGI(TAG, "TCP 已连接到 %s:%d", g_network_state.config.target_ip, g_network_state.config.target_port);
    return ESP_OK;
}
