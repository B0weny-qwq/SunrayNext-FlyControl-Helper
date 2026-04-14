/**
 * @file bridge_cmd.c
 * @brief MAVLink 桥接 UART CLI 命令
 *
 * 提供以下命令：
 * - uart_baud:   设置/查看 UART 波特率
 * - uart_en:    启用/禁用 UART
 * - uart_status: 显示所有 UART 状态
 *
 * @author P4 Team
 * @date 2026-04-05
 */

#include "bridge_cmd.h"

#include <stdio.h>
#include <string.h>

#include "bridge/service/bridge_runtime.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "bridge_cmd";

/* ==================== uart_baud 命令 ==================== */

static int cmd_uart_baud(int argc, char **argv)
{
    if (!mavlink_bridge_get_handle()) {
        ESP_LOGE(TAG, "Bridge 未初始化");
        return 1;
    }

    if (argc < 2) {
        /* 无参数：显示所有 UART 当前波特率 */
        printf("\n========== UART 波特率 ==========\n");
        for (int i = 0; i < UART_ID_MAX; i++) {
            int baud = mavlink_bridge_get_baudrate(mavlink_bridge_get_handle(), i);
            printf(" [%s] %d baud\n", mavlink_get_uart_name(i), baud);
        }
        printf("=================================\n\n");
        return 0;
    }

    /* 参数解析 */
    if (argc < 3) {
        ESP_LOGE(TAG, "用法: uart_baud <id> <baud>");
        ESP_LOGE(TAG, "  id:   0=TELEM1, 1=TELEM2, 2=DEBUG");
        ESP_LOGE(TAG, "  baud: 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600 ...");
        return 1;
    }

    int uart_id = atoi(argv[1]);
    if (uart_id < 0 || uart_id >= UART_ID_MAX) {
        ESP_LOGE(TAG, "无效的 UART ID: %d (有效范围: 0~%d)", uart_id, UART_ID_MAX - 1);
        return 1;
    }

    int baud = atoi(argv[2]);
    if (baud <= 0) {
        ESP_LOGE(TAG, "无效的波特率: %s", argv[2]);
        return 1;
    }

    esp_err_t ret = mavlink_bridge_set_baudrate(mavlink_bridge_get_handle(), (uint8_t)uart_id, baud);
    if (ret == ESP_OK) {
        printf("UART%d (%s) 波特率已设置为 %d\n", uart_id, mavlink_get_uart_name(uart_id), baud);
    } else {
        ESP_LOGE(TAG, "设置波特率失败: %s", esp_err_to_name(ret));
        return 1;
    }

    return 0;
}

/* ==================== uart_en 命令 ==================== */

static int cmd_uart_en(int argc, char **argv)
{
    if (!mavlink_bridge_get_handle()) {
        ESP_LOGE(TAG, "Bridge 未初始化");
        return 1;
    }

    if (argc < 3) {
        ESP_LOGE(TAG, "用法: uart_en <id> <0|1>");
        ESP_LOGE(TAG, "  id: 1=TELEM1, 2=TELEM2, 3=DEBUG");
        ESP_LOGE(TAG, "  0 = 禁用, 1 = 启用");
        return 1;
    }

    int uart_id = atoi(argv[1]);
    uint8_t count = mavlink_bridge_get_count(mavlink_bridge_get_handle());
    if (uart_id < 0 || (uint8_t)uart_id >= count) {
        ESP_LOGE(TAG, "无效的 UART ID: %d (有效范围: 0~%d)", uart_id, count - 1);
        return 1;
    }

    int enable = atoi(argv[2]);
    if (enable != 0 && enable != 1) {
        ESP_LOGE(TAG, "无效的值: %s (请使用 0 或 1)", argv[2]);
        return 1;
    }

    esp_err_t ret = mavlink_bridge_enable_uart(mavlink_bridge_get_handle(), (uint8_t)uart_id, enable ? true : false);
    if (ret == ESP_OK) {
        mavlink_bridge_handle_t h = mavlink_bridge_get_handle();
        printf("UART%d (%s) 已%s\n", uart_id,
               mavlink_bridge_get_uart_name(h, uart_id),
               enable ? "启用" : "禁用");
    } else {
        ESP_LOGE(TAG, "操作失败: %s", esp_err_to_name(ret));
        return 1;
    }

    return 0;
}

/* ==================== uart_status 命令 ==================== */

static int cmd_uart_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!mavlink_bridge_get_handle()) {
        ESP_LOGE(TAG, "Bridge 未初始化");
        return 1;
    }

    mavlink_stats_t stats;
    mavlink_bridge_get_stats(mavlink_bridge_get_handle(), &stats);

    printf("\n========== UART 状态 ==========\n");
    mavlink_bridge_handle_t h = mavlink_bridge_get_handle();
    int count = mavlink_bridge_get_count(h);
    for (int i = 0; i < count; i++) {
        int baud = mavlink_bridge_get_baudrate(h, i);
        const char *name = mavlink_bridge_get_uart_name(h, i);
        mavlink_state_t state = mavlink_bridge_get_state(h);
        (void)state;

        printf(" [%s]\n", name);
        printf("   波特率: %d\n", baud);
        printf("   TX: %lu bytes\n", (unsigned long)stats.uart_tx_bytes[i]);
        printf("   RX: %lu bytes\n", (unsigned long)stats.uart_rx_bytes[i]);
    }
    printf("==============================\n\n");

    return 0;
}

/* ==================== uart_help 命令 ==================== */

static int cmd_uart_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("\n========== UART 桥接命令 ==========\n");
    printf("uart_baud [id] [baud]\n");
    printf("  - 无参数: 显示所有 UART 波特率\n");
    printf("  - 有参数: 设置指定 UART 波特率\n");
    printf("  示例: uart_baud 0 115200\n\n");

    printf("uart_en <id> <0|1>\n");
    printf("  启用/禁用指定 UART\n");
    printf("  示例: uart_en 1 1   (启用 TELEM1)\n");
    printf("  示例: uart_en 2 1   (启用 TELEM2)\n\n");

    printf("uart_status\n");
    printf("  显示所有 UART 的波特率和收发统计\n\n");

    printf("uart_help\n");
    printf("  显示此帮助信息\n");
    printf("==================================\n\n");

    return 0;
}

/* ==================== 命令数组 ==================== */

static const esp_console_cmd_t s_commands[] = {
    {
        .command = "uart_baud",
        .help    = "设置/查看 UART 波特率\n"
                   "  uart_baud          - 显示所有波特率\n"
                   "  uart_baud <id> <baud> - 设置波特率",
        .hint    = "[id] [baud]",
        .func    = &cmd_uart_baud,
    },
    {
        .command = "uart_en",
        .help    = "启用/禁用 UART\n"
                   "  uart_en <id> <0|1>\n"
                   "  0=禁用, 1=启用",
        .hint    = "<id> <0|1>",
        .func    = &cmd_uart_en,
    },
    {
        .command = "uart_status",
        .help    = "显示所有 UART 状态和统计",
        .hint    = NULL,
        .func    = &cmd_uart_status,
    },
    {
        .command = "uart_help",
        .help    = "显示 UART 命令帮助",
        .hint    = NULL,
        .func    = &cmd_uart_help,
    },
};

/* ==================== 注册函数 ==================== */

esp_err_t bridge_cmd_register(void)
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
