/**
 * @file network_cmd.h
 * @brief P4 WiFi Remote 网络通信命令模块头文件
 *
 * @author P4 Team
 * @date 2026-04-04
 */

#ifndef NETWORK_CMD_H
#define NETWORK_CMD_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册所有网络通信命令
 *
 * @return ESP_OK 成功
 *         其他 注册失败
 */
esp_err_t network_cmd_register(void);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_CMD_H */
