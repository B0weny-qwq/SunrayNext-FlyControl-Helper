#include "bridge/infra/bridge_internal.h"

#include <stdlib.h>
#include <string.h>
#include <utility>

#include "esp_log.h"
#include "esp_timer.h"
#include "bridge/adapter/mavlink_parser.h"
#include "transport/adapter/uart_port.h"

static const char *TAG = "bridge_net";

void bridge_ordered_buffer_init(ordered_buffer_t *buf, size_t size) {
    buf->packets = static_cast<ordered_packet_t *>(calloc(size, sizeof(ordered_packet_t)));
    buf->size = size;
    buf->next_seq = 0;
    buf->mutex = xSemaphoreCreateMutex();
}

void bridge_ordered_buffer_deinit(ordered_buffer_t *buf) {
    if (buf->packets) {
        free(buf->packets);
        buf->packets = NULL;
    }
    if (buf->mutex) {
        vSemaphoreDelete(buf->mutex);
        buf->mutex = NULL;
    }
}

bool bridge_ordered_buffer_push(ordered_buffer_t *buf, const uint8_t *data, size_t len, uint32_t sequence) {
    if (!buf || !buf->packets || !data) {
        return false;
    }
    if (xSemaphoreTake(buf->mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }

    const int32_t diff = (int32_t)(sequence - buf->next_seq);
    if (diff < -(int32_t)MAVLINK_SEQ_WINDOW || diff > (int32_t)MAVLINK_SEQ_WINDOW) {
        xSemaphoreGive(buf->mutex);
        return false;
    }

    ordered_packet_t *slot = &buf->packets[sequence % buf->size];
    slot->sequence = sequence;
    slot->timestamp = (uint64_t)(esp_timer_get_time() / 1000);
    slot->len = len > MAVLINK_MAX_FRAME_LEN ? MAVLINK_MAX_FRAME_LEN : len;
    memcpy(slot->data, data, slot->len);
    slot->used = true;
    xSemaphoreGive(buf->mutex);
    return true;
}

bool bridge_ordered_buffer_pop(ordered_buffer_t *buf, uint8_t *data, size_t *len) {
    if (!buf || !buf->packets || !data || !len) {
        return false;
    }
    if (xSemaphoreTake(buf->mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return false;
    }

    ordered_packet_t *slot = &buf->packets[buf->next_seq % buf->size];
    if (!slot->used) {
        xSemaphoreGive(buf->mutex);
        return false;
    }

    *len = slot->len;
    memcpy(data, slot->data, *len);
    slot->used = false;
    buf->next_seq++;
    xSemaphoreGive(buf->mutex);
    return true;
}

static std::optional<std::vector<uint8_t>> udp_receive_callback(std::vector<uint8_t> &data, const espp::Socket::Info &) {
    if (data.empty() || !g_bridge_handle) {
        return std::nullopt;
    }

    const uint32_t seq = mavlink_is_frame(data.data(), data.size())
                             ? mavlink_get_sequence(data.data())
                             : (uint32_t)(esp_timer_get_time() / 1000);
    if (g_bridge_handle->ordered_mode) {
        if (!bridge_ordered_buffer_push(&g_bridge_handle->ordered_buf, data.data(), data.size(), seq)) {
            g_bridge_handle->stats.udp_drop_count++;
            return std::nullopt;
        }
    } else {
        mavlink_bridge_net_to_all_uarts(g_bridge_handle, data.data(), data.size());
    }

    g_bridge_handle->stats.udp_rx_count++;
    if (g_bridge_handle->net_callback) {
        g_bridge_handle->net_callback(UART_ID_MAX, data.data(), data.size(), g_bridge_handle->net_callback_ctx);
    }
    return std::nullopt;
}

static void ordered_send_task(void *arg) {
    auto *handle = static_cast<mavlink_bridge_handle_t>(arg);
    uint8_t buffer[MAVLINK_MAX_FRAME_LEN];
    size_t len = 0;

    while (handle->state == MAVLINK_STATE_RUNNING) {
        if (bridge_ordered_buffer_pop(&handle->ordered_buf, buffer, &len)) {
            if (xSemaphoreTake(handle->uart_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                for (int i = 0; i < handle->uart_count; i++) {
                    if (handle->uarts[i].enabled) {
                        bridge_uart_send(handle, &handle->uarts[i], buffer, len);
                    }
                }
                xSemaphoreGive(handle->uart_mutex);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    handle->ordered_send_task = NULL;
    vTaskDelete(NULL);
}

static void tcp_client_task(void *arg) {
    auto *client = static_cast<bridge_tcp_client_t *>(arg);

    while (client && client->connected && g_bridge_handle && g_bridge_handle->state == MAVLINK_STATE_RUNNING) {
        if (!client->socket || !client->socket->is_connected()) {
            client->connected = false;
            break;
        }

        std::vector<uint8_t> data;
        if (client->socket->receive(data, MAVLINK_MAX_FRAME_LEN) && !data.empty()) {
            g_bridge_handle->stats.tcp_rx_count++;
            const uint32_t seq = mavlink_is_frame(data.data(), data.size())
                                     ? mavlink_get_sequence(data.data())
                                     : (uint32_t)(esp_timer_get_time() / 1000);

            if (g_bridge_handle->ordered_mode) {
                if (!bridge_ordered_buffer_push(&g_bridge_handle->ordered_buf, data.data(), data.size(), seq)) {
                    g_bridge_handle->stats.udp_drop_count++;
                }
            } else {
                mavlink_bridge_net_to_all_uarts(g_bridge_handle, data.data(), data.size());
            }

            if (g_bridge_handle->net_callback) {
                g_bridge_handle->net_callback(UART_ID_MAX, data.data(), data.size(), g_bridge_handle->net_callback_ctx);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    if (client) {
        client->task_handle = NULL;
    }
    vTaskDelete(NULL);
}

static void tcp_server_task(void *arg) {
    auto *handle = static_cast<mavlink_bridge_handle_t>(arg);

    while (handle->tcp_running) {
        auto client_socket = handle->tcp_server ? handle->tcp_server->accept() : nullptr;
        if (!client_socket) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (xSemaphoreTake(handle->tcp_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        int active_clients = 0;
        for (auto &client : handle->tcp_clients) {
            if (client.connected) {
                active_clients++;
            }
        }

        if (active_clients < MAVLINK_MAX_TCP_CLIENTS) {
            auto &remote = client_socket->get_remote_info();
            bridge_tcp_client_t client = {};
            client.socket = std::move(client_socket);
            strncpy(client.ip_str, remote.address.c_str(), sizeof(client.ip_str) - 1);
            client.port = remote.port;
            client.connected = true;
            handle->tcp_clients.push_back(std::move(client));
            bridge_tcp_client_t *created = &handle->tcp_clients.back();
            xTaskCreatePinnedToCore(tcp_client_task, "TcpClient", 4096, created, 10, &created->task_handle, 0);
        } else {
            ESP_LOGW(TAG, "Max TCP clients reached");
        }

        xSemaphoreGive(handle->tcp_mutex);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    handle->tcp_server_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t bridge_network_start(mavlink_bridge_handle_t handle) {
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->udp_socket = std::make_unique<espp::UdpSocket>(
        espp::UdpSocket::Config{.log_level = espp::Logger::Verbosity::WARN});
    auto task_config = espp::Task::BaseConfig{.name = "MavlinkUdp", .stack_size_bytes = 4096};
    auto recv_config = espp::UdpSocket::ReceiveConfig{
        .port = handle->udp_port,
        .buffer_size = MAVLINK_MAX_FRAME_LEN,
        .on_receive_callback = udp_receive_callback,
    };

    if (!handle->udp_socket->start_receiving(task_config, recv_config)) {
        ESP_LOGE(TAG, "Failed to start UDP server on port %d", handle->udp_port);
        return ESP_FAIL;
    }

    if (handle->tcp_port > 0) {
        handle->tcp_server = std::make_unique<espp::TcpSocket>(
            espp::TcpSocket::Config{.log_level = espp::Logger::Verbosity::WARN});
        if (handle->tcp_server->bind(handle->tcp_port) && handle->tcp_server->listen(5)) {
            handle->tcp_running = true;
            xTaskCreatePinnedToCore(tcp_server_task, "MavlinkTcp", 4096, handle, 10, &handle->tcp_server_task, 0);
        }
    }

    if (handle->ordered_mode) {
        xTaskCreatePinnedToCore(ordered_send_task, "MavlinkSend", 4096, handle, 10, &handle->ordered_send_task, 0);
    }
    return ESP_OK;
}

void bridge_network_stop(mavlink_bridge_handle_t handle) {
    if (!handle) {
        return;
    }

    handle->tcp_running = false;
    if (handle->tcp_server_task) {
        vTaskDelete(handle->tcp_server_task);
        handle->tcp_server_task = NULL;
    }
    if (handle->ordered_send_task) {
        vTaskDelete(handle->ordered_send_task);
        handle->ordered_send_task = NULL;
    }

    if (handle->tcp_mutex && xSemaphoreTake(handle->tcp_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (auto &client : handle->tcp_clients) {
            client.connected = false;
            client.socket.reset();
            if (client.task_handle) {
                vTaskDelete(client.task_handle);
                client.task_handle = NULL;
            }
        }
        handle->tcp_clients.clear();
        xSemaphoreGive(handle->tcp_mutex);
    }

    handle->tcp_server.reset();
    handle->udp_socket.reset();
}

esp_err_t bridge_tcp_broadcast(mavlink_bridge_handle_t handle, const uint8_t *data, size_t len) {
    if (!handle || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(handle->tcp_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    std::vector<uint8_t> payload(data, data + len);
    for (auto &client : handle->tcp_clients) {
        if (client.connected && client.socket) {
            client.socket->transmit(payload);
        }
    }

    xSemaphoreGive(handle->tcp_mutex);
    return ESP_OK;
}
