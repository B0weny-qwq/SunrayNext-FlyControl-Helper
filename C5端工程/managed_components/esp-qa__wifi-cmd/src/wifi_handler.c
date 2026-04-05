/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "esp_log.h"

#include "wifi_cmd.h"

#if WIFI_CMD_ITWT_ENABLED
#include "esp_wifi_he_types.h"
#endif

#ifndef APP_TAG
#define APP_TAG "WIFI"
#endif

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

bool g_is_sta_wifi_connected = false;
bool g_is_sta_got_ip4 = false;
uint8_t g_wifi_cmd_scan_info_level = SCAN_INFO_LEVEL_1;
wifi_cmd_config_t g_wifi_cmd_config = WIFI_CMD_CONFIG_DEFAULT();

void wifi_cmd_default_handler_sta_disconnected(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    g_wcount.sta_disconnected += 1;

    g_is_sta_wifi_connected = false;
    g_is_sta_got_ip4 = false;
    wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
    ESP_LOGI(APP_TAG, "WIFI_EVENT_STA_DISCONNECTED! reason: %d", event->reason);
    // WIFI_REASON_ASSOC_LEAVE means user disconnect first
    // WIFI_REASON_STA_LEAVING means user disconnect when in connecting phase
    if (event->reason == WIFI_REASON_ASSOC_LEAVE || event->reason == WIFI_REASON_STA_LEAVING) {
        ESP_LOGD(APP_TAG, "call sta_connect second time without disconnect will send deauth, reason is WIFI_EVENT_STA_DISCONNECTED");
        return;
    }

    if (!g_wifi_cmd_config.reconnect) {
        return;
    }
    g_wifi_connect_retry_cnt++;
    if (g_wifi_connect_retry_cnt > g_wifi_cmd_config.reconnect_max_retry) {
        return;
    }
    ESP_LOGI(APP_TAG, "trying to reconnect...");
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(APP_TAG, "WiFi Reconnect failed! (%s)", esp_err_to_name(err));
        return;
    }
}

void wifi_cmd_default_handler_ap_stadisconnected(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0) /* IDF commit: 395ad3edc029bf4523ee7db60783389d1f12f375 */
    ESP_LOGI(APP_TAG, "WIFI_EVENT_AP_STADISCONNECTED,"MACSTR",%u", MAC2STR(event->mac), event->reason);
#else
    ESP_LOGI(APP_TAG, "WIFI_EVENT_AP_STADISCONNECTED,"MACSTR, MAC2STR(event->mac));
#endif

}

void wifi_cmd_default_handler_sta_connected(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    g_is_sta_wifi_connected = true;
    ESP_LOGI(APP_TAG, "WIFI_EVENT_STA_CONNECTED!");
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 8) /* IDF commit: 09f26ece10b3519ad23b0e0fdd1c4dfb2958117e */
    wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
    ESP_LOGI(APP_TAG, "STA_CONNECTED_AUTH:%d", event->authmode);
#endif
#if WIFI_CMD_IPV6_ENABLED && !CONFIG_WIFI_CMD_NO_LWIP
    esp_netif_create_ip6_linklocal(g_netif_sta);
#endif // WIFI_CMD_IPV6_ENABLED && !CONFIG_WIFI_CMD_NO_LWIP
}

void wifi_cmd_default_handler_sta_got_ip(void *arg, esp_event_base_t event_base,
                                         int32_t event_id, void *event_data)
{
    g_is_sta_got_ip4 = true;
    g_wifi_connect_retry_cnt = 0;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(APP_TAG, "IP_EVENT_STA_GOT_IP: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    ESP_LOGI(APP_TAG, "- IPv4 address: " IPSTR ",", IP2STR(&event->ip_info.ip));

}

#if WIFI_CMD_IPV6_ENABLED
void wifi_cmd_default_handler_sta_got_ipv6(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
    esp_ip6_addr_type_t ipv6_type = esp_netif_ip6_get_addr_type(&event->ip6_info.ip);
    ESP_LOGI(APP_TAG, "IP_EVENT_GOT_IP6: Interface \"%s\" address: " IPV6STR ", type: %d", esp_netif_get_desc(event->esp_netif),
             IPV62STR(event->ip6_info.ip), ipv6_type);
    ESP_LOGI(APP_TAG, "- IPv6 address: " IPV6STR ", type: %d", IPV62STR(event->ip6_info.ip), ipv6_type);
}
#endif // WIFI_CMD_IPV6_ENABLED

void wifi_cmd_default_handler_scan_done(void *arg, esp_event_base_t event_base,
                                        int32_t event_id, void *event_data)
{
    uint16_t sta_number = 0;
    uint8_t i;
    wifi_ap_record_t *ap_list_buffer;

    esp_wifi_scan_get_ap_num(&sta_number);
    if (!sta_number) {
        ESP_LOGE(APP_TAG, "SCAN_DONE: No AP found");
        return;
    }

    if (g_wifi_cmd_scan_info_level == SCAN_INFO_LEVEL_0) {
        ESP_LOGI(APP_TAG, "SCAN_DONE: Found %d APs", sta_number);
#ifdef esp_wifi_clear_ap_list /* IDF >= v4.3, MR: 19441 */
        esp_wifi_clear_ap_list();
#endif
        return;
    }

    ap_list_buffer = malloc(sta_number * sizeof(wifi_ap_record_t));
    if (ap_list_buffer == NULL) {
        ESP_LOGE(APP_TAG, "SCAN_DONE: Failed to malloc buffer to print scan results");
#ifdef esp_wifi_clear_ap_list /* IDF >= v4.3, MR: 19441 */
        esp_wifi_clear_ap_list();
#endif
        return;
    }

    if (esp_wifi_scan_get_ap_records(&sta_number, (wifi_ap_record_t *)ap_list_buffer) == ESP_OK) {
        printf("\n");
        for (i = 0; i < sta_number; i++) {
            char scan_result[256] = {0, };
            wifi_phy_support_info_t phy_info = {
                .phy_11b  = ap_list_buffer[i].phy_11b,
                .phy_11g  = ap_list_buffer[i].phy_11g,
                .phy_11n  = ap_list_buffer[i].phy_11n,
                .phy_lr   = ap_list_buffer[i].phy_lr,
#if WIFI_CMD_5G_SUPPORTED
                .phy_11a  = ap_list_buffer[i].phy_11a,
                .phy_11ac = ap_list_buffer[i].phy_11ac,
#endif
#if WIFI_CMD_HE_SUPPORTED
                .phy_11ax = ap_list_buffer[i].phy_11ax,
#endif
            };
            uint32_t phy_bitmap = wifi_cmd_get_phy_bitmap(&phy_info);
            sprintf(scan_result,  "+SCAN:["MACSTR"][%s][rssi=%d][auth=%s][ch=%d]",
                    MAC2STR(ap_list_buffer[i].bssid), ap_list_buffer[i].ssid, ap_list_buffer[i].rssi,
                    wifi_cmd_auth_mode_num2str(ap_list_buffer[i].authmode), ap_list_buffer[i].primary);
            if (g_wifi_cmd_scan_info_level & SCAN_SHOW_DETAIL_INFO) {
                sprintf(scan_result + strlen(scan_result), "[second=%d][proto=%s]",
                        ap_list_buffer[i].second, wifi_cmd_protocol_bitmap2str(phy_bitmap));
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 1)   /* IDF commit: f0365ba6b5d832a2e5999fda56b89c1f18c02858 */
                sprintf(scan_result + strlen(scan_result), "[cbw=%s]",
                        wifi_cmd_ap_bandwidth_to_str(ap_list_buffer[i].bandwidth));
#endif
#if WIFI_CMD_5G_SUPPORTED
                if (ap_list_buffer[i].bandwidth >= WIFI_BW80) {
                    sprintf(scan_result + strlen(scan_result), "[vht_freq=%u/%u]",
                            ap_list_buffer[i].vht_ch_freq1, ap_list_buffer[i].vht_ch_freq2);
                }
#endif
            }
#if WIFI_CMD_HE_SUPPORTED
            if (g_wifi_cmd_scan_info_level & SCAN_SHOW_AX_INFO) {
                if (phy_bitmap & WIFI_PROTOCOL_11AX) {
                    sprintf(scan_result + strlen(scan_result), "[bssid_index=%d][bss_color=%d/%s]",
                            ap_list_buffer[i].he_ap.bssid_index, ap_list_buffer[i].he_ap.bss_color,
                            ap_list_buffer[i].he_ap.bss_color_disabled ? "disabled" : "enabled");
                } else {
                    sprintf(scan_result + strlen(scan_result), "[non_he_ap]");
                }
            }
#endif
            ESP_LOGI(APP_TAG, "%s", scan_result);
        }
    }
    free(ap_list_buffer);
    ESP_LOGI(APP_TAG, "SCAN_DONE: Found %d APs", sta_number);
}

void wifi_cmd_default_handler_sta_beacon_timeout(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    g_wcount.bcn_timeout += 1;
    /* WiFi will print "wifi:bcn_timeout,ap_probe_send_start" */
    // ESP_LOGI(APP_TAG, "WIFI_EVENT_STA_BEACON_TIMEOUT!");
}

#if WIFI_CMD_ITWT_ENABLED

#define ITWT_SETUP_SUCCESS 1

static const char *itwt_probe_status_to_str(wifi_itwt_probe_status_t status)
{
    switch (status) {
    case ITWT_PROBE_FAIL:                 return "itwt probe fail";
    case ITWT_PROBE_SUCCESS:              return "itwt probe success";
    case ITWT_PROBE_TIMEOUT:              return "itwt probe timeout";
    case ITWT_PROBE_STA_DISCONNECTED:     return "Sta disconnected";
    default:                              return "Unknown status";
    }
}

void wifi_cmd_default_handler_itwt_setup(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_event_sta_itwt_setup_t *setup = (wifi_event_sta_itwt_setup_t *) event_data;
    if (setup->status == ITWT_SETUP_SUCCESS) {
        /* TWT Wake Interval = TWT Wake Interval Mantissa * (2 ^ TWT Wake Interval Exponent) */
        ESP_LOGI(APP_TAG, "<WIFI_EVENT_ITWT_SETUP>twt_id:%d, flow_id:%d, %s, %s, wake_dura:%d, wake_dura_unit:%d, wake_invl_e:%d, wake_invl_m:%d", setup->config.twt_id,
                 setup->config.flow_id, setup->config.trigger ? "trigger-enabled" : "non-trigger-enabled", setup->config.flow_type ? "unannounced" : "announced",
                 setup->config.min_wake_dura, setup->config.wake_duration_unit, setup->config.wake_invl_expn, setup->config.wake_invl_mant);
        ESP_LOGI(APP_TAG, "<WIFI_EVENT_ITWT_SETUP>target wake time:%lld, wake duration:%d us, service period:%d us", setup->target_wake_time, setup->config.min_wake_dura << (setup->config.wake_duration_unit == 1 ? 10 : 8),
                 setup->config.wake_invl_mant << setup->config.wake_invl_expn);
    } else {
        if (setup->status == ESP_ERR_WIFI_TWT_SETUP_TIMEOUT) {
            ESP_LOGE(APP_TAG, "<WIFI_EVENT_ITWT_SETUP>twt_id:%d, timeout of receiving twt setup response frame", setup->config.twt_id);
        } else if (setup->status == ESP_ERR_WIFI_TWT_SETUP_TXFAIL) {
            ESP_LOGE(APP_TAG, "<WIFI_EVENT_ITWT_SETUP>twt_id:%d, twt setup frame tx failed, reason: %d", setup->config.twt_id, setup->reason);
        } else if (setup->status == ESP_ERR_WIFI_TWT_SETUP_REJECT) {
            ESP_LOGE(APP_TAG, "<WIFI_EVENT_ITWT_SETUP>twt_id:%d, twt setup request was rejected, setup cmd: %d", setup->config.twt_id, setup->config.setup_cmd);
        } else {
            ESP_LOGE(APP_TAG, "<WIFI_EVENT_ITWT_SETUP>twt_id:%d, twt setup failed, status: %d", setup->config.twt_id, setup->status);
        }
    }
}

void wifi_cmd_default_handler_itwt_teardown(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_event_sta_itwt_teardown_t *teardown = (wifi_event_sta_itwt_teardown_t *) event_data;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)  // IDF MR: 30679
    // sta tx twt teardown action but not receive ack
    if (teardown->status == ITWT_TEARDOWN_FAIL) {
        ESP_LOGE(APP_TAG, "<WIFI_EVENT_ITWT_TEARDOWN>flow_id %d%s, twt teardown frame tx failed", teardown->flow_id, (teardown->flow_id == 8) ? "(all twt)" : "");
        return;
    }
#endif /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0) */
    ESP_LOGI(APP_TAG, "<WIFI_EVENT_ITWT_TEARDOWN>flow_id %d%s", teardown->flow_id, (teardown->flow_id == 8) ? "(all twt)" : "");
}

void wifi_cmd_default_handler_itwt_suspend(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_event_sta_itwt_suspend_t *suspend = (wifi_event_sta_itwt_suspend_t *) event_data;
    ESP_LOGI(APP_TAG, "<WIFI_EVENT_ITWT_SUSPEND>status:%d, flow_id_bitmap:0x%x, actual_suspend_time_ms:[%lu %lu %lu %lu %lu %lu %lu %lu]",
             suspend->status, suspend->flow_id_bitmap,
             suspend->actual_suspend_time_ms[0], suspend->actual_suspend_time_ms[1], suspend->actual_suspend_time_ms[2], suspend->actual_suspend_time_ms[3],
             suspend->actual_suspend_time_ms[4], suspend->actual_suspend_time_ms[5], suspend->actual_suspend_time_ms[6], suspend->actual_suspend_time_ms[7]);
}

void wifi_cmd_default_handler_itwt_probe(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_event_sta_itwt_probe_t *probe = (wifi_event_sta_itwt_probe_t *) event_data;
    ESP_LOGI(APP_TAG, "<WIFI_EVENT_ITWT_PROBE>status:%s, reason:0x%x", itwt_probe_status_to_str(probe->status), probe->reason);
}
#endif /* WIFI_CMD_ITWT_ENABLED */

/* ================================================================ weak functions ================================================================================= */

__attribute__((weak)) void wifi_cmd_handler_sta_disconnected(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    return wifi_cmd_default_handler_sta_disconnected(arg, event_base, event_id, event_data);
}
__attribute__((weak)) void wifi_cmd_handler_sta_connected(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    return wifi_cmd_default_handler_sta_connected(arg, event_base, event_id, event_data);
}
__attribute__((weak)) void wifi_cmd_handler_scan_done(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    return wifi_cmd_default_handler_scan_done(arg, event_base, event_id, event_data);
}
__attribute__((weak)) void wifi_cmd_handler_sta_beacon_timeout(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    return wifi_cmd_default_handler_sta_beacon_timeout(arg, event_base, event_id, event_data);
}
#if WIFI_CMD_ITWT_ENABLED
__attribute__((weak)) void wifi_cmd_handler_itwt_setup(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    return wifi_cmd_default_handler_itwt_setup(arg, event_base, event_id, event_data);
}
__attribute__((weak)) void wifi_cmd_handler_itwt_teardown(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    return wifi_cmd_default_handler_itwt_teardown(arg, event_base, event_id, event_data);
}
__attribute__((weak)) void wifi_cmd_handler_itwt_suspend(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    return wifi_cmd_default_handler_itwt_suspend(arg, event_base, event_id, event_data);
}

__attribute__((weak)) void wifi_cmd_handler_itwt_probe(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    return wifi_cmd_default_handler_itwt_probe(arg, event_base, event_id, event_data);
}
#endif /* WIFI_CMD_ITWT_ENABLED */

__attribute__((weak)) void wifi_cmd_handler_sta_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    return wifi_cmd_default_handler_sta_got_ip(arg, event_base, event_id, event_data);
}
__attribute__((weak)) void wifi_cmd_handler_sta_got_ipv6(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    return wifi_cmd_default_handler_sta_got_ipv6(arg, event_base, event_id, event_data);
}
__attribute__((weak)) void wifi_cmd_handler_ap_stadisconnected(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    return wifi_cmd_default_handler_ap_stadisconnected(arg, event_base, event_id, event_data);
}
