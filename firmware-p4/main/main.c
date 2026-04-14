/**
 * @file main.c
 * @brief P4 WiFi Remote 主程序入口
 *
 * ============================================================================
 * 使用说明
 * ============================================================================
 *
 * UART 配置：在 main/mavlink_bridge.h 顶部修改宏定义
 * 网络配置：在 main/mavlink_bridge.h 顶部修改宏定义
 *
 * vrpn-sim-mavlink 配置：
 *   PC 端启动: ./build/fake_vrpn_uav_server --bind :3883 --num-trackers 1 --rate 50
 *   ESP32 监听: UDP 8888
 *   wifi_set  LingLONG_5G 12341234
     net_target  192.168.110.166 8888
     uart_baud 0 460800
 */

#include <stdio.h>
#include "esp_log.h"
#include "wifi_app.h"
#include "console_app.h"
#include "mavlink_bridge.h"
#include "freertos/task.h"

static const char *TAG = "main";

/* MAVLink 桥接句柄 */
static mavlink_bridge_handle_t s_bridge = NULL;

/**
 * @brief UART 数据回调示例
 *
 * 当任意 UART 接收到数据时调用。
 * 可在此处理数据、转发、过滤等。
 */
static void uart_data_callback(uint8_t uart_id, const uint8_t *data, size_t len, void *ctx)
{
    ESP_LOGI(TAG, "[%s] Rx: %d bytes", mavlink_get_uart_name(uart_id), len);

    if (MAVLINK_LOG_HEXDUMP && len > 0) {
        printf("RAW: ");
        for (size_t i = 0; i < len && i < 32; i++) {
            printf("%02X ", data[i]);
        }
        printf("\r\n");
    }

    /* MAVLink 帧解析 */
    if (mavlink_is_frame(data, len)) {
        uint8_t seq = mavlink_get_sequence(data);
        uint8_t msg_id = mavlink_get_msg_id(data);
        uint8_t payload_len = mavlink_get_payload_len(data);

        ESP_LOGI(TAG, "  MAVLink: SEQ=%d, MSG=0x%02X, LEN=%d",
                 seq, msg_id, payload_len);
    }
}

/**
 * @brief 网络数据回调示例
 *
 * 当 UDP/TCP 收到数据时调用。
 * 可在此处理网络数据、路由到特定串口等。
 */
static void net_data_callback(uint8_t uart_id, const uint8_t *data, size_t len, void *ctx)
{
    ESP_LOGI(TAG, "[NET] Rx: %d bytes", len);

    if (mavlink_is_frame(data, len)) {
        uint8_t seq = mavlink_get_sequence(data);
        uint8_t msg_id = mavlink_get_msg_id(data);

        ESP_LOGI(TAG, "  MAVLink: SEQ=%d, MSG=0x%02X", seq, msg_id);
    }
}

/**
 * @brief 初始化 MAVLink 桥接
 */
static esp_err_t init_bridge(void)
{
    /* 使用默认配置（从宏定义读取） */
    mavlink_bridge_config_t config = MAVLINK_BRIDGE_CONFIG_DEFAULT();

    /* 初始化桥接 */
    esp_err_t err = mavlink_bridge_init(&config, &s_bridge);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Bridge init failed: %s", esp_err_to_name(err));
        return err;
    }

    /* 注册回调 */
    mavlink_bridge_on_net_data(s_bridge, net_data_callback, NULL);

    for (int i = 0; i < UART_ID_MAX; i++) {
        mavlink_bridge_on_uart_data(s_bridge, i, uart_data_callback, NULL);
    }

    /* 打印信息 */
    char local_ip[16];
    if (mavlink_bridge_get_local_ip(s_bridge, local_ip, sizeof(local_ip)) == ESP_OK) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, " P4 WiFi Remote - MAVLink Bridge");
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, " Listen: UDP %s:%d, TCP %s:%d",
                 local_ip, MAVLINK_UDP_PORT, local_ip, MAVLINK_TCP_PORT);
        ESP_LOGI(TAG, "");

#if MAVLINK_TELEM1_ENABLED
        ESP_LOGI(TAG, " TELEM1: %d baud, TX=%d, RX=%d",
                 MAVLINK_TELEM1_BAUD, MAVLINK_TELEM1_TX_PIN, MAVLINK_TELEM1_RX_PIN);
#endif
#if MAVLINK_TELEM2_ENABLED
        ESP_LOGI(TAG, " TELEM2: %d baud, TX=%d, RX=%d",
                 MAVLINK_TELEM2_BAUD, MAVLINK_TELEM2_TX_PIN, MAVLINK_TELEM2_RX_PIN);
#endif
#if MAVLINK_DEBUG_ENABLED
        ESP_LOGI(TAG, " DEBUG:  %d baud, TX=%d, RX=%d",
                 MAVLINK_DEBUG_BAUD, MAVLINK_DEBUG_TX_PIN, MAVLINK_DEBUG_RX_PIN);
#endif

        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "");
    }

    return ESP_OK;
}

/**
 * @brief 打印桥接状态
 */
static void print_status(void)
{
    if (!s_bridge) return;

    mavlink_stats_t stats;
    mavlink_bridge_get_stats(s_bridge, &stats);

    char local_ip[16];
    mavlink_bridge_get_local_ip(s_bridge, local_ip, sizeof(local_ip));

    ESP_LOGI(TAG, "========== MAVLink Bridge Status ==========");
    ESP_LOGI(TAG, "IP: %s", local_ip);
    ESP_LOGI(TAG, "UDP RX: %lu, Drop: %lu",
             (unsigned long)stats.udp_rx_count, (unsigned long)stats.udp_drop_count);
    ESP_LOGI(TAG, "TCP RX: %lu", (unsigned long)stats.tcp_rx_count);

    for (int i = 0; i < UART_ID_MAX; i++) {
        ESP_LOGI(TAG, " %s: TX=%lu, RX=%lu",
                 mavlink_get_uart_name(i),
                 (unsigned long)stats.uart_tx_bytes[i],
                 (unsigned long)stats.uart_rx_bytes[i]);
    }

    ESP_LOGI(TAG, "==========================================");
}

/**
 * @brief 主入口
 */
void app_main(void)
{
    ESP_LOGI(TAG, "P4 WiFi Remote 启动");

    /* 初始化 WiFi */
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_ERROR_CHECK(wifi_app_init());

    /* 初始化 MAVLink 桥接 */
    ESP_ERROR_CHECK(init_bridge());

    /* 初始化 Console */
    console_app_init();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Console 就绪，输入 help 查看命令");
    ESP_LOGI(TAG, "========================================");

    vTaskSuspend(NULL);
}
