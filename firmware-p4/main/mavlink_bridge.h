/**
 * @file mavlink_bridge.h
 * @brief MAVLink 多串口?向?接模?
 *
 * ============================================================================
 *                              用?配置?
 * ============================================================================
 *
 * 在此文件?部修改配置??，??后生效。
 */

/* ==================== UART 端口配置 ==================== */

/**
 * @name TELEM1 配置 (?控主端口)
 * @{
 */
#ifndef MAVLINK_TELEM1_ENABLED
#define MAVLINK_TELEM1_ENABLED       1       /**< 1=?用, 0=禁用 */
#endif
#ifndef MAVLINK_TELEM1_BAUD
#define MAVLINK_TELEM1_BAUD          115200  /**< 波特率 */
#endif
#ifndef MAVLINK_TELEM1_TX_PIN
#define MAVLINK_TELEM1_TX_PIN        20       /**< TX 引? */
#endif
#ifndef MAVLINK_TELEM1_RX_PIN
#define MAVLINK_TELEM1_RX_PIN        21       /**< RX 引? */
#endif
/** @} */

/**
 * @name TELEM2 配置 (?控?端口)
 * @{
 */
#ifndef MAVLINK_TELEM2_ENABLED
#define MAVLINK_TELEM2_ENABLED       1       /**< 1=?用, 0=禁用 */
#endif
#ifndef MAVLINK_TELEM2_BAUD
#define MAVLINK_TELEM2_BAUD          115200   /**< 波特率 */
#endif
#ifndef MAVLINK_TELEM2_TX_PIN
#define MAVLINK_TELEM2_TX_PIN        10      /**< TX 引? */
#endif
#ifndef MAVLINK_TELEM2_RX_PIN
#define MAVLINK_TELEM2_RX_PIN        11      /**< RX 引? */
#endif
/** @} */

/**
 * @name DEBUG 配置 (??串口)
 * @{
 */
#ifndef MAVLINK_DEBUG_ENABLED
#define MAVLINK_DEBUG_ENABLED        1       /**< 1=?用, 0=禁用 */
#endif
#ifndef MAVLINK_DEBUG_BAUD
#define MAVLINK_DEBUG_BAUD           115200  /**< 波特率 */
#endif
#ifndef MAVLINK_DEBUG_TX_PIN
#define MAVLINK_DEBUG_TX_PIN         22       /**< TX 引? */
#endif
#ifndef MAVLINK_DEBUG_RX_PIN
#define MAVLINK_DEBUG_RX_PIN         23       /**< RX 引? */
#endif
/** @} */

/* ==================== 网?配置 ==================== */

#ifndef MAVLINK_UDP_PORT
#define MAVLINK_UDP_PORT             8888   /**< UDP ?听端口 */
#endif

#ifndef MAVLINK_TCP_PORT
#define MAVLINK_TCP_PORT             8889   /**< TCP ?听端口，0=禁用 */
#endif

#ifndef MAVLINK_MAX_TCP_CLIENTS
#define MAVLINK_MAX_TCP_CLIENTS      5       /**< 最大 TCP 客?端? */
#endif

/* ==================== ?列配置 ==================== */

#ifndef MAVLINK_QUEUE_SIZE
#define MAVLINK_QUEUE_SIZE           32      /**< 有序?列大小 */
#endif

#ifndef MAVLINK_SEQ_WINDOW
#define MAVLINK_SEQ_WINDOW           10       /**< 序列?容?窗口 */
#endif

/* ==================== MAVLink ??配置 ==================== */

#define MAVLINK_STX_V1               0xFE    /**< MAVLink v1 ?起始 */
#define MAVLINK_HEADER_LEN           6       /**< ???度 */
#define MAVLINK_FOOTER_LEN           2       /**< ?尾?度 */
#define MAVLINK_MIN_FRAME_LEN        (MAVLINK_HEADER_LEN + MAVLINK_FOOTER_LEN)
#define MAVLINK_MAX_PAYLOAD_LEN      255     /**< 最大?? */
#define MAVLINK_MAX_FRAME_LEN        (MAVLINK_MAX_PAYLOAD_LEN + MAVLINK_MIN_FRAME_LEN)

/* ==================== ??配置 ==================== */

#ifndef MAVLINK_DEBUG_LOG
#define MAVLINK_DEBUG_LOG            1       /**< ??日志?? */
#endif

#ifndef MAVLINK_LOG_HEXDUMP
#define MAVLINK_LOG_HEXDUMP          0       /**< HEX dump 日志?? */
#endif

/* ==================== ?部常量 ==================== */

#define MAVLINK_MAX_UARTS            3       /**< 最大 UART ?量 */

/* ============================================================================
 *                              用?配置??束
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

/* ==================== 枚?定? ==================== */

/**
 * @brief UART 端口 ID 枚?
 *
 * 固定分配的串口用途，ID 即索引。
 */
typedef enum {
    UART_ID_TELEM1 = 1,    /**< TELEM1: ?控主端口 (UART1, GPIO20/21) */
    UART_ID_TELEM2 = 2,    /**< TELEM2: ?控?端口 (UART2, GPIO10/11) */
    UART_ID_DEBUG = 3,      /**< DEBUG:  ??串口   (UART3, GPIO22/23) */
    UART_ID_MAX
} mavlink_uart_id_t;

/**
 * @brief ?接?行??
 */
typedef enum {
    MAVLINK_STATE_IDLE = 0,           /**< 空? */
    MAVLINK_STATE_INITIALIZED,          /**< 已初始化 */
    MAVLINK_STATE_RUNNING,              /**< ?行中 */
    MAVLINK_STATE_ERROR                 /**< ???? */
} mavlink_state_t;

/* ==================== ?型定? ==================== */

/**
 * @brief MAVLink ?接句柄（不透明?型）
 */
typedef struct mavlink_bridge_t *mavlink_bridge_handle_t;

/**
 * @brief 网??据回?函?
 *
 * @param uart_id  目? UART ID (??到?串口)
 * @param data     ?据???
 * @param len      ?据?度
 * @param ctx      用?上下文
 */
typedef void (*net_data_callback_t)(uint8_t uart_id, const uint8_t *data, size_t len, void *ctx);

/**
 * @brief UART ?据回?函?
 *
 * @param uart_id  ?源 UART ID
 * @param data     ?据???
 * @param len      ?据?度
 * @param ctx      用?上下文
 */
typedef void (*uart_data_callback_t)(uint8_t uart_id, const uint8_t *data, size_t len, void *ctx);

/**
 * @brief UART 配置?构
 */
typedef struct {
    mavlink_uart_id_t id;          /**< UART ID */
    int baud_rate;                 /**< 波特率 */
    uint8_t tx_pin;                /**< TX 引? */
    uint8_t rx_pin;                /**< RX 引? */
    bool enabled;                  /**< 是否?用 */
    const char *name;              /**< 名??? */

    /* ?据回? */
    uart_data_callback_t data_callback;  /**< ?据接收回? */
    void *callback_ctx;                  /**< 回?上下文 */

    /* MAVLink 配置 */
    bool mavlink_mode;             /**< MAVLink 模式 */
    uint8_t sysid_filter;          /**< 系? ID ?? (0=不??) */
} mavlink_uart_config_t;

/**
 * @brief MAVLink ?接配置?构
 */
typedef struct {
    uint16_t udp_port;             /**< UDP ?听端口 */
    uint16_t tcp_port;              /**< TCP ?听端口，0=禁用 */
    uint8_t queue_size;             /**< 有序?列大小 */
    bool ordered_mode;              /**< 有序模式 (true=保??序) */
} mavlink_bridge_config_t;

/**
 * @brief ?接??信息
 */
typedef struct {
    uint32_t udp_rx_count;         /**< UDP 接收?? */
    uint32_t tcp_rx_count;          /**< TCP 接收?? */
    uint32_t udp_drop_count;        /**< UDP ???? */
    uint32_t uart_tx_bytes[UART_ID_MAX];   /**< 各 UART ?送字?? */
    uint32_t uart_rx_bytes[UART_ID_MAX];   /**< 各 UART 接收字?? */
    uint32_t error_count;           /**< ???? */
} mavlink_stats_t;

/* ==================== 宏定? ==================== */

/**
 * @brief ?取默? UART 配置
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
 * @brief ?取默??接配置
 */
#define MAVLINK_BRIDGE_CONFIG_DEFAULT() { \
    .udp_port = MAVLINK_UDP_PORT, \
    .tcp_port = MAVLINK_TCP_PORT, \
    .queue_size = MAVLINK_QUEUE_SIZE, \
    .ordered_mode = false, \
}

/* ==================== 核心 API ==================== */

/**
 * @brief 初始化 MAVLink ?接
 *
 * @param config  ?接配置
 * @param handle  ?出：?接句柄
 * @return ESP_OK 成功
 */
esp_err_t mavlink_bridge_init(const mavlink_bridge_config_t *config,
                               mavlink_bridge_handle_t *handle);

/**
 * @brief ?? MAVLink ?接
 *
 * @param handle ?接句柄
 * @return ESP_OK 成功
 */
esp_err_t mavlink_bridge_deinit(mavlink_bridge_handle_t handle);

/**
 * @brief ?查?接是否已初始化
 */
bool mavlink_bridge_is_initialized(mavlink_bridge_handle_t handle);

/**
 * @brief ?取?接??
 */
mavlink_state_t mavlink_bridge_get_state(mavlink_bridge_handle_t handle);

/* ==================== 网? -> UART ?? API ==================== */

/**
 * @brief ?送?据到所有?用的 UART (网???串口)
 *
 * @param handle ?接句柄
 * @param data   ?据???
 * @param len    ?据?度
 * @return ESP_OK 成功
 */
esp_err_t mavlink_bridge_net_to_all_uarts(mavlink_bridge_handle_t handle,
                                           const uint8_t *data, size_t len);

/**
 * @brief ?送?据到指定 UART (网???到??串口)
 *
 * @param handle  ?接句柄
 * @param uart_id UART ID (UART_ID_TELEM1, etc)
 * @param data    ?据???
 * @param len     ?据?度
 * @return ESP_OK 成功
 */
esp_err_t mavlink_bridge_net_to_uart(mavlink_bridge_handle_t handle,
                                      uint8_t uart_id,
                                      const uint8_t *data, size_t len);

/* ==================== UART -> 网? ?? API ==================== */

/**
 * @brief ?送 UART ?据到所有网??接 (串口???到网?)
 *
 * @param handle  ?接句柄
 * @param uart_id ?源 UART ID
 * @param data    ?据???
 * @param len     ?据?度
 * @return ESP_OK 成功
 */
esp_err_t mavlink_bridge_uart_to_net(mavlink_bridge_handle_t handle,
                                       uint8_t uart_id,
                                       const uint8_t *data, size_t len);

/**
 * @brief ?置网??播模式
 *
 * @param handle ?接句柄
 * @param enable true=?用?播
 */
void mavlink_bridge_set_net_broadcast(mavlink_bridge_handle_t handle, bool enable);

/* ==================== UART 操作 API ==================== */

/**
 * @brief ?送?据到指定 UART
 *
 * @param handle  ?接句柄
 * @param uart_id UART ID
 * @param data    ?据???
 * @param len     ?据?度
 * @return ESP_OK 成功
 */
esp_err_t mavlink_bridge_send_to_uart(mavlink_bridge_handle_t handle,
                                       uint8_t uart_id,
                                       const uint8_t *data, size_t len);

/**
 * @brief 修改 UART 波特率
 *
 * @param handle    ?接句柄
 * @param uart_id   UART ID
 * @param baud_rate 新波特率
 * @return ESP_OK 成功
 */
esp_err_t mavlink_bridge_set_baudrate(mavlink_bridge_handle_t handle,
                                        uint8_t uart_id,
                                        int baud_rate);

/**
 * @brief ?取 UART ?前波特率
 */
int mavlink_bridge_get_baudrate(mavlink_bridge_handle_t handle, uint8_t uart_id);

/**
 * @brief ?用/禁用 UART
 */
esp_err_t mavlink_bridge_enable_uart(mavlink_bridge_handle_t handle,
                                      uint8_t uart_id, bool enable);

/* ==================== 回?注? API ==================== */

/**
 * @brief 注?网??据到?回?
 *
 * ? UDP/TCP 收到?据??用。
 *
 * @param handle   ?接句柄
 * @param callback 回?函?
 * @param ctx      用?上下文
 */
void mavlink_bridge_on_net_data(mavlink_bridge_handle_t handle,
                                 net_data_callback_t callback, void *ctx);

/**
 * @brief 注?指定 UART 的?据回?
 *
 * @param handle   ?接句柄
 * @param uart_id  UART ID
 * @param callback 回?函?
 * @param ctx      用?上下文
 */
void mavlink_bridge_on_uart_data(mavlink_bridge_handle_t handle,
                                  uint8_t uart_id,
                                  uart_data_callback_t callback, void *ctx);

/* ==================== ?? API ==================== */

/**
 * @brief ?取?接??
 */
void mavlink_bridge_get_stats(mavlink_bridge_handle_t handle,
                               mavlink_stats_t *stats);

/**
 * @brief 重置??
 */
void mavlink_bridge_reset_stats(mavlink_bridge_handle_t handle);

/**
 * @brief ?取本地 IP
 */
esp_err_t mavlink_bridge_get_local_ip(mavlink_bridge_handle_t handle,
                                       char *ip_str, size_t len);

/* ==================== ?助函? ==================== */

/**
 * @brief ?查?据是否? MAVLink ?
 */
bool mavlink_is_frame(const uint8_t *data, size_t len);

/**
 * @brief ?取 MAVLink 序列?
 */
uint8_t mavlink_get_sequence(const uint8_t *data);

/**
 * @brief ?取 MAVLink 消息 ID
 */
uint8_t mavlink_get_msg_id(const uint8_t *data);

/**
 * @brief ?取 MAVLink ???度
 */
uint8_t mavlink_get_payload_len(const uint8_t *data);

/**
 * @brief ?? MAVLink CRC
 */
bool mavlink_verify_crc(const uint8_t *data, size_t len);

/**
 * @brief ?取 UART 名?（??版本，根据枚?值映射）
 */
const char* mavlink_get_uart_name(uint8_t uart_id);

/**
 * @brief ?取已配置的 UART ?量
 */
uint8_t mavlink_bridge_get_count(mavlink_bridge_handle_t handle);

/**
 * @brief ?取 UART 名?（??版本，???配置中?取）
 * @note 建?优先使用此函?，?示更准确
 */
const char* mavlink_bridge_get_uart_name(mavlink_bridge_handle_t handle, uint8_t uart_id);

/**
 * @brief ?取全局?接句柄
 */
mavlink_bridge_handle_t mavlink_bridge_get_handle(void);

/** @} */ // end of mavlink_bridge group

#ifdef __cplusplus
}
#endif

#endif /* MAVLINK_BRIDGE_H */