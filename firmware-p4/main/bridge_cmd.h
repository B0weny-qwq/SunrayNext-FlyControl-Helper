/**
 * @file bridge_cmd.h
 * @brief MAVLink 桥接 CLI 命令接口
 *
 * @author P4 Team
 * @date 2026-04-05
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册桥接相关命令
 *
 * 在 console_app_init() 之前调用
 *
 * @return ESP_OK 成功
 */
esp_err_t bridge_cmd_register(void);

#ifdef __cplusplus
}
#endif
