/**
 * @file console_app.c
 * @brief Console 命令行交互实现
 *
 * @details
 * 本模块实现 ESP-IDF Console REPL 功能，提供交互式命令行界面：
 * - 配置 UART 硬件接口作为命令输入
 * - 注册并处理可用命令
 * - 提供 WiFi 连接命令接口
 *
 * 支持命令：
 * - \c echo: 回显输入内容
 * - \c wifi_set: 连接指定 WiFi 网络
 *
 * @author P4 Team
 * @date 2026-03-25
 *
 * @defgroup console_app Console Application
 * @{
 */

#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"
#include "console_app.h"
#include "wifi_app.h"
#include "network_cmd.h"
#include "bridge_cmd.h"

/**
 * @brief 日志输出标签
 */
static const char *TAG = "console_app";

/**
 * @brief REPL 实例指针
 *
 * 保存 REPL 环境指针，用于后续操作。
 */
static esp_console_repl_t *s_repl = NULL;

/**
 * @brief Echo 命令处理函数
 *
 * @param argc 命令参数个数
 * @param argv 命令参数数组
 *
 * @details
 * 将输入的内容原样回显输出。
 * 用于测试命令解析是否正常工作。
 *
 * @return 始终返回 0
 */
static int cmd_echo(int argc, char **argv)
{
    if (argc < 2) {
        ESP_LOGI(TAG, "(空输入)");
        return 0;
    }
    printf("\n>>> ");
    for (int i = 1; i < argc; i++) {
        printf("%s%s", argv[i], i < argc - 1 ? " " : "");
    }
    printf(" <<<\n\n");
    return 0;
}

/**
 * @brief WiFi 设置命令处理函数
 *
 * @param argc 命令参数个数
 * @param argv 命令参数数组
 *             - argv[1]: WiFi SSID
 *             - argv[2]: WiFi 密码
 *
 * @details
 * 调用 wifi_app_connect_with_creds() 连接指定 WiFi 网络。
 *
 * @return
 *   - 0: 连接成功或命令参数正确
 *   - 1: 参数错误（缺少 SSID 或密码）
 *   - 其他: WiFi 连接失败
 */
static int cmd_wifi_set(int argc, char **argv)
{
    if (argc < 3) {
        ESP_LOGE(TAG, "用法: wifi_set <ssid> <password>");
        return 1;
    }
    ESP_LOGI(TAG, "正在连接 WiFi: SSID=[%s]", argv[1]);
    esp_err_t ret = wifi_app_connect_with_creds(argv[1], argv[2]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi 连接失败: %s", esp_err_to_name(ret));
    }
    return ret;
}

/**
 * @brief 命令定义数组
 *
 * 定义所有可用的 Console 命令。
 */
static const esp_console_cmd_t s_commands[] = {
    {
        .command  = "echo",
        .help     = "回显输入内容\n  用法: echo <任意内容>",
        .hint     = NULL,
        .func     = &cmd_echo,
    },
    {
        .command  = "wifi_set",
        .help     = "连接指定 WiFi\n  用法: wifi_set <ssid> <password>",
        .hint     = "<ssid> <password>",
        .func     = &cmd_wifi_set,
    },
};

int console_app_init(void)
{
    // 1. 配置 REPL (交互式控制台)
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "esp32p4> ";
    repl_config.max_cmdline_length = 256;

    // 2. 配置 UART 硬件
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    // 3. 创建 REPL 实例
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &s_repl));

    // 4. 注册命令（必须在启动 REPL 之前）
    for (size_t i = 0; i < sizeof(s_commands) / sizeof(s_commands[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&s_commands[i]));
    }

    // 4.1 注册网络通信命令
    ESP_ERROR_CHECK(network_cmd_register());

    // 4.2 注册桥接/UART 命令
    ESP_ERROR_CHECK(bridge_cmd_register());

    ESP_LOGI(TAG, "Console REPL 已初始化，共注册 %d 个命令",
             (int)(sizeof(s_commands) / sizeof(s_commands[0])) + 10 + 4);  // +10 网络命令, +4 桥接命令

    // 5. 启动 REPL（此函数内部会阻塞，命令在独立任务中处理）
    ESP_ERROR_CHECK(esp_console_start_repl(s_repl));
    return 0;  // 永远不会执行
}

/** @} */ // end of console_app group
