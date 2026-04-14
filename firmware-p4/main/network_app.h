/**
 * @file network_app.h
 * @brief P4 WiFi Remote 网络通信模块
 *
 * @details
 * 本模块封装 espp::UdpSocket 和 espp::TcpSocket，提供：
 * - UDP/TCP 通信方式选择
 * - 目标地址和端口配置
 * - 数据收发功能
 * - 接收数据自动打印
 *
 * @author P4 Team
 * @date 2026-04-04
 */

#ifndef NETWORK_APP_H
#define NETWORK_APP_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 网络通信类型
 */
typedef enum {
    NETWORK_TYPE_NONE = 0,  /**< 未设置 */
    NETWORK_TYPE_UDP,       /**< UDP 通信 */
    NETWORK_TYPE_TCP        /**< TCP 通信 */
} network_type_t;

/**
 * @brief 网络通信模式
 */
typedef enum {
    NETWORK_MODE_NONE = 0,  /**< 未设置 */
    NETWORK_MODE_CLIENT,    /**< 客户端模式（主动连接/发送） */
    NETWORK_MODE_SERVER     /**< 服务器模式（监听等待） */
} network_mode_t;

/**
 * @brief 网络配置结构
 */
typedef struct {
    network_type_t type;        /**< 通信类型 */
    network_mode_t mode;        /**< 通信模式 */
    char target_ip[16];         /**< 目标 IP 地址 */
    uint16_t target_port;      /**< 目标端口 */
    uint16_t local_port;       /**< 本地监听端口 */
    bool is_running;           /**< 是否正在运行 */
    uint32_t recv_count;       /**< 接收数据包计数 */
    uint32_t send_count;       /**< 发送数据包计数 */
} network_config_t;

/**
 * @brief 接收回调函数类型
 *
 * @param data 接收到的数据
 * @param len  数据长度
 * @param ip   来源 IP 地址
 * @param port 来源端口
 * @param ctx  用户上下文
 */
typedef void (*network_recv_callback_t)(const uint8_t *data, size_t len,
                                        const char *ip, uint16_t port, void *ctx);

/**
 * @brief 初始化网络通信模块
 *
 * @return ESP_OK 成功
 *         ESP_ERR_NO_MEM 内存不足
 */
esp_err_t network_app_init(void);

/**
 * @brief 反初始化网络通信模块
 *
 * @return ESP_OK 成功
 */
esp_err_t network_app_deinit(void);

/**
 * @brief 设置通信类型
 *
 * @param type 通信类型 (NETWORK_TYPE_UDP / NETWORK_TYPE_TCP)
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_ARG 参数无效
 *         ESP_ERR_INVALID_STATE 正在运行中，无法更改
 */
esp_err_t network_app_set_type(network_type_t type);

/**
 * @brief 获取当前通信类型
 *
 * @return 当前通信类型
 */
network_type_t network_app_get_type(void);

/**
 * @brief 设置通信模式
 *
 * @param mode 通信模式 (NETWORK_MODE_CLIENT / NETWORK_MODE_SERVER)
 * @return ESP_OK 成功
 */
esp_err_t network_app_set_mode(network_mode_t mode);

/**
 * @brief 设置目标地址和端口（客户端模式使用）
 *
 * @param ip   目标 IP 地址字符串
 * @param port 目标端口
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_ARG 参数无效
 */
esp_err_t network_app_set_target(const char *ip, uint16_t port);

/**
 * @brief 设置本地监听端口（服务器模式使用）
 *
 * @param port 本地端口
 * @return ESP_OK 成功
 */
esp_err_t network_app_set_local_port(uint16_t port);

/**
 * @brief 启动网络通信
 *
 * @note 服务器模式：监听本地端口等待连接/数据
 *       客户端模式：连接到目标地址
 *
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_STATE 未配置参数
 *         ESP_ERR_WIFI_NOT_CONNECTED WiFi 未连接
 *         其他错误 启动失败
 */
esp_err_t network_app_start(void);

/**
 * @brief 停止网络通信
 *
 * @return ESP_OK 成功
 */
esp_err_t network_app_stop(void);

/**
 * @brief 发送数据
 *
 * @param data 数据指针
 * @param len  数据长度
 * @return ESP_OK 成功发送
 *         ESP_ERR_INVALID_STATE 未启动
 *         ESP_FAIL 发送失败
 */
esp_err_t network_app_send(const uint8_t *data, size_t len);

/**
 * @brief 发送字符串数据
 *
 * @param str 字符串指针（以 '\0' 结尾）
 * @return ESP_OK 成功发送
 *         ESP_ERR_INVALID_STATE 未启动
 *         ESP_FAIL 发送失败
 */
esp_err_t network_app_send_string(const char *str);

/**
 * @brief 获取当前配置
 *
 * @return 当前配置指针（内部静态对象）
 */
const network_config_t* network_app_get_config(void);

/**
 * @brief 检查是否正在运行
 *
 * @return true 运行中
 *         false 未运行
 */
bool network_app_is_running(void);

/**
 * @brief 获取本地 IP 地址
 *
 * @param ip_str 存储 IP 字符串的缓冲区
 * @param len    缓冲区长度（建议 >= 16）
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_ARG 参数无效
 *         ESP_ERR_WIFI_NOT_CONNECTED WiFi 未连接
 */
esp_err_t network_app_get_local_ip(char *ip_str, size_t len);

/**
 * @brief 注册接收回调函数
 *
 * @param callback 回调函数
 * @param ctx      用户上下文
 */
void network_app_set_recv_callback(network_recv_callback_t callback, void *ctx);

/**
 * @brief 打印网络状态信息
 */
void network_app_print_status(void);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_APP_H */
