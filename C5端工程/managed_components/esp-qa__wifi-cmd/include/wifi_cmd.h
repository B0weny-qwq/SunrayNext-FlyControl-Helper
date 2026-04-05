/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_idf_version.h"
#include "wifi_cmd_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#if WIFI_CMD_WIFI_ENABLED

#include "wifi_cmd_handler.h"
#include "wifi_cmd_deprecated.h"

/* Avoid re-connect forever */
#define DEF_WIFI_CONN_MAX_RETRY_CNT (30)

/*
 * Default rssi offset value for 5G SSID when connect wifi
 * Make sta prefer to connect SSID on 5G band when do full scan connect
 * This value will affact SSID sort order when sort method is WIFI_CONNECT_AP_BY_SIGNAL
 */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0) && !CONFIG_WIFI_CMD_BASIC_ONLY
#define DEF_5G_CONN_RSSI_OFFSET     (10)
#endif

typedef struct {
    bool reconnect;
    uint32_t reconnect_max_retry;
} wifi_cmd_config_t;

#define WIFI_CMD_CONFIG_DEFAULT() { \
    .reconnect = true, \
    .reconnect_max_retry = DEF_WIFI_CONN_MAX_RETRY_CNT, \
}

#define LOG_WIFI_CMD_DONE(ret, desc) do { \
    if (ret == ESP_OK) { \
        ESP_LOGI(APP_TAG, "DONE.%s,OK.", desc); \
    } else { \
        ESP_LOGI(APP_TAG, "DONE.%s,FAIL.%d,%s", desc, ret, esp_err_to_name(ret)); \
    } \
} while(0)

#define WIFI_ERR_CHECK_LOG(ret, desc) do { \
    if (ret != ESP_OK) { \
        ESP_LOGW(APP_TAG, "@EW:failed:%s,%d,%s", desc, ret, esp_err_to_name(ret)); \
    } \
} while(0)

#define WIFI_ERR_PRINT_LOG(ret, desc) do { \
    if (ret != ESP_OK) { \
        ESP_LOGW(APP_TAG, "failed:%s,%d,%s", desc, ret, esp_err_to_name(ret)); \
    } \
} while(0)

/* TODO: v1.0 use wifi_init_config_t directly */
typedef struct {
    wifi_init_config_t wifi_init_cfg;
    wifi_storage_t storage;
    wifi_ps_type_t ps_type;
    bool disable_11b_rate;
    uint32_t magic;
} wifi_cmd_initialize_cfg_t;

#define app_wifi_initialise_config_t wifi_cmd_initialize_cfg_t
#define WIFI_CMD_INIT_MAGIC 0x00FF7161
#define WIFI_CMD_INIT_MAGIC_DEPRECATED_APP 0xA1A2A3A4

/* Deprecated, remove in v1.0 */
#define APP_WIFI_CONFIG_DEFAULT() { \
    .storage = WIFI_STORAGE_FLASH, \
    .ps_type = WIFI_PS_MIN_MODEM, \
    .disable_11b_rate = false, \
    .magic = WIFI_CMD_INIT_MAGIC_DEPRECATED_APP, \
}

#define WIFI_CMD_INITIALIZE_CONFIG_DEFAULT() { \
    .wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT(), \
    .magic = WIFI_CMD_INIT_MAGIC, \
}

typedef struct {
    int bcn_timeout;
    int sta_disconnected;
} wifi_cmd_wifi_count_t;
#define app_wifi_count_t  wifi_cmd_wifi_count_t

/* Scan info level */
#define SCAN_SHOW_COUNT_ONLY        0         /**< Display only the count of scanned aps */
#define SCAN_SHOW_BASIC_INFO      BIT(0)      /**< Display basic information: SSID, BSSID, RSSI, channel */
#define SCAN_SHOW_DETAIL_INFO     BIT(1)      /**< Display detailed information: second channel, protocol, cbw, VHT frequency */
#define SCAN_SHOW_AX_INFO         BIT(2)      /**< Whether to include 802.11ax-specific information */
/* TODO: Future extensions: e.g., FTM, country, cipher, etc. */
// #define SCAN_SHOW_FTM_INFO        BIT(3)
// #define SCAN_SHOW_COUNTRY_INFO    BIT(4)
// #define SCAN_SHOW_CIPHYER_INFO    BIT(5)
#define SCAN_INFO_LEVEL_0  SCAN_SHOW_COUNT_ONLY
#define SCAN_INFO_LEVEL_1  SCAN_SHOW_BASIC_INFO
#define SCAN_INFO_LEVEL_2  SCAN_SHOW_BASIC_INFO | SCAN_SHOW_DETAIL_INFO
#define SCAN_INFO_LEVEL_3  SCAN_SHOW_BASIC_INFO | SCAN_SHOW_DETAIL_INFO | SCAN_SHOW_AX_INFO

typedef enum {
    WIFI_CMD_STATUS_NONE = 0,
    WIFI_CMD_STATUS_INIT,
    WIFI_CMD_STATUS_STARTED,
} wifi_cmd_status_t;

/**
 * Variables
 */
extern wifi_cmd_config_t g_wifi_cmd_config;
extern wifi_cmd_wifi_count_t g_wcount;
#if !CONFIG_WIFI_CMD_NO_LWIP
#if CONFIG_ESP_WIFI_SOFTAP_SUPPORT
extern esp_netif_t *g_netif_ap;
#endif
extern esp_netif_t *g_netif_sta;
#endif /* !CONFIG_WIFI_CMD_NO_LWIP */
extern bool g_is_sta_wifi_connected;
extern bool g_is_sta_got_ip4;
extern uint8_t g_wifi_cmd_scan_info_level;

/**
 * @brief This function is a combination of "wifi init" and "wifi start". This function is needed for wifi-cmd to enable handlers and set status.
 *
 * @param[in] config Specifies the WiFi initialization configuration that differs from the sdkconfig default, can be NULL.
 *                   wifi-cmd will save this configuration globally and use it during "wifi restart".
 */
void wifi_cmd_initialize_wifi(wifi_cmd_initialize_cfg_t *config);

/**
 * @brief Register command: wifi [init|deinit|start|stop|status]
 */
void wifi_cmd_register_wifi_init_deinit(void);

/**
 * @brief Register command: wifi_event [register|unregister]
 */
void wifi_cmd_register_wifi_event_handler(void);

/**
 * @brief Register: wifi_txpower
 */
void wifi_cmd_register_wifi_txpower(void);

/**
 * @brief Register commands: wifi, wifi_count, wifi_mode, wifi_protocol, wifi_bandwidth, wifi_ps.
 */
void wifi_cmd_register_base_basic(void);

/**
 * @brief Register commands: sta_connect, sta_disconnect, sta_scan
 */
void wifi_cmd_register_sta_basic(void);

/**
 * @brief Register commands: ap_set
 */
void wifi_cmd_register_ap_basic(void);

/**
 * @brief Register: base/sta/ap base commands
 */
void wifi_cmd_register_all_basic(void);

/**
 * @brief Register: all supported wifi commands
 */
esp_err_t wifi_cmd_register_all(void);

/* ----------------------------------------- Optional/Enhanced commands -------------------------------------------- */
#if WIFI_CMD_BAND_ENABLED
/**
 * @brief Register commands: wifi_band
 */
void wifi_cmd_register_wifi_band(void);
#endif

#if WIFI_CMD_ITWT_ENABLED
/**
 * @brief Register commands: itwt, probe
 */
void wifi_cmd_register_itwt(void);
#endif /* WIFI_CMD_ITWT_ENABLED */

#if CONFIG_WIFI_CMD_ENABLE_WIFI_STATS
/**
 * @brief Register commands: txstats, rxstats
 */
void wifi_cmd_register_stats(void);
#endif /* CONFIG_WIFI_CMD_ENABLE_WIFI_STATS */

#if WIFI_CMD_HE_DEBUG_ENABLED
/**
 * @brief Register: wifi he debug related commands.
 */
void wifi_cmd_register_he_debug(void);
#endif /* WIFI_CMD_HE_DEBUG_ENABLED */

#if !CONFIG_WIFI_CMD_BASIC_ONLY
void wifi_cmd_register_wifi_config_query(void);
#endif /* !CONFIG_WIFI_CMD_BASIC_ONLY */

/* --------------------------------------------- low level -------------------------------------------------------- */

/**
 * @brief run sta_connect with given config (and set status inside wifi-cmd)
 */
esp_err_t wifi_cmd_sta_connect(wifi_config_t* cfg, bool reconnect);

/**
 * @brief run sta_disconnect (and set status inside wifi-cmd)
 */
esp_err_t wifi_cmd_sta_disconnect(void);

/**
 * @brief esp_wifi_init() and set wifi event handlers. wifi_cmd_initialize_wifi() internally calls wifi_cmd_wifi_init() and wifi_cmd_wifi_start() sequentially.
 */
esp_err_t wifi_cmd_wifi_init(wifi_cmd_initialize_cfg_t *config);

/**
* @brief esp_wifi_start() and set wifi-cmd status
*/
esp_err_t wifi_cmd_wifi_start(void);

/* --------------------------------------------- Internal ---------------------------------------------------------- */

typedef struct {
    uint8_t phy_11b;
    uint8_t phy_11g;
    uint8_t phy_11n;
    uint8_t phy_lr;
    uint8_t phy_11a;
    uint8_t phy_11ac;
    uint8_t phy_11ax;
} wifi_phy_support_info_t;

extern int g_wifi_connect_retry_cnt;
uint8_t wifi_cmd_auth_mode_str2num(const char *auth_str);
const char *wifi_cmd_auth_mode_num2str(uint8_t value);
wifi_interface_t app_wifi_interface_str2ifx(const char *interface);
uint32_t wifi_cmd_protocol_str2bitmap(const char *protocol_str);
const char *wifi_cmd_protocol_bitmap2str(const uint32_t bitmap);
wifi_mode_t app_wifi_mode_str2num(const char *mode_str);
const char *app_wifi_mode_num2str(const wifi_mode_t mode);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 1)
const char *wifi_cmd_ap_bandwidth_to_str(wifi_bandwidth_t bandwidth);
#endif
#if WIFI_PHY_MODE_SUPPORTED
const char *wifi_cmd_phy_mode_num2str(wifi_phy_mode_t phymode);
#endif
void wifi_cmd_query_wifi_info(void);
esp_err_t wifi_cmd_str2mac(const char *str, uint8_t *mac_addr);
#if WIFI_CMD_5G_SUPPORTED
uint32_t wifi_cmd_channel_str2bitmsk(wifi_band_t band, const char *channel_list_str);
const char *wifi_cmd_channel_bitmsk2str(wifi_band_t band, uint32_t bitmap);
#endif
uint32_t wifi_cmd_get_phy_bitmap(const wifi_phy_support_info_t *phy_support_info);
void wifi_cmd_register_wifi_protocol(void);
void wifi_cmd_register_wifi_bandwidth(void);
void wifi_cmd_register_wifi_count(void);

#endif  /* CONFIG_ESP_WIFI_ENABLED */

#ifdef __cplusplus
}
#endif
