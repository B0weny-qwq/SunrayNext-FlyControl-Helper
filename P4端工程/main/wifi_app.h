/**
 * @file wifi_app.h
 * @brief WiFi 应用层接口定义
 *
 * @details
 * 本模块提供 WiFi STA（Station）模式的初始化和连接管理功能。
 * WiFi 连接通过 esp_hosted 转发到 ESP32-C5 实际执行。
 *
 * @author P4 Team
 * @date 2026-03-25
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "esp_err.h"
#include "esp_netif_types.h"

/**
 * @brief WiFi 连接最大重试次数
 *
 * 当 WiFi 连接断开后，允许的最大自动重连次数。
 */
#define WIFI_MAX_RETRY        5

/**
 * @brief WiFi 重连延迟时间（毫秒）
 *
 * 断开后等待的时间再发起重连，避免打断 ESP32-C5 的连接流程。
 */
#define WIFI_RETRY_DELAY_MS   3000

/**
 * @defgroup wifi_app WiFi Application
 * @brief WiFi 应用模块
 * @{
 */

/**
 * @brief 初始化 WiFi STA 组件
 *
 * @details
 * 执行 WiFi 驱动的完整初始化流程：
 * - 初始化 NVS 存储
 * - 初始化 TCP/IP 堆栈（LwIP）
 * - 创建默认事件循环
 * - 创建 WiFi STA 网络接口
 * - 注册 WiFi 事件处理器
 *
 * @note
 * 初始化完成后，WiFi 处于停止状态，需要调用
 * wifi_app_connect_with_creds() 才能开始连接。
 *
 * @return
 *   - ESP_OK: 初始化成功
 *   - 其他: 初始化失败（参考 esp_err_t）
 *
 * @see wifi_app_connect_with_creds()
 */
esp_err_t wifi_app_init(void);

/**
 * @brief 使用指定凭证连接 WiFi
 *
 * @param ssid     WiFi 网络名称（SSID）
 * @param password WiFi 密码
 *
 * @details
 * 配置并启动 WiFi STA 连接：
 * - 停止现有 WiFi 连接（如有）
 * - 设置新的 SSID 和密码
 * - 启动 WiFi 并等待连接成功
 * - 自动处理连接断开和重连逻辑
 *
 * @note
 * 支持 WPA2-PSK 和 WPA3-SAE 认证方式。
 * 连接过程中会阻塞直到连接成功或失败。
 *
 * @return
 *   - ESP_OK: 连接成功
 *   - ESP_ERR_INVALID_ARG: ssid 或 password 为空
 *   - ESP_FAIL: 连接失败
 *
 * @see wifi_app_init()
 * @see wifi_app_deinit()
 */
esp_err_t wifi_app_connect_with_creds(const char *ssid, const char *password);

/**
 * @brief 获取当前 IP 地址信息
 *
 * @param[out] out_ip 指向 esp_netif_ip_info_t 的指针，用于输出 IP 信息
 *
 * @details
 * 获取 WiFi 连接后分配的 IP 地址、子网掩码和网关地址。
 * 仅在成功获取 IP 后有效。
 *
 * @param[out] out_ip 指向存储 IP 信息的结构体
 *
 * @return
 *   - true:  获取成功，out_ip 已填充
 *   - false: 未获取 IP 或 out_ip 为 NULL
 */
bool wifi_app_get_ip_info(esp_netif_ip_info_t *out_ip);

/**
 * @brief 释放 WiFi 资源
 *
 * @details
 * 停止 WiFi、释放驱动资源、删除事件组。
 * 通常在系统关闭或重启 WiFi 时调用。
 */
void wifi_app_deinit(void);

/**
 * @brief 检查 WiFi 是否已连接并获取 IP
 *
 * @return
 *   - true:  WiFi 已连接并获取到 IP
 *   - false: WiFi 未连接或未获取 IP
 */
bool wifi_app_is_connected(void);

/** @} */ // end of wifi_app group

#ifdef __cplusplus
}
#endif
