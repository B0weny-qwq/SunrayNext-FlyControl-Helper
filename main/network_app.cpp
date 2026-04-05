/**
 * @file network_app.cpp
 * @brief P4 WiFi Remote 网络通信模块实现
 *
 * @details
 * 本模块封装 espp::UdpSocket 和 espp::TcpSocket，提供跨平台的
 * C++ 接口供应用程序使用。
 *
 * @author P4 Team
 * @date 2026-04-04
 */

#include "network_app.h"
#include "wifi_app.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include <string.h>
#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <functional>

// FreeRTOS 头文件
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// espp Socket 头文件
#include "udp_socket.hpp"
#include "tcp_socket.hpp"

static const char *TAG = "network_app";

// 全局配置
static network_config_t s_config = {
    .type = NETWORK_TYPE_NONE,
    .mode = NETWORK_MODE_NONE,
    .target_ip = {0},
    .target_port = 0,
    .local_port = 5000,
    .is_running = false,
    .recv_count = 0,
    .send_count = 0,
};

// C++ Socket 对象（智能指针管理）
static std::unique_ptr<espp::UdpSocket> s_udp_socket;
static std::unique_ptr<espp::TcpSocket> s_tcp_socket;
static std::unique_ptr<espp::TcpSocket> s_tcp_client;

// 接收回调
static network_recv_callback_t s_recv_callback = nullptr;
static void *s_recv_ctx = nullptr;

// 互斥锁保护配置
static SemaphoreHandle_t s_config_mutex = nullptr;

// 任务句柄
static TaskHandle_t s_server_task_handle = nullptr;

/**
 * @brief 检查 WiFi 是否已连接
 */
static inline bool is_wifi_connected(void)
{
    return wifi_app_is_connected();
}

/**
 * @brief 初始化互斥锁
 */
static esp_err_t init_mutex(void)
{
    if (s_config_mutex == nullptr) {
        s_config_mutex = xSemaphoreCreateMutex();
        if (s_config_mutex == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

/**
 * @brief UDP 接收回调函数
 */
static std::optional<std::vector<uint8_t>> udp_receive_callback(
    std::vector<uint8_t> &data,
    const espp::Socket::Info &sender_info)
{
    s_config.recv_count++;

    // 打印接收到的数据
    ESP_LOGI(TAG, "UDP 收到 %d 字节 from %s:%zu",
             data.size(), sender_info.address.c_str(), sender_info.port);

    // 打印数据内容（如果是可打印字符）
    if (data.size() > 0) {
        std::string data_str(data.begin(), data.end());
        bool printable = true;
        for (uint8_t c : data) {
            if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
                printable = false;
                break;
            }
        }
        if (printable) {
            ESP_LOGI(TAG, "  内容: %s", data_str.c_str());
        } else {
            char hex_buf[64];
            size_t hex_len = 0;
            for (size_t i = 0; i < data.size() && i < 32; i++) {
                hex_len += snprintf(hex_buf + hex_len, sizeof(hex_buf) - hex_len,
                                   "%02X ", data[i]);
            }
            ESP_LOGI(TAG, "  Hex: %s", hex_buf);
        }
    }

    // 调用用户回调
    if (s_recv_callback) {
        s_recv_callback(data.data(), data.size(),
                       sender_info.address.c_str(), sender_info.port,
                       s_recv_ctx);
    }

    return std::nullopt;
}

/**
 * @brief TCP 服务器接收循环
 */
static void tcp_server_loop(void *arg)
{
    ESP_LOGI(TAG, "TCP 服务器任务启动");
    
    while (s_config.is_running) {
        if (!s_tcp_socket) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // 接受客户端连接
        if (!s_tcp_client) {
            ESP_LOGI(TAG, "等待 TCP 客户端连接...");
            s_tcp_client = s_tcp_socket->accept();
            if (s_tcp_client) {
                auto &info = s_tcp_client->get_remote_info();
                ESP_LOGI(TAG, "TCP 客户端已连接: %s:%zu", info.address.c_str(), info.port);
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!s_tcp_client->is_connected()) {
            ESP_LOGW(TAG, "TCP 客户端断开连接");
            s_tcp_client.reset();
            continue;
        }

        std::vector<uint8_t> data;
        if (s_tcp_client->receive(data, 1024)) {
            s_config.recv_count++;
            
            auto &info = s_tcp_client->get_remote_info();
            ESP_LOGI(TAG, "TCP 收到 %d 字节 from %s:%zu",
                     data.size(), info.address.c_str(), info.port);

            if (data.size() > 0) {
                std::string data_str(data.begin(), data.end());
                bool printable = true;
                for (uint8_t c : data) {
                    if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
                        printable = false;
                        break;
                    }
                }
                if (printable) {
                    ESP_LOGI(TAG, "  内容: %s", data_str.c_str());
                } else {
                    char hex_buf[64];
                    size_t hex_len = 0;
                    for (size_t i = 0; i < data.size() && i < 32; i++) {
                        hex_len += snprintf(hex_buf + hex_len, sizeof(hex_buf) - hex_len,
                                           "%02X ", data[i]);
                    }
                    ESP_LOGI(TAG, "  Hex: %s", hex_buf);
                }
            }

            if (s_recv_callback) {
                s_recv_callback(data.data(), data.size(),
                               info.address.c_str(), info.port,
                               s_recv_ctx);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(TAG, "TCP 服务器任务结束");
    vTaskDelete(NULL);
}

/**
 * @brief TCP 客户端接收循环
 */
static void tcp_client_loop(void *arg)
{
    ESP_LOGI(TAG, "TCP 客户端任务启动");
    
    while (s_config.is_running) {
        if (!s_tcp_client || !s_tcp_client->is_connected()) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        std::vector<uint8_t> data;
        if (s_tcp_client->receive(data, 1024)) {
            s_config.recv_count++;
            
            auto &info = s_tcp_client->get_remote_info();
            ESP_LOGI(TAG, "TCP 收到 %d 字节 from %s:%zu",
                     data.size(), info.address.c_str(), info.port);

            if (data.size() > 0) {
                std::string data_str(data.begin(), data.end());
                bool printable = true;
                for (uint8_t c : data) {
                    if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
                        printable = false;
                        break;
                    }
                }
                if (printable) {
                    ESP_LOGI(TAG, "  内容: %s", data_str.c_str());
                }
            }

            if (s_recv_callback) {
                s_recv_callback(data.data(), data.size(),
                               info.address.c_str(), info.port,
                               s_recv_ctx);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(TAG, "TCP 客户端任务结束");
    vTaskDelete(NULL);
}

// ==================== 公共 API 实现 ====================

extern "C" {

esp_err_t network_app_init(void)
{
    ESP_LOGI(TAG, "初始化网络通信模块");

    init_mutex();

    memset(&s_config, 0, sizeof(s_config));
    s_config.local_port = 5000;

    return ESP_OK;
}

esp_err_t network_app_deinit(void)
{
    ESP_LOGI(TAG, "反初始化网络通信模块");

    network_app_stop();

    if (s_config_mutex) {
        vSemaphoreDelete(s_config_mutex);
        s_config_mutex = nullptr;
    }

    return ESP_OK;
}

esp_err_t network_app_set_type(network_type_t type)
{
    if (s_config.is_running) {
        ESP_LOGE(TAG, "通信正在进行中，请先停止");
        return ESP_ERR_INVALID_STATE;
    }

    if (type != NETWORK_TYPE_UDP && type != NETWORK_TYPE_TCP) {
        ESP_LOGE(TAG, "无效的通信类型: %d", type);
        return ESP_ERR_INVALID_STATE;
    }

    s_config.type = type;
    ESP_LOGI(TAG, "通信类型设置为: %s", type == NETWORK_TYPE_UDP ? "UDP" : "TCP");

    return ESP_OK;
}

network_type_t network_app_get_type(void)
{
    return s_config.type;
}

esp_err_t network_app_set_mode(network_mode_t mode)
{
    s_config.mode = mode;
    ESP_LOGI(TAG, "通信模式设置为: %s", 
             mode == NETWORK_MODE_CLIENT ? "客户端" : "服务器");
    return ESP_OK;
}

esp_err_t network_app_set_target(const char *ip, uint16_t port)
{
    if (!ip || strlen(ip) >= sizeof(s_config.target_ip)) {
        ESP_LOGE(TAG, "无效的 IP 地址");
        return ESP_ERR_INVALID_ARG;
    }

    if (port == 0) {
        ESP_LOGE(TAG, "无效的端口号: %d", port);
        return ESP_ERR_INVALID_ARG;
    }

    if (s_config_mutex) {
        xSemaphoreTake(s_config_mutex, portMAX_DELAY);
    }
    strncpy(s_config.target_ip, ip, sizeof(s_config.target_ip) - 1);
    s_config.target_port = port;
    if (s_config_mutex) {
        xSemaphoreGive(s_config_mutex);
    }

    ESP_LOGI(TAG, "目标地址设置为: %s:%d", ip, port);

    return ESP_OK;
}

esp_err_t network_app_set_local_port(uint16_t port)
{
    s_config.local_port = port;
    ESP_LOGI(TAG, "本地端口设置为: %d", port);
    return ESP_OK;
}

esp_err_t network_app_start(void)
{
    if (s_config.is_running) {
        ESP_LOGW(TAG, "通信已在运行中");
        return ESP_OK;
    }

    if (s_config.type == NETWORK_TYPE_NONE) {
        ESP_LOGE(TAG, "请先设置通信类型");
        return ESP_ERR_INVALID_STATE;
    }

    if (!is_wifi_connected()) {
        ESP_LOGE(TAG, "WiFi 未连接");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "启动网络通信...");

    if (s_config.type == NETWORK_TYPE_UDP) {
        if (s_config.mode == NETWORK_MODE_SERVER) {
            ESP_LOGI(TAG, "启动 UDP 服务器，监听端口 %d", s_config.local_port);
            
            s_udp_socket = std::make_unique<espp::UdpSocket>(
                espp::UdpSocket::Config{
                    .log_level = espp::Logger::Verbosity::INFO
                }
            );

            auto task_config = espp::Task::BaseConfig{
                .name = "UdpServer",
                .stack_size_bytes = 8 * 1024,
            };
            
            auto recv_config = espp::UdpSocket::ReceiveConfig{
                .port = s_config.local_port,
                .buffer_size = 1024,
                .on_receive_callback = udp_receive_callback,
            };

            if (!s_udp_socket->start_receiving(task_config, recv_config)) {
                ESP_LOGE(TAG, "UDP 服务器启动失败");
                s_udp_socket.reset();
                return ESP_FAIL;
            }

            s_config.is_running = true;
            ESP_LOGI(TAG, "UDP 服务器已启动");
        } else {
            ESP_LOGI(TAG, "UDP 客户端已准备，目标: %s:%d", 
                     s_config.target_ip, s_config.target_port);
            s_config.is_running = true;
        }
    } else {
        if (s_config.mode == NETWORK_MODE_SERVER) {
            ESP_LOGI(TAG, "启动 TCP 服务器，监听端口 %d", s_config.local_port);
            
            s_tcp_socket = std::make_unique<espp::TcpSocket>(
                espp::TcpSocket::Config{
                    .log_level = espp::Logger::Verbosity::INFO
                }
            );

            if (!s_tcp_socket->bind(s_config.local_port)) {
                ESP_LOGE(TAG, "TCP 服务器绑定端口失败");
                s_tcp_socket.reset();
                return ESP_FAIL;
            }

            if (!s_tcp_socket->listen(1)) {
                ESP_LOGE(TAG, "TCP 服务器监听失败");
                s_tcp_socket.reset();
                return ESP_FAIL;
            }

            s_config.is_running = true;

            xTaskCreate(tcp_server_loop, "TcpServer", 8 * 1024, NULL, 5, &s_server_task_handle);
            
            ESP_LOGI(TAG, "TCP 服务器已启动，等待连接...");
        } else {
            if (strlen(s_config.target_ip) == 0 || s_config.target_port == 0) {
                ESP_LOGE(TAG, "请先设置目标地址 (net_target)");
                return ESP_ERR_INVALID_STATE;
            }

            ESP_LOGI(TAG, "连接 TCP 服务器 %s:%d", s_config.target_ip, s_config.target_port);
            
            s_tcp_client = std::make_unique<espp::TcpSocket>(
                espp::TcpSocket::Config{
                    .log_level = espp::Logger::Verbosity::INFO
                }
            );

            if (!s_tcp_client->connect({
                .ip_address = s_config.target_ip,
                .port = s_config.target_port
            })) {
                ESP_LOGE(TAG, "TCP 连接失败");
                s_tcp_client.reset();
                return ESP_FAIL;
            }

            s_config.is_running = true;

            xTaskCreate(tcp_client_loop, "TcpClient", 8 * 1024, NULL, 5, &s_server_task_handle);
            
            ESP_LOGI(TAG, "TCP 已连接到 %s:%d", s_config.target_ip, s_config.target_port);
        }
    }

    return ESP_OK;
}

esp_err_t network_app_stop(void)
{
    if (!s_config.is_running) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "停止网络通信...");

    s_config.is_running = false;

    if (s_server_task_handle) {
        vTaskDelete(s_server_task_handle);
        s_server_task_handle = nullptr;
    }

    s_udp_socket.reset();
    s_tcp_socket.reset();
    s_tcp_client.reset();

    ESP_LOGI(TAG, "网络通信已停止");

    return ESP_OK;
}

esp_err_t network_app_send(const uint8_t *data, size_t len)
{
    if (!s_config.is_running) {
        ESP_LOGE(TAG, "网络通信未启动");
        return ESP_ERR_INVALID_STATE;
    }

    if (!data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    bool success = false;

    if (s_config.type == NETWORK_TYPE_UDP) {
        // UDP 服务器模式下也需要发送功能
        if (s_config.target_ip[0] == '\0' || s_config.target_port == 0) {
            ESP_LOGE(TAG, "未设置目标地址 (net_target)");
            return ESP_ERR_INVALID_STATE;
        }

        // 使用临时 socket 发送（不影响监听 socket）
        auto send_socket = std::make_unique<espp::UdpSocket>(
            espp::UdpSocket::Config{}
        );

        success = send_socket->send(
            std::vector<uint8_t>(data, data + len),
            espp::UdpSocket::SendConfig{
                .ip_address = s_config.target_ip,
                .port = s_config.target_port
            }
        );

        // 局部 unique_ptr 自动释放，无需手动 reset

    } else {
        if (!s_tcp_client || !s_tcp_client->is_connected()) {
            ESP_LOGE(TAG, "TCP 未连接");
            return ESP_ERR_INVALID_STATE;
        }

        success = s_tcp_client->transmit(
            std::vector<uint8_t>(data, data + len),
            espp::TcpSocket::TransmitConfig::Default()
        );
    }

    if (success) {
        s_config.send_count++;
        ESP_LOGI(TAG, "发送成功: %d 字节", len);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "发送失败");
        return ESP_FAIL;
    }
}

esp_err_t network_app_send_string(const char *str)
{
    if (!str) {
        return ESP_ERR_INVALID_ARG;
    }
    return network_app_send((const uint8_t *)str, strlen(str));
}

const network_config_t* network_app_get_config(void)
{
    return &s_config;
}

bool network_app_is_running(void)
{
    return s_config.is_running;
}

esp_err_t network_app_get_local_ip(char *ip_str, size_t len)
{
    if (!ip_str || len < 16) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_netif_ip_info_t ip_info;
    esp_err_t ret = wifi_app_get_ip_info(&ip_info);
    if (ret != ESP_OK) {
        return ret;
    }

    snprintf(ip_str, len, IPSTR, IP2STR(&ip_info.ip));
    return ESP_OK;
}

void network_app_set_recv_callback(network_recv_callback_t callback, void *ctx)
{
    s_recv_callback = callback;
    s_recv_ctx = ctx;
}

void network_app_print_status(void)
{
    ESP_LOGI(TAG, "========== 网络状态 ==========");
    ESP_LOGI(TAG, "通信类型: %s",
             s_config.type == NETWORK_TYPE_UDP ? "UDP" :
             s_config.type == NETWORK_TYPE_TCP ? "TCP" : "未设置");
    ESP_LOGI(TAG, "通信模式: %s",
             s_config.mode == NETWORK_MODE_CLIENT ? "客户端" :
             s_config.mode == NETWORK_MODE_SERVER ? "服务器" : "未设置");
    ESP_LOGI(TAG, "本地端口: %d", s_config.local_port);
    
    if (s_config.target_ip[0] != '\0') {
        ESP_LOGI(TAG, "目标地址: %s:%d", s_config.target_ip, s_config.target_port);
    }
    
    ESP_LOGI(TAG, "运行状态: %s", s_config.is_running ? "运行中" : "已停止");
    ESP_LOGI(TAG, "接收计数: %lu", (unsigned long)s_config.recv_count);
    ESP_LOGI(TAG, "发送计数: %lu", (unsigned long)s_config.send_count);

    char local_ip[16];
    if (network_app_get_local_ip(local_ip, sizeof(local_ip)) == ESP_OK) {
        ESP_LOGI(TAG, "本地 IP: %s", local_ip);
    }

    ESP_LOGI(TAG, "==============================");
}

} // extern "C"
