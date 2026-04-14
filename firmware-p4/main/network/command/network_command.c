#include "network/command/network_command.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_console.h"
#include "esp_log.h"
#include "network/command/network_command_internal.h"
#include "network/service/network_runtime.h"

static const char *TAG = "net_cmd";

int cmd_net_start(int argc, char **argv) {
    (void)argc;
    (void)argv;
    if (network_app_is_running()) {
        printf("通信已在运行中，请先停止 (net_stop)\n");
        return 0;
    }
    esp_err_t ret = network_app_start();
    if (ret == ESP_OK) {
        network_app_print_status();
    }
    return ret == ESP_OK ? 0 : 1;
}

int cmd_net_stop(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return network_app_stop() == ESP_OK ? 0 : 1;
}

int cmd_net_send(int argc, char **argv) {
    if (argc < 2 || !network_app_is_running()) {
        return 1;
    }

    char message[256] = {0};
    size_t offset = 0;
    for (int i = 1; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (offset + len + 1 >= sizeof(message)) {
            break;
        }
        if (i > 1) {
            message[offset++] = ' ';
        }
        memcpy(message + offset, argv[i], len);
        offset += len;
    }
    return network_app_send_string(message) == ESP_OK ? 0 : 1;
}

int cmd_net_status(int argc, char **argv) {
    (void)argc;
    (void)argv;
    network_app_print_status();
    return 0;
}

int cmd_net_localip(int argc, char **argv) {
    (void)argc;
    (void)argv;
    char ip_str[16];
    if (network_app_get_local_ip(ip_str, sizeof(ip_str)) != ESP_OK) {
        return 1;
    }
    printf("本地 IP: %s\n", ip_str);
    return 0;
}

const esp_console_cmd_t *network_command_table(size_t *count) {
    static const esp_console_cmd_t s_commands[] = {
        {.command = "net_type", .help = "设置通信类型 (udp/tcp)", .hint = "<udp|tcp>", .func = &cmd_net_type},
        {.command = "net_mode", .help = "设置通信模式 (client/server)", .hint = "<client|server>", .func = &cmd_net_mode},
        {.command = "net_target", .help = "设置目标地址 (客户端模式)", .hint = "<IP> <PORT>", .func = &cmd_net_target},
        {.command = "net_port", .help = "设置本地端口 (服务器模式)", .hint = "<PORT>", .func = &cmd_net_port},
        {.command = "net_start", .help = "启动网络通信", .hint = NULL, .func = &cmd_net_start},
        {.command = "net_stop", .help = "停止网络通信", .hint = NULL, .func = &cmd_net_stop},
        {.command = "net_send", .help = "发送消息", .hint = "<MESSAGE>", .func = &cmd_net_send},
        {.command = "net_status", .help = "显示网络状态", .hint = NULL, .func = &cmd_net_status},
        {.command = "net_localip", .help = "显示本地 IP 地址", .hint = NULL, .func = &cmd_net_localip},
        {.command = "net_help", .help = "显示网络命令帮助", .hint = NULL, .func = &cmd_net_help},
    };
    *count = sizeof(s_commands) / sizeof(s_commands[0]);
    return s_commands;
}

esp_err_t network_cmd_register(void) {
    size_t count = 0;
    const esp_console_cmd_t *commands = network_command_table(&count);
    esp_err_t ret = ESP_OK;
    for (size_t i = 0; i < count; i++) {
        esp_err_t err = esp_console_cmd_register(&commands[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "注册命令 '%s' 失败: %s", commands[i].command, esp_err_to_name(err));
            ret = err;
        }
    }
    return ret;
}
