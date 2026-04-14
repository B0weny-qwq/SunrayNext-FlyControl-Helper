/**
 * @file network_cmd.c
 * @brief P4 WiFi Remote 网络通信 Console 命令
 *
 * @details
 * 提供以下命令：
 * - net_type: 设置通信类型 (udp/tcp)
 * - net_mode: 设置通信模式 (client/server)
 * - net_target: 设置目标地址
 * - net_port: 设置本地端口
 * - net_start: 启动通信
 * - net_stop: 停止通信
 * - net_send: 发送数据
 * - net_status: 显示状态
 *
 * @author P4 Team
 * @date 2026-04-04
 */

#include "network_app.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "net_cmd";

/**
 * @brief net_type 命令 - 设置通信类型
 */
static int cmd_net_type(int argc, char **argv)
{
    if (argc < 2) {
        ESP_LOGE(TAG, "用法: net_type <udp|tcp>");
        return 1;
    }

    if (strcmp(argv[1], "udp") == 0 || strcmp(argv[1], "UDP") == 0) {
        esp_err_t ret = network_app_set_type(NETWORK_TYPE_UDP);
        if (ret == ESP_OK) {
            printf("通信类型已设置为 UDP\n");
        }
        return ret == ESP_OK ? 0 : 1;
    }
    else if (strcmp(argv[1], "tcp") == 0 || strcmp(argv[1], "TCP") == 0) {
        esp_err_t ret = network_app_set_type(NETWORK_TYPE_TCP);
        if (ret == ESP_OK) {
            printf("通信类型已设置为 TCP\n");
        }
        return ret == ESP_OK ? 0 : 1;
    }
    else {
        ESP_LOGE(TAG, "无效的类型: %s, 请使用 udp 或 tcp", argv[1]);
        return 1;
    }
}

/**
 * @brief net_mode 命令 - 设置通信模式
 */
static int cmd_net_mode(int argc, char **argv)
{
    if (argc < 2) {
        ESP_LOGE(TAG, "用法: net_mode <client|server>");
        return 1;
    }

    if (strcmp(argv[1], "client") == 0 || strcmp(argv[1], "CLIENT") == 0) {
        network_app_set_mode(NETWORK_MODE_CLIENT);
        printf("通信模式已设置为客户端 (主动连接)\n");
    }
    else if (strcmp(argv[1], "server") == 0 || strcmp(argv[1], "SERVER") == 0) {
        network_app_set_mode(NETWORK_MODE_SERVER);
        printf("通信模式已设置为服务器 (监听等待)\n");
    }
    else {
        ESP_LOGE(TAG, "无效的模式: %s, 请使用 client 或 server", argv[1]);
        return 1;
    }

    return 0;
}

/**
 * @brief net_target 命令 - 设置目标地址
 */
static int cmd_net_target(int argc, char **argv)
{
    if (argc < 3) {
        ESP_LOGE(TAG, "用法: net_target <IP> <PORT>");
        ESP_LOGE(TAG, "示例: net_target 192.168.1.100 5000");
        return 1;
    }

    uint16_t port = atoi(argv[2]);
    if (port == 0) {
        ESP_LOGE(TAG, "无效的端口号: %s", argv[2]);
        return 1;
    }

    esp_err_t ret = network_app_set_target(argv[1], port);
    if (ret == ESP_OK) {
        printf("目标地址已设置为: %s:%d\n", argv[1], port);
    }
    return ret == ESP_OK ? 0 : 1;
}

/**
 * @brief net_port 命令 - 设置本地端口
 */
static int cmd_net_port(int argc, char **argv)
{
    if (argc < 2) {
        ESP_LOGE(TAG, "用法: net_port <PORT>");
        ESP_LOGE(TAG, "示例: net_port 5000");
        return 1;
    }

    uint16_t port = atoi(argv[1]);
    if (port == 0) {
        ESP_LOGE(TAG, "无效的端口号: %s", argv[1]);
        return 1;
    }

    network_app_set_local_port(port);
    printf("本地端口已设置为: %d\n", port);

    return 0;
}

/**
 * @brief net_start 命令 - 启动通信
 */
static int cmd_net_start(int argc, char **argv)
{
    if (network_app_is_running()) {
        printf("通信已在运行中，请先停止 (net_stop)\n");
        return 0;
    }

    printf("正在启动网络通信...\n");
    esp_err_t ret = network_app_start();

    if (ret == ESP_OK) {
        printf("网络通信已启动\n");
        network_app_print_status();
    } else if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "请先设置通信类型 (net_type)");
    } else if (ret == ESP_FAIL) {
        ESP_LOGE(TAG, "WiFi 未连接，请先连接 WiFi (wifi_set)");
    } else {
        ESP_LOGE(TAG, "启动失败: %s", esp_err_to_name(ret));
    }

    return ret == ESP_OK ? 0 : 1;
}

/**
 * @brief net_stop 命令 - 停止通信
 */
static int cmd_net_stop(int argc, char **argv)
{
    if (!network_app_is_running()) {
        printf("通信未在运行\n");
        return 0;
    }

    esp_err_t ret = network_app_stop();
    if (ret == ESP_OK) {
        printf("网络通信已停止\n");
    }
    return ret == ESP_OK ? 0 : 1;
}

/**
 * @brief net_send 命令 - 发送数据
 */
static int cmd_net_send(int argc, char **argv)
{
    if (argc < 2) {
        ESP_LOGE(TAG, "用法: net_send <MESSAGE>");
        ESP_LOGE(TAG, "示例: net_send Hello");
        return 1;
    }

    if (!network_app_is_running()) {
        ESP_LOGE(TAG, "网络通信未启动，请先启动 (net_start)");
        return 1;
    }

    // 拼接所有参数作为消息
    char message[256] = {0};
    size_t offset = 0;
    for (int i = 1; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (offset + len + 1 < sizeof(message)) {
            if (i > 1) {
                message[offset++] = ' ';
            }
            memcpy(message + offset, argv[i], len);
            offset += len;
        }
    }

    esp_err_t ret = network_app_send_string(message);
    if (ret == ESP_OK) {
        printf("消息已发送: %s\n", message);
    } else {
        ESP_LOGE(TAG, "发送失败: %s", esp_err_to_name(ret));
    }

    return ret == ESP_OK ? 0 : 1;
}

/**
 * @brief net_status 命令 - 显示状态
 */
static int cmd_net_status(int argc, char **argv)
{
    network_app_print_status();
    return 0;
}

/**
 * @brief net_localip 命令 - 显示本地 IP
 */
static int cmd_net_localip(int argc, char **argv)
{
    char ip_str[16];
    esp_err_t ret = network_app_get_local_ip(ip_str, sizeof(ip_str));

    if (ret == ESP_OK) {
        printf("本地 IP: %s\n", ip_str);
    } else if (ret == ESP_FAIL) {
        ESP_LOGE(TAG, "WiFi 未连接");
    } else {
        ESP_LOGE(TAG, "获取 IP 失败: %s", esp_err_to_name(ret));
    }

    return ret == ESP_OK ? 0 : 1;
}

/**
 * @brief net_help 命令 - 显示帮助
 */
static int cmd_net_help(int argc, char **argv)
{
    printf("\n========== 网络通信命令 ==========\n");
    printf("net_type <udp|tcp>   - 设置通信类型\n");
    printf("net_mode <client|server> - 设置通信模式\n");
    printf("net_target <IP> <PORT>  - 设置目标地址 (客户端模式)\n");
    printf("net_port <PORT>         - 设置本地端口 (服务器模式)\n");
    printf("net_start               - 启动通信\n");
    printf("net_stop                - 停止通信\n");
    printf("net_send <MESSAGE>      - 发送消息\n");
    printf("net_status              - 显示网络状态\n");
    printf("net_localip             - 显示本地 IP\n");
    printf("net_help                - 显示此帮助\n");
    printf("\n示例:\n");
    printf("  UDP 服务器:\n");
    printf("    net_type udp\n");
    printf("    net_mode server\n");
    printf("    net_port 5000\n");
    printf("    net_start\n");
    printf("\n");
    printf("  UDP 客户端:\n");
    printf("    net_type udp\n");
    printf("    net_mode client\n");
    printf("    net_target 192.168.1.100 5000\n");
    printf("    net_start\n");
    printf("    net_send Hello\n");
    printf("\n");
    printf("  TCP 客户端:\n");
    printf("    net_type tcp\n");
    printf("    net_mode client\n");
    printf("    net_target 192.168.1.100 5000\n");
    printf("    net_start\n");
    printf("    net_send Hello\n");
    printf("===================================\n\n");
    return 0;
}

// 命令定义数组
static const esp_console_cmd_t s_commands[] = {
    {
        .command = "net_type",
        .help = "设置通信类型 (udp/tcp)",
        .hint = "<udp|tcp>",
        .func = &cmd_net_type,
    },
    {
        .command = "net_mode",
        .help = "设置通信模式 (client/server)",
        .hint = "<client|server>",
        .func = &cmd_net_mode,
    },
    {
        .command = "net_target",
        .help = "设置目标地址 (客户端模式)",
        .hint = "<IP> <PORT>",
        .func = &cmd_net_target,
    },
    {
        .command = "net_port",
        .help = "设置本地端口 (服务器模式)",
        .hint = "<PORT>",
        .func = &cmd_net_port,
    },
    {
        .command = "net_start",
        .help = "启动网络通信",
        .hint = NULL,
        .func = &cmd_net_start,
    },
    {
        .command = "net_stop",
        .help = "停止网络通信",
        .hint = NULL,
        .func = &cmd_net_stop,
    },
    {
        .command = "net_send",
        .help = "发送消息",
        .hint = "<MESSAGE>",
        .func = &cmd_net_send,
    },
    {
        .command = "net_status",
        .help = "显示网络状态",
        .hint = NULL,
        .func = &cmd_net_status,
    },
    {
        .command = "net_localip",
        .help = "显示本地 IP 地址",
        .hint = NULL,
        .func = &cmd_net_localip,
    },
    {
        .command = "net_help",
        .help = "显示网络命令帮助",
        .hint = NULL,
        .func = &cmd_net_help,
    },
};

/**
 * @brief 注册网络命令
 */
esp_err_t network_cmd_register(void)
{
    esp_err_t ret = ESP_OK;
    size_t num_commands = sizeof(s_commands) / sizeof(s_commands[0]);

    for (size_t i = 0; i < num_commands; i++) {
        esp_err_t err = esp_console_cmd_register(&s_commands[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "注册命令 '%s' 失败: %s",
                     s_commands[i].command, esp_err_to_name(err));
            ret = err;
        }
    }

    return ret;
}
