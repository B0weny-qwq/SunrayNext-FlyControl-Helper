#include "network/infra/network_internal.hpp"

#include <string.h>

#include "esp_log.h"
#include "wifi_app.h"

static const char *TAG = "network_cfg";

bool network_internal_is_wifi_connected(void) {
    return wifi_app_is_connected();
}

esp_err_t network_internal_init_mutex(void) {
    if (g_network_state.config_mutex == nullptr) {
        g_network_state.config_mutex = xSemaphoreCreateMutex();
        if (g_network_state.config_mutex == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

void network_internal_reset_config(void) {
    memset(&g_network_state.config, 0, sizeof(g_network_state.config));
    g_network_state.config.local_port = 5000;
}

extern "C" {

esp_err_t network_app_set_type(network_type_t type) {
    if (g_network_state.config.is_running) {
        ESP_LOGE(TAG, "通信正在进行中，请先停止");
        return ESP_ERR_INVALID_STATE;
    }
    if (type != NETWORK_TYPE_UDP && type != NETWORK_TYPE_TCP) {
        ESP_LOGE(TAG, "无效的通信类型: %d", type);
        return ESP_ERR_INVALID_ARG;
    }

    g_network_state.config.type = type;
    return ESP_OK;
}

network_type_t network_app_get_type(void) {
    return g_network_state.config.type;
}

esp_err_t network_app_set_mode(network_mode_t mode) {
    g_network_state.config.mode = mode;
    return ESP_OK;
}

esp_err_t network_app_set_target(const char *ip, uint16_t port) {
    if (!ip || strlen(ip) >= sizeof(g_network_state.config.target_ip) || port == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (g_network_state.config_mutex) {
        xSemaphoreTake(g_network_state.config_mutex, portMAX_DELAY);
    }
    strncpy(g_network_state.config.target_ip, ip, sizeof(g_network_state.config.target_ip) - 1);
    g_network_state.config.target_port = port;
    if (g_network_state.config_mutex) {
        xSemaphoreGive(g_network_state.config_mutex);
    }
    return ESP_OK;
}

esp_err_t network_app_set_local_port(uint16_t port) {
    g_network_state.config.local_port = port;
    return ESP_OK;
}

const network_config_t *network_app_get_config(void) {
    return &g_network_state.config;
}

bool network_app_is_running(void) {
    return g_network_state.config.is_running;
}

void network_app_set_recv_callback(network_recv_callback_t callback, void *ctx) {
    g_network_state.recv_callback = callback;
    g_network_state.recv_ctx = ctx;
}

}  // extern "C"
