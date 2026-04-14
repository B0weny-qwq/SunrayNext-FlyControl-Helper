#pragma once

#include "esp_err.h"
#include "network/model/network_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t network_app_init(void);
esp_err_t network_app_deinit(void);
esp_err_t network_app_set_type(network_type_t type);
network_type_t network_app_get_type(void);
esp_err_t network_app_set_mode(network_mode_t mode);
esp_err_t network_app_set_target(const char *ip, uint16_t port);
esp_err_t network_app_set_local_port(uint16_t port);
esp_err_t network_app_start(void);
esp_err_t network_app_stop(void);
esp_err_t network_app_send(const uint8_t *data, size_t len);
esp_err_t network_app_send_string(const char *str);
const network_config_t *network_app_get_config(void);
bool network_app_is_running(void);
esp_err_t network_app_get_local_ip(char *ip_str, size_t len);
void network_app_set_recv_callback(network_recv_callback_t callback, void *ctx);
void network_app_print_status(void);

#ifdef __cplusplus
}
#endif
