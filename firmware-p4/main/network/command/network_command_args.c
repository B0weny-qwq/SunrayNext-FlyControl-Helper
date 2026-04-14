#include "network/command/network_command_internal.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "network/service/network_runtime.h"

static const char *TAG = "net_cmd";

int cmd_net_type(int argc, char **argv) {
    if (argc < 2) {
        ESP_LOGE(TAG, "用法: net_type <udp|tcp>");
        return 1;
    }

    network_type_t type = NETWORK_TYPE_NONE;
    if (strcmp(argv[1], "udp") == 0 || strcmp(argv[1], "UDP") == 0) {
        type = NETWORK_TYPE_UDP;
    } else if (strcmp(argv[1], "tcp") == 0 || strcmp(argv[1], "TCP") == 0) {
        type = NETWORK_TYPE_TCP;
    } else {
        ESP_LOGE(TAG, "无效的类型: %s", argv[1]);
        return 1;
    }

    return network_app_set_type(type) == ESP_OK ? 0 : 1;
}

int cmd_net_mode(int argc, char **argv) {
    if (argc < 2) {
        ESP_LOGE(TAG, "用法: net_mode <client|server>");
        return 1;
    }
    if (strcmp(argv[1], "client") == 0 || strcmp(argv[1], "CLIENT") == 0) {
        return network_app_set_mode(NETWORK_MODE_CLIENT) == ESP_OK ? 0 : 1;
    }
    if (strcmp(argv[1], "server") == 0 || strcmp(argv[1], "SERVER") == 0) {
        return network_app_set_mode(NETWORK_MODE_SERVER) == ESP_OK ? 0 : 1;
    }

    ESP_LOGE(TAG, "无效的模式: %s", argv[1]);
    return 1;
}

int cmd_net_target(int argc, char **argv) {
    if (argc < 3) {
        ESP_LOGE(TAG, "用法: net_target <IP> <PORT>");
        return 1;
    }
    uint16_t port = (uint16_t)atoi(argv[2]);
    return network_app_set_target(argv[1], port) == ESP_OK ? 0 : 1;
}

int cmd_net_port(int argc, char **argv) {
    if (argc < 2) {
        ESP_LOGE(TAG, "用法: net_port <PORT>");
        return 1;
    }
    uint16_t port = (uint16_t)atoi(argv[1]);
    return network_app_set_local_port(port) == ESP_OK ? 0 : 1;
}
