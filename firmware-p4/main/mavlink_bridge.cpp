/**
 * @file mavlink_bridge.cpp
 * @brief MAVLink 多串口双向桥接模块实现
 */

#include "mavlink_bridge.h"
#include "wifi_app.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include <string.h>
#include <memory>
#include <optional>
#include <vector>
#include <list>

/* FreeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

/* espp Socket */
#include "udp_socket.hpp"
#include "tcp_socket.hpp"

static const char *TAG = "mavlink_bridge";

/* ==================== 内部类型定义 ==================== */

typedef struct {
    uint32_t sequence;
    uint64_t timestamp;
    uint8_t data[MAVLINK_MAX_FRAME_LEN];
    size_t len;
    bool used;
} ordered_packet_t;

typedef struct {
    ordered_packet_t *packets;
    size_t size;
    uint32_t next_seq;
    SemaphoreHandle_t mutex;
} ordered_buffer_t;

typedef struct {
    int client_fd;
    std::shared_ptr<espp::TcpSocket> socket;
    char ip_str[16];
    uint16_t port;
    bool connected;
    TaskHandle_t task_handle;
} tcp_client_t;

typedef struct {
    mavlink_uart_id_t id;
    uart_port_t port_num;
    int baud_rate;
    uint8_t tx_pin;
    uint8_t rx_pin;
    bool enabled;
    const char *name;
    uart_data_callback_t callback;
    void *callback_ctx;
    bool mavlink_mode;
    uint8_t sysid_filter;
    TaskHandle_t rx_task;
    uint32_t tx_bytes;
    uint32_t rx_bytes;
} mavlink_uart_port_info_t;

struct mavlink_bridge_t {
    uint16_t udp_port;
    uint16_t tcp_port;
    uint8_t queue_size;
    bool ordered_mode;
    mavlink_state_t state;
    bool initialized;
    mavlink_uart_port_info_t uarts[MAVLINK_MAX_UARTS];
    uint8_t uart_count;
    ordered_buffer_t ordered_buf;
    std::unique_ptr<espp::UdpSocket> udp_socket;
    std::unique_ptr<espp::TcpSocket> tcp_server;
    std::list<tcp_client_t> tcp_clients;
    TaskHandle_t tcp_server_task;
    bool tcp_running;
    bool net_broadcast;
    net_data_callback_t net_callback;
    void *net_callback_ctx;
    SemaphoreHandle_t uart_mutex;
    SemaphoreHandle_t tcp_mutex;
    SemaphoreHandle_t net_mutex;
    mavlink_stats_t stats;
};

static mavlink_bridge_handle_t s_handle = NULL;

static const char* s_uart_names[UART_ID_MAX] = {
    "TELEM1",
    "TELEM2",
    "DEBUG"
};

/* ==================== 有序缓冲区实现 ==================== */

static void ordered_buffer_init(ordered_buffer_t *buf, size_t size)
{
    buf->packets = (ordered_packet_t *)calloc(size, sizeof(ordered_packet_t));
    buf->size = size;
    buf->next_seq = 0;
    buf->mutex = xSemaphoreCreateMutex();
}

static void ordered_buffer_deinit(ordered_buffer_t *buf)
{
    if (buf->packets) {
        free(buf->packets);
        buf->packets = NULL;
    }
    if (buf->mutex) {
        vSemaphoreDelete(buf->mutex);
        buf->mutex = NULL;
    }
}

static bool ordered_buffer_push(ordered_buffer_t *buf,
                                 const uint8_t *data, size_t len,
                                 uint32_t sequence)
{
    if (!buf || !buf->packets || !data) return false;
    if (xSemaphoreTake(buf->mutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;

    uint32_t expected = buf->next_seq;
    int32_t diff = (int32_t)(sequence - expected);

    if (diff < -((int32_t)MAVLINK_SEQ_WINDOW) || diff > (int32_t)MAVLINK_SEQ_WINDOW) {
        xSemaphoreGive(buf->mutex);
        return false;
    }

    size_t slot = sequence % buf->size;
    buf->packets[slot].sequence = sequence;
    buf->packets[slot].timestamp = esp_timer_get_time() / 1000;
    buf->packets[slot].len = (len > MAVLINK_MAX_FRAME_LEN) ? MAVLINK_MAX_FRAME_LEN : len;
    memcpy(buf->packets[slot].data, data, buf->packets[slot].len);
    buf->packets[slot].used = true;

    xSemaphoreGive(buf->mutex);
    return true;
}

static bool ordered_buffer_pop(ordered_buffer_t *buf,
                                uint8_t *data, size_t *len)
{
    if (!buf || !buf->packets || !data || !len) return false;
    if (xSemaphoreTake(buf->mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return false;
    }

    size_t slot = buf->next_seq % buf->size;

    if (!buf->packets[slot].used) {
        xSemaphoreGive(buf->mutex);
        return false;
    }

    *len = buf->packets[slot].len;
    memcpy(data, buf->packets[slot].data, *len);
    buf->packets[slot].used = false;
    buf->next_seq++;

    xSemaphoreGive(buf->mutex);
    return true;
}

/* ==================== MAVLink 辅助函数 ==================== */

bool mavlink_is_frame(const uint8_t *data, size_t len)
{
    if (!data || len < MAVLINK_MIN_FRAME_LEN) return false;
    return (data[0] == MAVLINK_STX_V1);
}

uint8_t mavlink_get_sequence(const uint8_t *data)
{
    if (!data || data[0] != MAVLINK_STX_V1) return 0;
    return data[2];
}

uint8_t mavlink_get_msg_id(const uint8_t *data)
{
    if (!data || data[0] != MAVLINK_STX_V1) return 0;
    return data[5];
}

uint8_t mavlink_get_payload_len(const uint8_t *data)
{
    if (!data || data[0] != MAVLINK_STX_V1) return 0;
    return data[1];
}

static uint16_t mavlink_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}

bool mavlink_verify_crc(const uint8_t *data, size_t len)
{
    if (!data || data[0] != MAVLINK_STX_V1 || len < MAVLINK_MIN_FRAME_LEN) return false;

    uint8_t payload_len = data[1];
    size_t frame_len = payload_len + MAVLINK_MIN_FRAME_LEN;
    if (len < frame_len) return false;

    uint8_t crc_buf[MAVLINK_MAX_PAYLOAD_LEN + 2];
    crc_buf[0] = data[5];
    memcpy(&crc_buf[1], &data[6], payload_len);

    uint16_t crc = mavlink_crc16(crc_buf, payload_len + 1);
    uint16_t frame_crc = data[frame_len - 1] | (data[frame_len - 2] << 8);

    return (crc == frame_crc);
}

const char* mavlink_get_uart_name(uint8_t uart_id)
{
    if (uart_id == UART_ID_TELEM1) return "TELEM1";
    if (uart_id == UART_ID_TELEM2) return "TELEM2";
    if (uart_id == UART_ID_DEBUG) return "DEBUG";
    return "UNKNOWN";
}

const char* mavlink_bridge_get_uart_name(mavlink_bridge_handle_t handle, uint8_t uart_id)
{
    if (!handle) return "N/A";
    if (uart_id >= handle->uart_count) return "INVALID";
    return handle->uarts[uart_id].name ? handle->uarts[uart_id].name : "UNKNOWN";
}

/* ==================== UART 实现 ==================== */

static esp_err_t init_uart_port(mavlink_bridge_handle_t handle,
                                  const mavlink_uart_config_t *config,
                                  mavlink_uart_port_info_t *port_info)
{
    if (!handle || !config || !port_info) return ESP_ERR_INVALID_ARG;

    port_info->id = config->id;
    port_info->port_num = (uart_port_t)config->id;
    port_info->baud_rate = config->baud_rate;
    port_info->tx_pin = config->tx_pin;
    port_info->rx_pin = config->rx_pin;
    port_info->enabled = config->enabled;
    port_info->name = config->name;
    port_info->callback = config->data_callback;
    port_info->callback_ctx = config->callback_ctx;
    port_info->mavlink_mode = config->mavlink_mode;
    port_info->sysid_filter = config->sysid_filter;
    port_info->tx_bytes = 0;
    port_info->rx_bytes = 0;
    port_info->rx_task = NULL;

    uart_config_t uart_cfg = {};
    uart_cfg.baud_rate = config->baud_rate;
    uart_cfg.data_bits = UART_DATA_8_BITS;
    uart_cfg.parity = UART_PARITY_DISABLE;
    uart_cfg.stop_bits = UART_STOP_BITS_1;
    uart_cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    uart_cfg.source_clk = UART_SCLK_DEFAULT;
#endif

    esp_err_t ret = uart_param_config(port_info->port_num, &uart_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART%d", port_info->id);
        return ret;
    }

    if (config->tx_pin != UART_PIN_NO_CHANGE && config->rx_pin != UART_PIN_NO_CHANGE) {
        ret = uart_set_pin(port_info->port_num, config->tx_pin, config->rx_pin,
                            UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set UART%d pins", port_info->id);
            return ret;
        }
    }

    ret = uart_driver_install(port_info->port_num, 4096, 4096, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART%d driver", port_info->id);
        return ret;
    }

    ESP_LOGI(TAG, "UART%d (%s) init: baud=%d, tx=%d, rx=%d",
             port_info->id, port_info->name, port_info->baud_rate,
             port_info->tx_pin, port_info->rx_pin);

    return ESP_OK;
}

static void uart_rx_task(void *arg)
{
    mavlink_uart_port_info_t *port_info = (mavlink_uart_port_info_t *)arg;
    uint8_t buffer[MAVLINK_MAX_FRAME_LEN];

    ESP_LOGI(TAG, "UART%d Rx task started", port_info->id);

    while (true) {
        int len = uart_read_bytes(port_info->port_num, buffer, sizeof(buffer),
                                   pdMS_TO_TICKS(10));

        if (len > 0) {
            port_info->rx_bytes += len;
            if (s_handle) {
                s_handle->stats.uart_rx_bytes[port_info->id] += len;
            }

            if (MAVLINK_DEBUG_LOG) {
                ESP_LOGD(TAG, "UART%d Rx: %d bytes", port_info->id, len);
            }

            if (port_info->callback) {
                port_info->callback(port_info->id, buffer, len, port_info->callback_ctx);
            }

            if (s_handle && s_handle->net_broadcast) {
                mavlink_bridge_uart_to_net(s_handle, port_info->id, buffer, len);
            }
        }

        taskYIELD();
    }
}

static esp_err_t start_uart_rx_task(mavlink_bridge_handle_t handle,
                                      mavlink_uart_port_info_t *port_info)
{
    char task_name[16];
    snprintf(task_name, sizeof(task_name), "Uart%dRx", port_info->id);

    BaseType_t res = xTaskCreatePinnedToCore(
                         uart_rx_task, task_name, 4096, port_info, 10,
                         &port_info->rx_task, 0);

    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UART%d Rx task", port_info->id);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static esp_err_t send_to_uart_internal(mavlink_bridge_handle_t handle,
                                       mavlink_uart_port_info_t *port_info,
                                       const uint8_t *data, size_t len)
{
    if (!port_info->enabled) return ESP_ERR_INVALID_STATE;

    // 等待 UART TX 空闲，确保前一个包已发完再发下一个，避免丢数据
    if (uart_wait_tx_done(port_info->port_num, pdMS_TO_TICKS(100)) != ESP_OK) {
        handle->stats.error_count++;
        return ESP_ERR_TIMEOUT;
    }

    int written = uart_write_bytes(port_info->port_num, (const char *)data, len);
    if (written < 0) {
        handle->stats.error_count++;
        return ESP_FAIL;
    }

    port_info->tx_bytes += written;
    handle->stats.uart_tx_bytes[port_info->id] += written;

    return ESP_OK;
}

/* ==================== 网络实现 ==================== */

static std::optional<std::vector<uint8_t>> udp_receive_callback(
    std::vector<uint8_t> &data,
    const espp::Socket::Info &info)
{
    if (data.empty() || !s_handle) return std::nullopt;

    uint32_t seq = 0;
    if (data[0] == MAVLINK_STX_V1 && data.size() >= 4) {
        seq = data[2];
    } else {
        seq = (uint8_t)(esp_timer_get_time() / 1000);
    }

    if (s_handle->ordered_mode) {
        if (!ordered_buffer_push(&s_handle->ordered_buf, data.data(), data.size(), seq)) {
            s_handle->stats.udp_drop_count++;
            ESP_LOGW(TAG, "UDP packet dropped: seq=%u", seq);
            return std::nullopt;
        }
    } else {
        mavlink_bridge_net_to_all_uarts(s_handle, data.data(), data.size());
    }

    s_handle->stats.udp_rx_count++;

    if (s_handle->net_callback) {
        s_handle->net_callback(UART_ID_MAX, data.data(), data.size(), s_handle->net_callback_ctx);
    }

    return std::nullopt;
}

static void ordered_send_task(void *arg)
{
    mavlink_bridge_handle_t handle = (mavlink_bridge_handle_t)arg;
    uint8_t buffer[MAVLINK_MAX_FRAME_LEN];
    size_t len;

    ESP_LOGI(TAG, "Ordered send task started");

    while (handle->state == MAVLINK_STATE_RUNNING) {
        if (ordered_buffer_pop(&handle->ordered_buf, buffer, &len)) {
            if (MAVLINK_DEBUG_LOG) {
                ESP_LOGD(TAG, "Ordered send: len=%zu", len);
            }

            if (xSemaphoreTake(handle->uart_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                for (int i = 0; i < handle->uart_count; i++) {
                    if (handle->uarts[i].enabled) {
                        send_to_uart_internal(handle, &handle->uarts[i], buffer, len);
                    }
                }
                xSemaphoreGive(handle->uart_mutex);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    vTaskDelete(NULL);
}

static void tcp_client_task(void *arg);

static void tcp_server_task(void *arg)
{
    mavlink_bridge_handle_t handle = (mavlink_bridge_handle_t)arg;

    ESP_LOGI(TAG, "TCP server task started");

    while (handle->tcp_running) {
        auto client_socket = handle->tcp_server->accept();

        if (client_socket) {
            if (xSemaphoreTake(handle->tcp_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                int current = 0;
                for (auto &c : handle->tcp_clients) {
                    if (c.connected) current++;
                }

                if (current >= MAVLINK_MAX_TCP_CLIENTS) {
                    ESP_LOGW(TAG, "Max TCP clients reached");
                } else {
                    auto &remote = client_socket->get_remote_info();

                    tcp_client_t client = {};
                    strncpy(client.ip_str, remote.address.c_str(), sizeof(client.ip_str) - 1);
                    client.port = remote.port;
                    client.connected = true;

                    handle->tcp_clients.push_back(client);
                    tcp_client_t *p_client = &handle->tcp_clients.back();

                    ESP_LOGI(TAG, "TCP client connected: %s:%d", client.ip_str, client.port);

                    xTaskCreatePinnedToCore(tcp_client_task, "TcpClient", 4096,
                                             p_client, 10, &p_client->task_handle, 0);
                }

                xSemaphoreGive(handle->tcp_mutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    vTaskDelete(NULL);
}

static void tcp_client_task(void *arg)
{
    tcp_client_t *client = (tcp_client_t *)arg;

    if (!client || !client->socket) {
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "TCP client task: %s:%d", client->ip_str, client->port);

    uint8_t buffer[MAVLINK_MAX_FRAME_LEN];

    while (client->connected && s_handle && s_handle->state == MAVLINK_STATE_RUNNING) {
        if (!client->socket->is_connected()) {
            ESP_LOGI(TAG, "TCP client disconnected: %s:%d", client->ip_str, client->port);
            client->connected = false;
            break;
        }

        std::vector<uint8_t> data;
        if (client->socket->receive(data, sizeof(buffer))) {
            if (!data.empty()) {
                s_handle->stats.tcp_rx_count++;

                uint8_t seq = data[0] == MAVLINK_STX_V1 ? data[2] :
                              (uint8_t)(esp_timer_get_time() / 1000);

                if (s_handle->ordered_mode) {
                    if (!ordered_buffer_push(&s_handle->ordered_buf, data.data(), data.size(), seq)) {
                        s_handle->stats.udp_drop_count++;
                    }
                } else {
                    mavlink_bridge_net_to_all_uarts(s_handle, data.data(), data.size());
                }

                if (s_handle->net_callback) {
                    s_handle->net_callback(UART_ID_MAX, data.data(), data.size(), s_handle->net_callback_ctx);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    client->task_handle = NULL;
    vTaskDelete(NULL);
}

static esp_err_t tcp_broadcast(mavlink_bridge_handle_t handle,
                               const uint8_t *data, size_t len)
{
    if (xSemaphoreTake(handle->tcp_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    std::vector<uint8_t> vec(data, data + len);
    for (auto &client : handle->tcp_clients) {
        if (client.connected && client.socket) {
            client.socket->transmit(vec);
        }
    }

    xSemaphoreGive(handle->tcp_mutex);
    return ESP_OK;
}

static void cleanup_handle(mavlink_bridge_handle_t handle)
{
    if (!handle) return;

    handle->tcp_running = false;
    if (handle->tcp_server_task) {
        vTaskDelete(handle->tcp_server_task);
        handle->tcp_server_task = NULL;
    }

    if (handle->tcp_mutex) {
        xSemaphoreTake(handle->tcp_mutex, pdMS_TO_TICKS(100));
        for (auto &c : handle->tcp_clients) {
            c.connected = false;
            c.socket.reset();
            if (c.task_handle) vTaskDelete(c.task_handle);
        }
        handle->tcp_clients.clear();
        xSemaphoreGive(handle->tcp_mutex);
        vSemaphoreDelete(handle->tcp_mutex);
        handle->tcp_mutex = NULL;
    }

    handle->tcp_server.reset();
    handle->udp_socket.reset();

    ordered_buffer_deinit(&handle->ordered_buf);

    if (handle->uart_mutex) {
        vSemaphoreDelete(handle->uart_mutex);
        handle->uart_mutex = NULL;
    }
    if (handle->net_mutex) {
        vSemaphoreDelete(handle->net_mutex);
        handle->net_mutex = NULL;
    }

    for (int i = 0; i < handle->uart_count; i++) {
        if (handle->uarts[i].rx_task) {
            vTaskDelete(handle->uarts[i].rx_task);
            handle->uarts[i].rx_task = NULL;
        }
        uart_driver_delete(handle->uarts[i].port_num);
    }

    s_handle = NULL;
    free(handle);
}

/* ==================== 公共 API 实现 ==================== */

esp_err_t mavlink_bridge_init(const mavlink_bridge_config_t *config,
                               mavlink_bridge_handle_t *handle)
{
    if (!config || !handle) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    mavlink_bridge_handle_t h = (mavlink_bridge_handle_t)calloc(1, sizeof(struct mavlink_bridge_t));
    if (!h) {
        ESP_LOGE(TAG, "Failed to allocate");
        return ESP_ERR_NO_MEM;
    }

    /* 保存配置 */
    h->udp_port = config->udp_port > 0 ? config->udp_port : MAVLINK_UDP_PORT;
    h->tcp_port = config->tcp_port;
    h->queue_size = config->queue_size > 0 ? config->queue_size : MAVLINK_QUEUE_SIZE;
    h->ordered_mode = config->ordered_mode;
    h->uart_count = 0;
    h->state = MAVLINK_STATE_IDLE;
    h->initialized = false;
    h->net_broadcast = true;

    /* 创建同步对象 */
    h->uart_mutex = xSemaphoreCreateMutex();
    h->tcp_mutex = xSemaphoreCreateMutex();
    h->net_mutex = xSemaphoreCreateMutex();

    if (!h->uart_mutex || !h->tcp_mutex || !h->net_mutex) {
        ESP_LOGE(TAG, "Failed to create mutexes");
        cleanup_handle(h);
        return ESP_ERR_NO_MEM;
    }

    /* 初始化有序缓冲区 */
    ordered_buffer_init(&h->ordered_buf, h->queue_size);

    /* 使用宏定义配置三个 UART */
    mavlink_uart_config_t uart_configs[MAVLINK_MAX_UARTS];
    int config_count = 0;

#if MAVLINK_TELEM1_ENABLED
    uart_configs[config_count++] = (mavlink_uart_config_t){
        .id = UART_ID_TELEM1,
        .baud_rate = MAVLINK_TELEM1_BAUD,
        .tx_pin = MAVLINK_TELEM1_TX_PIN,
        .rx_pin = MAVLINK_TELEM1_RX_PIN,
        .enabled = false,
        .name = "TELEM1",
        .data_callback = NULL, .callback_ctx = NULL,
        .mavlink_mode = true, .sysid_filter = 0,
    };
#endif

#if MAVLINK_TELEM2_ENABLED
    uart_configs[config_count++] = (mavlink_uart_config_t){
        .id = UART_ID_TELEM2,
        .baud_rate = MAVLINK_TELEM2_BAUD,
        .tx_pin = MAVLINK_TELEM2_TX_PIN,
        .rx_pin = MAVLINK_TELEM2_RX_PIN,
        .enabled = false,
        .name = "TELEM2",
        .data_callback = NULL, .callback_ctx = NULL,
        .mavlink_mode = true, .sysid_filter = 0,
    };
#endif

#if MAVLINK_DEBUG_ENABLED
    uart_configs[config_count++] = (mavlink_uart_config_t){
        .id = UART_ID_DEBUG,
        .baud_rate = MAVLINK_DEBUG_BAUD,
        .tx_pin = MAVLINK_DEBUG_TX_PIN,
        .rx_pin = MAVLINK_DEBUG_RX_PIN,
        .enabled = false,
        .name = "DEBUG",
        .data_callback = NULL, .callback_ctx = NULL,
        .mavlink_mode = true, .sysid_filter = 0,
    };
#endif

    /* 初始化 UART */
    for (int i = 0; i < config_count; i++) {
        esp_err_t err = init_uart_port(h, &uart_configs[i], &h->uarts[h->uart_count]);
        if (err == ESP_OK) {
            h->uart_count++;
        } else {
            ESP_LOGW(TAG, "UART%d init failed, skipping", i);
        }
    }

    /* 启动 UDP 服务器 */
    h->udp_socket = std::make_unique<espp::UdpSocket>(
        espp::UdpSocket::Config{ .log_level = espp::Logger::Verbosity::WARN });

    auto task_config = espp::Task::BaseConfig{
        .name = "MavlinkUdp",
        .stack_size_bytes = 4096,
    };

    auto recv_config = espp::UdpSocket::ReceiveConfig{
        .port = h->udp_port,
        .buffer_size = MAVLINK_MAX_FRAME_LEN,
        .on_receive_callback = udp_receive_callback,
    };

    if (!h->udp_socket->start_receiving(task_config, recv_config)) {
        ESP_LOGE(TAG, "Failed to start UDP server on port %d", h->udp_port);
        cleanup_handle(h);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "UDP server started on port %d", h->udp_port);

    /* 启动 TCP 服务器 */
    if (h->tcp_port > 0) {
        h->tcp_server = std::make_unique<espp::TcpSocket>(
            espp::TcpSocket::Config{ .log_level = espp::Logger::Verbosity::WARN });

        if (h->tcp_server->bind(h->tcp_port) && h->tcp_server->listen(5)) {
            h->tcp_running = true;
            xTaskCreatePinnedToCore(tcp_server_task, "MavlinkTcp", 4096,
                                    h, 10, &h->tcp_server_task, 0);
            ESP_LOGI(TAG, "TCP server started on port %d", h->tcp_port);
        } else {
            ESP_LOGW(TAG, "TCP server failed to start on port %d", h->tcp_port);
        }
    }

    /* 启动有序发送任务 */
    if (h->ordered_mode) {
        h->state = MAVLINK_STATE_RUNNING;
        xTaskCreatePinnedToCore(ordered_send_task, "MavlinkSend", 4096,
                                h, 10, NULL, 0);
    }

    /* 启动 UART 接收任务 */
    for (int i = 0; i < h->uart_count; i++) {
        start_uart_rx_task(h, &h->uarts[i]);
    }

    h->initialized = true;
    h->state = MAVLINK_STATE_RUNNING;
    s_handle = h;
    *handle = h;

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "MAVLink Bridge initialized");
    ESP_LOGI(TAG, "  UDP: :%d, TCP: :%d", h->udp_port, h->tcp_port);
    ESP_LOGI(TAG, "  UARTs: %d", h->uart_count);
    for (int i = 0; i < h->uart_count; i++) {
        ESP_LOGI(TAG, "    [%s] baud=%d, tx=%d, rx=%d",
                 h->uarts[i].name, h->uarts[i].baud_rate,
                 h->uarts[i].tx_pin, h->uarts[i].rx_pin);
    }
    ESP_LOGI(TAG, "========================================");

    return ESP_OK;
}

esp_err_t mavlink_bridge_deinit(mavlink_bridge_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!handle->initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Deinitializing MAVLink Bridge...");
    handle->state = MAVLINK_STATE_IDLE;
    handle->initialized = false;

    cleanup_handle(handle);

    ESP_LOGI(TAG, "MAVLink Bridge deinitialized");
    return ESP_OK;
}

bool mavlink_bridge_is_initialized(mavlink_bridge_handle_t handle)
{
    return (handle && handle->initialized);
}

mavlink_state_t mavlink_bridge_get_state(mavlink_bridge_handle_t handle)
{
    return handle ? handle->state : MAVLINK_STATE_IDLE;
}

esp_err_t mavlink_bridge_net_to_all_uarts(mavlink_bridge_handle_t handle,
                                           const uint8_t *data, size_t len)
{
    if (!handle || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(handle->uart_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    for (int i = 0; i < handle->uart_count; i++) {
        if (handle->uarts[i].enabled) {
            send_to_uart_internal(handle, &handle->uarts[i], data, len);
        }
    }

    xSemaphoreGive(handle->uart_mutex);
    return ESP_OK;
}

esp_err_t mavlink_bridge_net_to_uart(mavlink_bridge_handle_t handle,
                                      uint8_t uart_id,
                                      const uint8_t *data, size_t len)
{
    if (!handle || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    if (uart_id >= handle->uart_count) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(handle->uart_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;
    if (handle->uarts[uart_id].enabled) {
        ret = send_to_uart_internal(handle, &handle->uarts[uart_id], data, len);
    } else {
        ret = ESP_ERR_INVALID_STATE;
    }

    xSemaphoreGive(handle->uart_mutex);
    return ret;
}

esp_err_t mavlink_bridge_uart_to_net(mavlink_bridge_handle_t handle,
                                       uint8_t uart_id,
                                       const uint8_t *data, size_t len)
{
    if (!handle || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    /* TODO: 实现 UDP 广播到已连接的客户端 */
    (void)uart_id;
    (void)tcp_broadcast;
    return ESP_OK;
}

void mavlink_bridge_set_net_broadcast(mavlink_bridge_handle_t handle, bool enable)
{
    if (handle) {
        handle->net_broadcast = enable;
        ESP_LOGI(TAG, "Net broadcast: %s", enable ? "enabled" : "disabled");
    }
}

esp_err_t mavlink_bridge_send_to_uart(mavlink_bridge_handle_t handle,
                                       uint8_t uart_id,
                                       const uint8_t *data, size_t len)
{
    return mavlink_bridge_net_to_uart(handle, uart_id, data, len);
}

esp_err_t mavlink_bridge_set_baudrate(mavlink_bridge_handle_t handle,
                                        uint8_t uart_id,
                                        int baud_rate)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (uart_id >= handle->uart_count) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(handle->uart_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = uart_set_baudrate(handle->uarts[uart_id].port_num, baud_rate);
    if (ret == ESP_OK) {
        handle->uarts[uart_id].baud_rate = baud_rate;
        ESP_LOGI(TAG, "UART%d baudrate changed to %d", uart_id, baud_rate);
    }

    xSemaphoreGive(handle->uart_mutex);
    return ret;
}

uint8_t mavlink_bridge_get_count(mavlink_bridge_handle_t handle)
{
    if (!handle) return 0;
    return handle->uart_count;
}

int mavlink_bridge_get_baudrate(mavlink_bridge_handle_t handle, uint8_t uart_id)
{
    if (!handle || uart_id >= handle->uart_count) return 0;
    return handle->uarts[uart_id].baud_rate;
}

esp_err_t mavlink_bridge_enable_uart(mavlink_bridge_handle_t handle,
                                      uint8_t uart_id, bool enable)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (uart_id >= handle->uart_count) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->uarts[uart_id].enabled = enable;
    ESP_LOGI(TAG, "UART%d (%s): %s", uart_id, handle->uarts[uart_id].name,
             enable ? "enabled" : "disabled");

    return ESP_OK;
}

void mavlink_bridge_on_net_data(mavlink_bridge_handle_t handle,
                                 net_data_callback_t callback, void *ctx)
{
    if (handle) {
        handle->net_callback = callback;
        handle->net_callback_ctx = ctx;
        ESP_LOGI(TAG, "Net data callback registered");
    }
}

void mavlink_bridge_on_uart_data(mavlink_bridge_handle_t handle,
                                  uint8_t uart_id,
                                  uart_data_callback_t callback, void *ctx)
{
    if (handle && uart_id < handle->uart_count) {
        handle->uarts[uart_id].callback = callback;
        handle->uarts[uart_id].callback_ctx = ctx;
        ESP_LOGI(TAG, "UART%d data callback registered", uart_id);
    }
}

void mavlink_bridge_get_stats(mavlink_bridge_handle_t handle, mavlink_stats_t *stats)
{
    if (handle && stats) {
        memcpy(stats, &handle->stats, sizeof(mavlink_stats_t));
    }
}

void mavlink_bridge_reset_stats(mavlink_bridge_handle_t handle)
{
    if (handle) {
        memset(&handle->stats, 0, sizeof(handle->stats));
        for (int i = 0; i < handle->uart_count; i++) {
            handle->uarts[i].tx_bytes = 0;
            handle->uarts[i].rx_bytes = 0;
        }
    }
}

esp_err_t mavlink_bridge_get_local_ip(mavlink_bridge_handle_t handle,
                                       char *ip_str, size_t len)
{
    if (!handle || !ip_str) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_netif_ip_info_t ip_info;
    esp_err_t ret = wifi_app_get_ip_info(&ip_info);
    if (ret == ESP_OK) {
        esp_ip4addr_ntoa(&ip_info.ip, ip_str, len);
    }
    return ret;
}

mavlink_bridge_handle_t mavlink_bridge_get_handle(void)
{
    return s_handle;
}
