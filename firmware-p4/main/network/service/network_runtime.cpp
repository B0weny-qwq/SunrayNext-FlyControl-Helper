#include "network/infra/network_internal.hpp"

#include <string.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "wifi_app.h"

static const char *TAG = "network_app";

network_runtime_state_t g_network_state = {
    .config = {
        .type = NETWORK_TYPE_NONE,
        .mode = NETWORK_MODE_NONE,
        .target_ip = {0},
        .target_port = 0,
        .local_port = 5000,
        .is_running = false,
        .recv_count = 0,
        .send_count = 0,
    },
    .udp_socket = nullptr,
    .tcp_socket = nullptr,
    .tcp_client = nullptr,
    .recv_callback = nullptr,
    .recv_ctx = nullptr,
    .config_mutex = nullptr,
    .server_task_handle = nullptr,
};

extern "C" {

esp_err_t network_app_init(void) {
    ESP_LOGI(TAG, "初始化网络通信模块");
    ESP_ERROR_CHECK(network_internal_init_mutex());
    network_internal_reset_config();
    return ESP_OK;
}

esp_err_t network_app_deinit(void) {
    ESP_LOGI(TAG, "反初始化网络通信模块");
    network_app_stop();
    if (g_network_state.config_mutex) {
        vSemaphoreDelete(g_network_state.config_mutex);
        g_network_state.config_mutex = nullptr;
    }
    return ESP_OK;
}

esp_err_t network_app_start(void) {
    if (g_network_state.config.is_running) {
        return ESP_OK;
    }
    if (g_network_state.config.type == NETWORK_TYPE_NONE) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!network_internal_is_wifi_connected()) {
        return ESP_FAIL;
    }
    return g_network_state.config.type == NETWORK_TYPE_UDP ? network_start_udp_session() : network_start_tcp_session();
}

esp_err_t network_app_stop(void) {
    if (!g_network_state.config.is_running) {
        return ESP_OK;
    }

    g_network_state.config.is_running = false;
    if (g_network_state.server_task_handle) {
        vTaskDelete(g_network_state.server_task_handle);
        g_network_state.server_task_handle = nullptr;
    }
    g_network_state.udp_socket.reset();
    g_network_state.tcp_socket.reset();
    g_network_state.tcp_client.reset();
    return ESP_OK;
}

esp_err_t network_app_send(const uint8_t *data, size_t len) {
    if (!g_network_state.config.is_running) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    bool success = false;
    if (g_network_state.config.type == NETWORK_TYPE_UDP) {
        if (g_network_state.config.target_ip[0] == '\0' || g_network_state.config.target_port == 0) {
            return ESP_ERR_INVALID_STATE;
        }
        success = network_send_udp_packet(data, len);
    } else {
        if (!g_network_state.tcp_client || !g_network_state.tcp_client->is_connected()) {
            return ESP_ERR_INVALID_STATE;
        }
        success = g_network_state.tcp_client->transmit(
            std::vector<uint8_t>(data, data + len),
            espp::TcpSocket::TransmitConfig::Default());
    }

    if (!success) {
        return ESP_FAIL;
    }
    g_network_state.config.send_count++;
    return ESP_OK;
}

esp_err_t network_app_send_string(const char *str) {
    return str ? network_app_send((const uint8_t *)str, strlen(str)) : ESP_ERR_INVALID_ARG;
}

esp_err_t network_app_get_local_ip(char *ip_str, size_t len) {
    if (!ip_str || len < 16) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_netif_ip_info_t ip_info;
    esp_err_t ret = wifi_app_get_ip_info(&ip_info);
    if (ret == ESP_OK) {
        snprintf(ip_str, len, IPSTR, IP2STR(&ip_info.ip));
    }
    return ret;
}

void network_app_print_status(void) {
    ESP_LOGI(
        TAG,
        "type=%s mode=%s local=%d target=%s:%d running=%s recv=%lu send=%lu",
        g_network_state.config.type == NETWORK_TYPE_UDP ? "UDP" :
        g_network_state.config.type == NETWORK_TYPE_TCP ? "TCP" : "NONE",
        g_network_state.config.mode == NETWORK_MODE_CLIENT ? "CLIENT" :
        g_network_state.config.mode == NETWORK_MODE_SERVER ? "SERVER" : "NONE",
        g_network_state.config.local_port,
        g_network_state.config.target_ip,
        g_network_state.config.target_port,
        g_network_state.config.is_running ? "yes" : "no",
        (unsigned long)g_network_state.config.recv_count,
        (unsigned long)g_network_state.config.send_count);
}

}  // extern "C"
