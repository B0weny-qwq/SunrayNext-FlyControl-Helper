/**
 * @file wifi_app.c
 * @brief WiFi 应用层实现
 *
 * @details
 * 本模块实现 WiFi STA 模式的连接管理功能，包括：
 * - WiFi 驱动初始化
 * - 事件处理和连接状态管理
 * - 自动重连机制
 * - IP 地址获取
 *
 * WiFi 实际连接通过 esp_hosted 转发到 ESP32-C5 执行。
 *
 * @author P4 Team
 * @date 2026-03-25
 *
 * @defgroup wifi_app WiFi Application
 * @{
 */

#include "wifi_app.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

/**
 * @brief 日志输出标签
 */
static const char *TAG = "wifi_app";

/**
 * @brief WiFi 事件组句柄
 *
 * 用于同步 WiFi 连接状态事件。
 */
static EventGroupHandle_t s_wifi_event_group;

/**
 * @brief WiFi 连接成功事件位
 *
 * 当 WiFi 连接成功或获取到 IP 时设置此位。
 */
#define WIFI_CONNECTED_BIT  BIT0

/**
 * @brief WiFi 重试计数器
 *
 * 记录当前重连次数。
 */
static int s_retry_num = 0;

/**
 * @brief IP 地址信息
 *
 * 存储从 DHCP 获取的 IP 地址、子网掩码和网关。
 */
static esp_netif_ip_info_t s_ip_info = {0};

/**
 * @brief IP 获取标志
 *
 * 标记是否已成功获取 IP 地址。
 */
static bool s_got_ip = false;

/**
 * @brief WiFi 事件处理器
 *
 * @param arg       事件参数（未使用）
 * @param event_base 事件基类（WIFI_EVENT 或 IP_EVENT）
 * @param event_id   事件 ID
 * @param event_data 事件数据指针
 *
 * @details
 * 处理以下事件：
 * - WIFI_EVENT_STA_START: 启动 STA 后发起连接
 * - WIFI_EVENT_STA_DISCONNECTED: 处理断开连接和自动重连
 * - WIFI_EVENT_STA_CONNECTED: 标记连接成功
 * - IP_EVENT_STA_GOT_IP: 获取 IP 地址
 *
 * @note
 * 断开后会根据重试次数决定是否自动重连。
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        s_retry_num = 0;
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            s_retry_num++;
            ESP_LOGI(TAG, "WiFi 断开，等待 %ds 后重试... (%d/%d)", WIFI_RETRY_DELAY_MS/1000, s_retry_num, WIFI_MAX_RETRY);
            vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_DELAY_MS));
            esp_wifi_connect();
            ESP_LOGI(TAG, "已发送重连命令到 C5");
        } else {
            ESP_LOGI(TAG, "重连次数超限，停止重试");
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "WiFi STA 连接成功！");
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        s_ip_info = event->ip_info;
        s_got_ip = true;
        ESP_LOGI(TAG, "获得 IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_app_init(void)
{
    esp_err_t ret = nvs_flash_init();               // 1. 初始化 NVS（WiFi 驱动依赖）
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());                        // 初始化底层 TCP/IP 堆栈（LwIP）
    ESP_ERROR_CHECK(esp_event_loop_create_default());         // 创建默认事件循环
    esp_netif_create_default_wifi_sta();                      // 创建默认的 WIFI STA 网络接口
    s_wifi_event_group = xEventGroupCreate();                                                                 
                                                              //  注册 WiFi 事件处理程序
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                &wifi_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();       // 初始化 WiFi 驱动
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));   // 设置为 STA 模式
    ESP_LOGI(TAG, "WiFi STA 初始化完成，等待终端输入 wifi_set 命令");
    return ESP_OK;
}

/**
 * @brief 使用指定凭证连接 WiFi
 *
 * @param ssid     WiFi 网络名称（SSID）
 * @param password WiFi 密码
 *
 * @details
 * 配置并启动 WiFi STA 连接：
 * - 停止现有 WiFi 连接（如有）
 * - 设置新的 SSID 和密码（WPA2-PSK / WPA3-SAE）
 * - 启动 WiFi 并等待连接成功
 *
 * @note
 * 连接过程会阻塞，直到连接成功或失败。
 * 自动重连由 wifi_event_handler 处理。
 *
 * @return
 *   - ESP_OK: 连接成功
 *   - ESP_ERR_INVALID_ARG: ssid 或 password 为空
 *   - ESP_FAIL: 连接失败
 *
 * @see wifi_app_init()
 * @see wifi_app_deinit()
 */
esp_err_t wifi_app_connect_with_creds(const char *ssid, const char *password)
{
    if (ssid == NULL || password == NULL) {
        ESP_LOGE(TAG, "SSID 或 Password 为空");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "正在使用以下凭证连接 WiFi:");
    ESP_LOGI(TAG, "  SSID: %s", ssid);

    // 停止现有 WiFi（如已运行），忽略"未启动"错误
    esp_err_t stop_ret = esp_wifi_stop();
    if (stop_ret != ESP_OK && stop_ret != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGW(TAG, "esp_wifi_stop 异常: %s", esp_err_to_name(stop_ret));
    }

    // 构建新的 WiFi 配置
    wifi_config_t new_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e      = WPA3_SAE_PWE_BOTH,
            .sae_h2e_identifier = "",
        },
    };
    strncpy((char *)new_config.sta.ssid,     ssid, sizeof(new_config.sta.ssid) - 1);
    strncpy((char *)new_config.sta.password, password, sizeof(new_config.sta.password) - 1);

    // 更新配置并启动
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &new_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi STA 已启动，等待连接...");

    // 等待连接成功事件
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        if (s_got_ip) {
            ESP_LOGI(TAG, "WiFi 连接成功！IP 地址: " IPSTR, IP2STR(&s_ip_info.ip));
        } else {
            ESP_LOGI(TAG, "WiFi 连接成功！（DHCP 尚未分配 IP）");
        }
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "WiFi 连接失败！");
        return ESP_FAIL;
    }
}

/**
 * @brief 获取当前 IP 地址信息
 *
 * @param[out] out_ip 指向 esp_netif_ip_info_t 的指针，用于输出 IP 信息
 *
 * @return
 *   - true:  获取成功，out_ip 已填充
 *   - false: 未获取 IP 或 out_ip 为 NULL
 *
 * @see wifi_app_connect_with_creds()
 */
bool wifi_app_get_ip_info(esp_netif_ip_info_t *out_ip)
{
    if (out_ip == NULL) return false;
    if (!s_got_ip) return false;
    *out_ip = s_ip_info;
    return true;
}

/**
 * @brief 释放 WiFi 资源
 *
 * @details
 * 停止 WiFi、释放驱动资源、删除事件组。
 * 重置 IP 获取状态和 IP 信息。
 */
void wifi_app_deinit(void)
{
    esp_wifi_stop();
    esp_wifi_deinit();
    vEventGroupDelete(s_wifi_event_group);
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler);
    s_got_ip = false;
    memset(&s_ip_info, 0, sizeof(s_ip_info));
}

bool wifi_app_is_connected(void)
{
    return s_got_ip;
}

/** @} */ // end of wifi_app group
