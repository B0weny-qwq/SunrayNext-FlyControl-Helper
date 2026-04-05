/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Deprecated, remove in v1.0 */
#define app_initialise_wifi wifi_cmd_initialize_wifi
#define app_register_wifi_init_deinit wifi_cmd_register_wifi_init_deinit
#define app_register_wifi_basic_commands wifi_cmd_register_base_basic
#define app_register_sta_basic_commands wifi_cmd_register_sta_basic
#define app_register_ap_basic_commands wifi_cmd_register_ap_basic
#define app_register_all_wifi_commands wifi_cmd_register_all
#define app_register_wifi_band_command wifi_cmd_register_wifi_band
#define app_register_itwt_commands wifi_cmd_register_itwt
#define app_register_wifi_stats_commands wifi_cmd_register_stats
#define app_register_wifi_he_debug_commands wifi_cmd_register_he_debug

#ifdef __cplusplus
}
#endif
