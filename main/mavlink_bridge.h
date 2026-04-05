/**
 * @file mavlink_bridge.h
 * @brief MAVLink 多串口双向桥接模块
 *
 * ============================================================================
 *                              用户配置区
 * ============================================================================
 *
 * 在此文件顶部修改配置参数，编译后生效。
 */

/* ==================== UART 端口配置 ==================== */

/**
 * @name TELEM1 配置 (飞控主端口)
 * @{
 */
#ifndef MAVLINK_TELEM1_ENABLED
#define MAVLINK_TELEM1_ENABLED       1       /**< 1=启用, 0=禁用 */
#endif
#ifndef MAVLINK_TELEM1_BAUD
#define MAVLINK_TELEM1_BAUD          115200  /**< 波特率 */
#endif
#ifndef MAVLINK_TELEM1_TX_PIN
#define MAVLINK_TELEM1_TX_PIN        20       /**< TX 引脚 */
#endif
#ifndef MAVLINK_TELEM1_RX_PIN
#define MAVLINK_TELEM1_RX_PIN        21       /**< RX 引脚 */
#endif
/** @} */

/**
 * @name TELEM2 配置 (飞控从端口)
 * @{
 */
#ifndef MAVLINK_TELEM2_ENABLED
#define MAVLINK_TELEM2_ENABLED       1       /**< 1=启用, 0=禁用 */
#endif
#ifndef MAVLINK_TELEM2_BAUD
#define MAVLINK_TELEM2_BAUD          115200   /**< 波特率 */
#endif
#ifndef MAVLINK_TELEM2_TX_PIN
#define MAVLINK_TELEM2_TX_PIN        10      /**< TX 引脚 */
#endif
#ifndef MAVLINK_TELEM2_RX_PIN
#define MAVLINK_TELEM2_RX_PIN        11      /**< RX 引脚 */
#endif
/** @} */

/**
 * @name DEBUG 配置 (调试串口)
 * @{
 */
#ifndef MAVLINK_DEBUG_ENABLED
#define MAVLINK_DEBUG_ENABLED        1       /**< 1=启用, 0=禁用 */
#endif
#ifndef MAVLINK_DEBUG_BAUD
#define MAVLINK_DEBUG_BAUD           115200  /**< 波特率 */
#endif
#ifndef MAVLINK_DEBUG_TX_PIN
#define MAVLINK_DEBUG_TX_PIN         22       /**< TX 引脚 */
#endif
#ifndef MAVLINK_DEBUG_RX_PIN
#define MAVLINK_DEBUG_RX_PIN         23       /**< RX 引脚 */
#endif
/** @} */

/* ==================== 网络配置 ==================== */

#ifndef MAVLINK_UDP_PORT
#define MAVLINK_UDP_PORT             8888   /**< UDP 监听端口 */
#endif

#ifndef MAVLINK_TCP_PORT
#define MAVLINK_TCP_PORT             8889   /**< TCP 监听端口，0=禁用 */
#endif

#ifndef MAVLINK_MAX_TCP_CLIENTS
#define MAVLINK_MAX_TCP_CLIENTS      5       /**< 最大 TCP 客户端数 */
#endif

/* ==================== 队列配置 ==================== */

#ifndef MAVLINK_QUEUE_SIZE
#define MAVLINK_QUEUE_SIZE           32      /**< 有序队列大小 */
#endif

#ifndef MAVLINK_SEQ_WINDOW
#define MAVLINK_SEQ_WINDOW           10       /**< 序列号容错窗口 */
#endif

/* ==================== MAVLink 协议配置 ==================== */

#define MAVLINK_STX_V1               0xFE    /**< MAVLink v1 帧起始 */
#define MAVLINK_HEADER_LEN           6       /**< 帧头长度 */
#define MAVLINK_FOOTER_LEN           2       /**< 帧尾长度 */
#define MAVLINK_MIN_FRAME_LEN        (MAVLINK_HEADER_LEN + MAVLINK_FOOTER_LEN)
#define MAVLINK_MAX_PAYLOAD_LEN      255     /**< 最大负载 */
#define MAVLINK_MAX_FRAME_LEN        (MAVLINK_MAX_PAYLOAD_LEN + MAVLINK_MIN_FRAME_LEN)

/* ==================== 调试配置 ==================== */

#ifndef MAVLINK_DEBUG_LOG
#define MAVLINK_DEBUG_LOG            1       /**< 调试日志开关 */
#endif

#ifndef MAVLINK_LOG_HEXDUMP
#define MAVLINK_LOG_HEXDUMP          0       /**< HEX dump 日志开关 */
#endif

/* ==================== 内部常量 ==================== */

#define MAVLINK_MAX_UARTS            3       /**< 最大 UART 数量 */

/* ============================================================================
 *                              用户配置区结束
 * ============================================================================
 */

#ifndef MAVLINK_BRIDGE_H
#define MAVLINK_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup mavlink_bridge MAVLink Bridge
 * @{
 */

/* ==================== 枚举定义 ==================== */

/**
 * @brief UART 端口 ID 枚举
 *
 * 固定分配的串口用途，ID 即索引。
 */
typedef enum {
    UART_ID_TELEM1 = 1,    /**< TELEM1: 飞控主端口 (UART1, GPIO20/21) */
    UART_ID_TELEM2 = 2,    /**< TELEM2: 飞控从端口 (UART2, GPIO10/11) */
    UART_ID_DEBUG = 3,      /**< DEBUG:  调试串口   (UART3, GPIO22/23) */
    UART_ID_MAX
} mavlink_uart_id_t;

/**
 * @brief 桥接运行状态
 */
typedef enum {
    MAVLINK_STATE_IDLE = 0,           /**< 空闲 */
    MAVLINK_STATE_INITIALIZED,          /**< 已初始化 */
    MAVLINK_STATE_RUNNING,              /**< 运行中 */
    MAVLINK_STATE_ERROR                 /**< 错误状态 */
} mavlink_state_t;

/* ==================== 类型定义 ==================== */

/**
 * @brief MAVLink 桥接句柄（不透明类型）
 */
typedef struct mavlink_bridge_t *mavlink_bridge_handle_t;

/**
 * @brief 网络数据回调函数
 *
 * @param uart_id  目标 UART ID (转发到该串口)
 * @param data     数据缓冲区
 * @param len      数据长度
 * @param ctx      用户上下文
 */
typedef void (*net_data_callback_t)(uint8_t uart_id, const uint8_t *data, size_t len, void *ctx);

/**
 * @brief UART 数据回调函数
 *
 * @param uart_id  来源 UART ID
 * @param data     数据缓冲区
 * @param len      数据长度
 * @param ctx      用户上下文
 */
typedef void (*uart_data_callback_t)(uint8_t uart_id, const uint8_t *data, size_t len, void *ctx);

/**
 * @brief UART 配置结构
 */
typedef struct {
    mavlink_uart_id_t id;          /**< UART ID */
    int baud_rate;                 /**< 波特率 */
    uint8_t tx_pin;                /**< TX 引脚 */
    uint8_t rx_pin;                /**< RX 引脚 */
    bool enabled;                  /**< 是否启用 */
    const char *name;              /**< 名称标签 */

    /* 数据回调 */
    uart_data_callback_t data_callback;  /**< 数据接收回调 */
    void *callback_ctx;                  /**< 回调上下文 */

    /* MAVLink 配置 */
    bool mavlink_mode;             /**< MAVLink 模式 */
    uint8_t sysid_filter;          /**< 系统 ID 过滤 (0=不过滤) */
} mavlink_uart_config_t;

/**
 * @brief MAVLink 桥接配置结构
 */
typedef struct {
    uint16_t udp_port;             /**< UDP 监听端口 */
    uint16_t tcp_port;              /**< TCP 监听端口，0=禁用 */
    uint8_t queue_size;             /**< 有序队列大小 */
    bool ordered_mode;              /**< 有序模式 (true=保证顺序) */
} mavlink_bridge_config_t;

/**
 * @brief 桥接统计信息
 */
typedef struct {
    uint32_t udp_rx_count;         /**< UDP 接收帧数 */
    uint32_t tcp_rx_count;          /**< TCP 接收帧数 */
    uint32_t udp_drop_count;        /**< UDP 丢弃帧数 */
    uint32_t uart_tx_bytes[UART_ID_MAX];   /**< 各 UART 发送字节数 */
    uint32_t uart_rx_bytes[UART_ID_MAX];   /**< 各 UART 接收字节数 */
    uint32_t error_count;           /**< 错误计数 */
} mavlink_stats_t;

/* ==================== 宏定义 ==================== */

/**
 * @brief 获取默认 UART 配置
 */
#define MAVLINK_UART_CONFIG_DEFAULT(_id, _name) { \
    .id = _id, \
    .baud_rate = 115200, \
    .tx_pin = UART_PIN_NO_CHANGE, \
    .rx_pin = UART_PIN_NO_CHANGE, \
    .enabled = true, \
    .name = _name, \
    .data_callback = NULL, \
    .callback_ctx = NULL, \
    .mavlink_mode = true, \
    .sysid_filter = 0, \
}

/**
 * @brief 获取默认桥接配置
 */
#define MAVLINK_BRIDGE_CONFIG_DEFAULT() { \
    .udp_port = MAVLINK_UDP_PORT, \
    .tcp_port = MAVLINK_TCP_PORT, \
    .queue_size = MAVLINK_QUEUE_SIZE, \
    .ordered_mode = false, \
}

/* ==================== 核心 API ==================== */

/**
 * @brief 初始化 MAVLink 桥接
 *
 * @param config  桥接配置
 * @param handle  输出：桥接句柄
 * @return ESP_OK 成功
 */
esp_err_t mavlink_bridge_init(const mavlink_bridge_config_t *config,
                               mavlink_bridge_handle_t *handle);

/**
 * @brief 销毁 MAVLink 桥接
 *
 * @param handle 桥接句柄
 * @return ESP_OK 成功
 */
esp_err_t mavlink_bridge_deinit(mavlink_bridge_handle_t handle);

/**
 * @brief 检查桥接是否已初始化
 */
bool mavlink_bridge_is_initialized(mavlink_bridge_handle_t handle);

/**
 * @brief 获取桥接状态
 */
mavlink_state_t mavlink_bridge_get_state(mavlink_bridge_handle_t handle);

/* ==================== 网络 -> UART 转发 API ==================== */

/**
 * @brief 发送数据到所有启用的 UART (网络转发串口)
 *
 * @param handle 桥接句柄
 * @param data   数据缓冲区
 * @param len    数据长度
 * @return ESP_OK 成功
 */
esp_err_t mavlink_bridge_net_to_all_uarts(mavlink_bridge_handle_t handle,
                                           const uint8_t *data, size_t len);

/**
 * @brief 发送数据到指定 UART (网络转发到单个串口)
 *
 * @param handle  桥接句柄
 * @param uart_id UART ID (UART_ID_TELEM1, etc)
 * @param data    数据缓冲区
 * @param len     数据长度
 * @return ESP_OK 成功
 */
esp_err_t mavlink_bridge_net_to_uart(mavlink_bridge_handle_t handle,
                                      uint8_t uart_id,
                                      const uint8_t *data, size_t len);

/* ==================== UART -> 网络 转发 API ==================== */

/**
 * @brief 发送 UART 数据到所有网络连接 (串口��发到网络)
 *
 * @param handle  桥接句柄
 * @param uart_id 来源 UART ID
 * @param data    数据缓冲区
 * @param len     数据长度
 * @return ESP_OK 成功
 */
esp_err_t mavlink_bridge_uart_to_net(mavlink_bridge_handle_t handle,
                                       uint8_t uart_id,
                                       const uint8_t *data, size_t len);

/**
 * @brief 设置网络广播模式
 *
 * @param handle 桥接句柄
 * @param enable true=启用广播
 */
void mavlink_bridge_set_net_broadcast(mavlink_bridge_handle_t handle, bool enable);

/* ==================== UART 操作 API ==================== */

/**
 * @brief 发送数据到指定 UART
 *
 * @param handle  桥接句柄
 * @param uart_id UART ID
 * @param data    数据缓冲区
 * @param len     数据长度
 * @return ESP_OK 成功
 */
esp_err_t mavlink_bridge_send_to_uart(mavlink_bridge_handle_t handle,
                                       uint8_t uart_id,
                                       const uint8_t *data, size_t len);

/**
 * @brief 修改 UART 波特率
 *
 * @param handle    桥接句柄
 * @param uart_id   UART ID
 * @param baud_rate 新波特率
 * @return ESP_OK 成功
 */
esp_err_t mavlink_bridge_set_baudrate(mavlink_bridge_handle_t handle,
                                        uint8_t uart_id,
                                        int baud_rate);

/**
 * @brief 获取 UART 当前波特率
 */
int mavlink_bridge_get_baudrate(mavlink_bridge_handle_t handle, uint8_t uart_id);

/**
 * @brief 启用/禁用 UART
 */
esp_err_t mavlink_bridge_enable_uart(mavlink_bridge_handle_t handle,
                                      uint8_t uart_id, bool enable);

/* ==================== 回调注册 API ==================== */

/**
 * @brief 注册网络数据到达回调
 *
 * 当 UDP/TCP 收到数据时调用。
 *
 * @param handle   桥接句柄
 * @param callback 回调函数
 * @param ctx      用户上下文
 */
void mavlink_bridge_on_net_data(mavlink_bridge_handle_t handle,
                                 net_data_callback_t callback, void *ctx);

/**
 * @brief 注册指定 UART 的数据回调
 *
 * @param handle   桥接句柄
 * @param uart_id  UART ID
 * @param callback 回调函数
 * @param ctx      用户上下文
 */
void mavlink_bridge_on_uart_data(mavlink_bridge_handle_t handle,
                                  uint8_t uart_id,
                                  uart_data_callback_t callback, void *ctx);

/* ==================== 统计 API ==================== */

/**
 * @brief 获取桥接统计
 */
void mavlink_bridge_get_stats(mavlink_bridge_handle_t handle,
                               mavlink_stats_t *stats);

/**
 * @brief 重置统计
 */
void mavlink_bridge_reset_stats(mavlink_bridge_handle_t handle);

/**
 * @brief 获取本地 IP
 */
esp_err_t mavlink_bridge_get_local_ip(mavlink_bridge_handle_t handle,
                                       char *ip_str, size_t len);

/* ==================== 辅助函数 ==================== */

/**
 * @brief 检查数据是否为 MAVLink 帧
 */
bool mavlink_is_frame(const uint8_t *data, size_t len);

/**
 * @brief 获取 MAVLink 序列号
 */
uint8_t mavlink_get_sequence(const uint8_t *data);

/**
 * @brief 获取 MAVLink 消息 ID
 */
uint8_t mavlink_get_msg_id(const uint8_t *data);

/**
 * @brief 获取 MAVLink 负载长度
 */
uint8_t mavlink_get_payload_len(const uint8_t *data);

/**
 * @brief 验证 MAVLink CRC
 */
bool mavlink_verify_crc(const uint8_t *data, size_t len);

/**
 * @brief 获取 UART 名称（静态版本，根据枚举值映射）
 */
const char* mavlink_get_uart_name(uint8_t uart_id);

/**
 * @brief 获取已配置的 UART 数量
 */
uint8_t mavlink_bridge_get_count(mavlink_bridge_handle_t handle);

/**
 * @brief 获取 UART 名称（动态版本，从实际配置中读取）
 * @note 建议优先使用此函数，显示更准确
 */
const char* mavlink_bridge_get_uart_name(mavlink_bridge_handle_t handle, uint8_t uart_id);

/**
 * @brief 获取全局桥接句柄
 */
mavlink_bridge_handle_t mavlink_bridge_get_handle(void);

/** @} */ // end of mavlink_bridge group

#ifdef __cplusplus
}
#endif

#endif /* MAVLINK_BRIDGE_H */