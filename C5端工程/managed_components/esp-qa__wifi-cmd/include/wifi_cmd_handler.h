/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* This header file is not included when WIFI is not enabled */
#pragma once

#include "wifi_cmd_config.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================ WIFI_EVENT (basic) =============================================================================== */

/* Default handler for WIFI_EVENT_STA_DISCONNECTED */
void wifi_cmd_default_handler_sta_disconnected(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

__attribute__((weak)) void wifi_cmd_handler_sta_disconnected(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

/* Default handler for WIFI_EVENT_STA_CONNECTED */
void wifi_cmd_default_handler_sta_connected(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

__attribute__((weak)) void wifi_cmd_handler_sta_connected(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

/* Default handler for WIFI_EVENT_SCAN_DONE */
void wifi_cmd_default_handler_scan_done(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

__attribute__((weak)) void wifi_cmd_handler_scan_done(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

/* Default handler for WIFI_EVENT_STA_BEACON_TIMEOUT */
void wifi_cmd_default_handler_sta_beacon_timeout(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

__attribute__((weak)) void wifi_cmd_handler_sta_beacon_timeout(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

/* ================================================================ WIFI_EVENT ITWT ================================================================================= */

#if WIFI_CMD_ITWT_ENABLED

/* Default handler for WIFI_EVENT_ITWT_SETUP */
void wifi_cmd_default_handler_itwt_setup(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

__attribute__((weak)) void wifi_cmd_handler_itwt_setup(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

/* Default handler for WIFI_EVENT_ITWT_TEARDOWN */
void wifi_cmd_default_handler_itwt_teardown(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

__attribute__((weak)) void wifi_cmd_handler_itwt_teardown(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

/* Default handler for WIFI_EVENT_ITWT_SUSPEND */
void wifi_cmd_default_handler_itwt_suspend(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

__attribute__((weak)) void wifi_cmd_handler_itwt_suspend(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

/* Default handler for WIFI_EVENT_ITWT_PROBE */
void wifi_cmd_default_handler_itwt_probe(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

__attribute__((weak)) void wifi_cmd_handler_itwt_probe(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

#endif /* WIFI_CMD_ITWT_ENABLED */

/* =================================================================== IP_EVENT =================================================================================== */

/* Default handler for IP_EVENT_STA_GOT_IP */
void wifi_cmd_default_handler_sta_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

__attribute__((weak)) void wifi_cmd_handler_sta_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

/* Default handler for IP_EVENT_GOT_IP6 */
void wifi_cmd_default_handler_sta_got_ipv6(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

__attribute__((weak)) void wifi_cmd_handler_sta_got_ipv6(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

/* Default handler for WIFI_EVENT_AP_STADISCONNECTED */
void wifi_cmd_default_handler_ap_stadisconnected(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

__attribute__((weak)) void wifi_cmd_handler_ap_stadisconnected(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

#ifdef __cplusplus
}
#endif
