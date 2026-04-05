/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "wifi_cmd.h"

#ifndef APP_TAG
#define APP_TAG "WIFI"
#endif

#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
typedef struct {
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_str *bssid;
    struct arg_int *channel;
    struct arg_lit *no_disconnect;
    struct arg_lit *no_reconnect;
    struct arg_lit *full_scan;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 1) /* IDF commit: 52b24918f01a2e90f395f4056050511516aa0d6e */
    struct arg_int *failure_retry;
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0) && !CONFIG_WIFI_CMD_BASIC_ONLY
    struct arg_int *rssi_5g_offset;
#endif
#if WIFI_CMD_STA_ROAM_SUPPORTED
    /* roaming related options */
    struct arg_lit *rm_enabled;
    struct arg_lit *btm_enabled;
    struct arg_lit *mbo_enabled;
    struct arg_lit *ft_enabled;
    struct arg_lit *roam_enabled; /* enable all raming related options: rm,btm,mbo,ft */
#endif /* WIFI_CMD_STA_ROAM_SUPPORTED */
#if WIFI_CMD_STA_OWE_SUPPORTED
    struct arg_lit *owe_enabled;
#endif
    struct arg_lit *transition_disable;
    struct arg_int *sae_pwe;
    struct arg_lit *disable_pmf_capable;
    struct arg_lit *pmf_required;
    struct arg_str *threshold_auth;
    struct arg_int *threshold_rssi;
    struct arg_int *sort_method;
    struct arg_int *listen_interval;
#if WIFI_CMD_STA_SAE_PK_SUPPORTED
    struct arg_int *sae_pk_mode;
#endif
#if WIFI_CMD_STA_SAE_PASSWORD_SUPPORTED
    struct arg_str *sae_h2e_identifire;
#endif
#if WIFI_CMD_HE_SUPPORTED
    struct arg_lit *he_mcs9;
    struct arg_lit *he_dcm_set;
    struct arg_int *he_dcm_max_constellation_tx;
    struct arg_int *he_dcm_max_constellation_rx;
    struct arg_lit *he_su_beamformee_disabled;
    struct arg_lit *he_trig_su_bmforming_feedback_disabled;
    struct arg_lit *he_trig_mu_bmforming_partial_feedback_disabled;
    struct arg_lit *he_trig_cqi_feedback_disabled;
#endif
#if WIFI_CMD_5G_SUPPORTED
    struct arg_lit *vht_mcs8;
    struct arg_lit *vht_su_beamformee_disabled;
    struct arg_lit *vht_mu_beamformee_disabled;
#endif
    struct arg_lit *no_reconfig;
#if WIFI_CMD_WPA3_COMPATIBLE_SUPPORTED
    struct arg_lit *disable_wpa3_compatible;
#endif
    struct arg_end *end;
} sta_connect_args_t;
static sta_connect_args_t sta_connect_args;

typedef struct {
    struct arg_str *ssid;
    struct arg_str *bssid;
    struct arg_int *channel;
    struct arg_lit *show_hidden;
    /* active scan */
    struct arg_int *max;
    struct arg_int *min;
    /* passive scan */
    struct arg_lit *passive;
    struct arg_int *passive_time;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    struct arg_int *home_chan_dwell_time;
    /* scan bitmap */
    struct arg_int *bitmap_2g;
    struct arg_int *bitmap_5g;
#endif
    /* do not show scan details */
    struct arg_lit *count_only;
    struct arg_lit *ax_info;
    struct arg_lit *verbose;
    struct arg_end *end;
} sta_scan_args_t;
static sta_scan_args_t scan_args;

esp_err_t wifi_cmd_sta_connect(wifi_config_t* cfg, bool reconnect)
{
    /* clear auto-reconnect retry count */
    g_wifi_connect_retry_cnt = 0;
    g_wifi_cmd_config.reconnect = reconnect;
    esp_err_t err;

    err = esp_wifi_set_config(WIFI_IF_STA, cfg);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_wifi_connect();
    return err;
}

static esp_err_t wifi_cmd_sta_connect_with_pmf_disable(wifi_config_t* cfg, bool reconnect)
{
    /* clear auto-reconnect retry count */
    g_wifi_connect_retry_cnt = 0;
    g_wifi_cmd_config.reconnect = reconnect;

    esp_err_t err;

    err = esp_wifi_set_config(WIFI_IF_STA, cfg);
    if (err != ESP_OK) {
        return err;
    }
    if (esp_wifi_disable_pmf_config(WIFI_IF_STA) != ESP_OK) {
        ESP_LOGE(APP_TAG, "Failed to disable PMF config");
        return ESP_FAIL;
    }
    err = esp_wifi_connect();
    return err;
}
esp_err_t wifi_cmd_sta_disconnect()
{
    g_wifi_cmd_config.reconnect = false;
    esp_err_t err = esp_wifi_disconnect();
    return err;

}

static int cmd_do_sta_connect(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &sta_connect_args);

    if (nerrors != 0) {
        arg_print_errors(stderr, sta_connect_args.end, argv[0]);
        return 1;
    }

    esp_err_t err = ESP_FAIL;

    wifi_config_t wifi_config = {
        .sta = {
            .scan_method = sta_connect_args.full_scan->count > 0 ? WIFI_ALL_CHANNEL_SCAN : WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
        },
    };

    /* use nvs config to connect, ignore all other parameters */
    if (sta_connect_args.no_reconfig->count > 0) {
        g_wifi_connect_retry_cnt = 0;
        g_wifi_cmd_config.reconnect = true;
        err = esp_wifi_connect();
        LOG_WIFI_CMD_DONE(err, "WIFI_CONNECT_START");
        return 0;
    }

    /* ssid must be set */
    if (sta_connect_args.ssid->count == 0) {
        ESP_LOGE(APP_TAG, "ssid must be set");
        return 1;
    }
    const char *ssid = sta_connect_args.ssid->sval[0];
    memcpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    const char *pass = sta_connect_args.password->sval[0];
    if (sta_connect_args.password->count > 0) {
        memcpy((char *) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WEP;
    }
    if (sta_connect_args.channel->count > 0) {
        wifi_config.sta.channel = (uint8_t)(sta_connect_args.channel->ival[0]);
    }
    if (sta_connect_args.bssid->count > 0) {
        if (wifi_cmd_str2mac(sta_connect_args.bssid->sval[0], wifi_config.sta.bssid) != ESP_OK) {
            ESP_LOGE(APP_TAG, "Can not parse bssid: %s", sta_connect_args.bssid->sval[0]);
            return 1;
        }
        wifi_config.sta.bssid_set = 1;
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0) && !CONFIG_WIFI_CMD_BASIC_ONLY
    if (sta_connect_args.rssi_5g_offset->count > 0) {
        wifi_config.sta.threshold.rssi_5g_adjustment = (uint8_t)(sta_connect_args.rssi_5g_offset->ival[0]);
    } else {
        wifi_config.sta.threshold.rssi_5g_adjustment = DEF_5G_CONN_RSSI_OFFSET;
    }
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 1) /* IDF commit: 52b24918f01a2e90f395f4056050511516aa0d6e */
    if (sta_connect_args.failure_retry->count > 0) {
        wifi_config.sta.failure_retry_cnt = sta_connect_args.failure_retry->ival[0];
    }
#endif

#if WIFI_CMD_STA_ROAM_SUPPORTED
    if (sta_connect_args.rm_enabled->count > 0) {
        wifi_config.sta.rm_enabled = 1;
    }
    if (sta_connect_args.btm_enabled->count > 0) {
        wifi_config.sta.btm_enabled = 1;
    }
    if (sta_connect_args.mbo_enabled->count > 0) {
        wifi_config.sta.mbo_enabled = 1;
    }
    if (sta_connect_args.ft_enabled->count > 0) {
        wifi_config.sta.ft_enabled = 1;
    }
    if (sta_connect_args.roam_enabled->count > 0) {
        wifi_config.sta.rm_enabled = 1;
        wifi_config.sta.btm_enabled = 1;
        wifi_config.sta.mbo_enabled = 1;
        wifi_config.sta.ft_enabled = 1;
    }
#endif /* WIFI_CMD_STA_ROAM_SUPPORTED */
#if WIFI_CMD_STA_OWE_SUPPORTED
    if (sta_connect_args.owe_enabled->count > 0) {
        wifi_config.sta.owe_enabled = 1;
    }
#endif

    /* Try to disconnect from the previous AP before connect. */
    if (sta_connect_args.no_disconnect->count == 0) {
        esp_wifi_disconnect();
    }
    bool reconnect = true;
    if (sta_connect_args.no_reconnect->count > 0) {
        reconnect = false;
    }

    if (sta_connect_args.transition_disable->count > 0) {
        wifi_config.sta.transition_disable = 1;
    }

    if (sta_connect_args.sae_pwe->count > 0) {
        wifi_config.sta.sae_pwe_h2e = sta_connect_args.sae_pwe->ival[0];
    }

    if (sta_connect_args.disable_pmf_capable->count > 0) {
        wifi_config.sta.pmf_cfg.capable = false;
    } else {
        wifi_config.sta.pmf_cfg.capable = true;
    }

    if (sta_connect_args.pmf_required->count > 0) {
        wifi_config.sta.pmf_cfg.required = true;
    } else {
        wifi_config.sta.pmf_cfg.required = false;
    }

    if (sta_connect_args.listen_interval->count > 0) {
        wifi_config.sta.listen_interval = sta_connect_args.listen_interval->ival[0];
    }

    if (sta_connect_args.sort_method->count > 0) {
        wifi_config.sta.sort_method = sta_connect_args.sort_method->ival[0];
    }

#if WIFI_CMD_HE_SUPPORTED
    if (sta_connect_args.he_mcs9->count > 0) {
        wifi_config.sta.he_mcs9_enabled = 1;
    }
    if (sta_connect_args.he_dcm_set->count > 0) {
        wifi_config.sta.he_dcm_set = 1;
    }
    if (sta_connect_args.he_dcm_max_constellation_tx->count > 0) {
        wifi_config.sta.he_dcm_max_constellation_tx = sta_connect_args.he_dcm_max_constellation_tx->ival[0];
    }
    if (sta_connect_args.he_dcm_max_constellation_rx->count > 0) {
        wifi_config.sta.he_dcm_max_constellation_rx = sta_connect_args.he_dcm_max_constellation_rx->ival[0];
    }
    if (sta_connect_args.he_su_beamformee_disabled->count > 0) {
        wifi_config.sta.he_su_beamformee_disabled = 1;
    }
    if (sta_connect_args.he_trig_su_bmforming_feedback_disabled->count > 0) {
        wifi_config.sta.he_trig_su_bmforming_feedback_disabled = 1;
    }
    if (sta_connect_args.he_trig_mu_bmforming_partial_feedback_disabled->count > 0) {
        wifi_config.sta.he_trig_mu_bmforming_partial_feedback_disabled = 1;
    }
    if (sta_connect_args.he_trig_cqi_feedback_disabled->count > 0) {
        wifi_config.sta.he_trig_cqi_feedback_disabled = 1;
    }
#endif
#if WIFI_CMD_5G_SUPPORTED
    if (sta_connect_args.vht_mcs8->count > 0) {
        wifi_config.sta.vht_mcs8_enabled = 1;
    }
    if (sta_connect_args.vht_su_beamformee_disabled->count > 0) {
        wifi_config.sta.vht_su_beamformee_disabled = 1;
    }
    if (sta_connect_args.vht_mu_beamformee_disabled->count > 0) {
        wifi_config.sta.vht_mu_beamformee_disabled = 1;
    }
#endif

#if WIFI_CMD_STA_SAE_PK_SUPPORTED
    if (sta_connect_args.sae_pk_mode->count > 0) {
        wifi_config.sta.sae_pk_mode = sta_connect_args.sae_pk_mode->ival[0];
    }
#endif
#if WIFI_CMD_STA_SAE_PASSWORD_SUPPORTED
    const char *identifier = sta_connect_args.sae_h2e_identifire->sval[0];
    if (sta_connect_args.sae_h2e_identifire->count > 0) {
        memcpy((char *) wifi_config.sta.sae_h2e_identifier, identifier, sizeof(wifi_config.sta.sae_h2e_identifier));
    }
#endif
    const char *threshold_auth = sta_connect_args.threshold_auth->sval[0];
    if (sta_connect_args.threshold_auth->count > 0) {
        wifi_config.sta.threshold.authmode = wifi_cmd_auth_mode_str2num(threshold_auth);
    }
    if (sta_connect_args.threshold_rssi->count > 0) {
        wifi_config.sta.threshold.rssi = sta_connect_args.threshold_rssi->ival[0];
    }
#if WIFI_CMD_WPA3_COMPATIBLE_SUPPORTED
    if (sta_connect_args.disable_wpa3_compatible->count > 0) {
        wifi_config.sta.disable_wpa3_compatible_mode = 1;
        ESP_LOGI(APP_TAG, "disable wpa3 compatible mode");
    }
#endif
    ESP_LOGI(APP_TAG, "Connecting to %s...", ssid);
    if (!wifi_config.sta.pmf_cfg.capable) {
        err = wifi_cmd_sta_connect_with_pmf_disable(&wifi_config, reconnect);
        LOG_WIFI_CMD_DONE(err, "WIFI_CONNECT_START");
        return 0;
    }
    err = wifi_cmd_sta_connect(&wifi_config, reconnect);
    LOG_WIFI_CMD_DONE(err, "WIFI_CONNECT_START");
    return 0;
}

static int cmd_do_sta_disconnect(int argc, char **argv)
{
    esp_err_t err = wifi_cmd_sta_disconnect();
    LOG_WIFI_CMD_DONE(err, "WIFI_DISCONNECT");
    return 0;
}

#if CONFIG_WIFI_CMD_ENABLE_DEPRECATED
static int cmd_do_sta_deprecated(int argc, char **argv)
{
    ESP_LOGW(APP_TAG, "'sta' is deprecated, please use 'sta_connect'.");
    return cmd_do_sta_connect(argc, argv);
}
static int cmd_do_disconnect_deprecated(int argc, char **argv)
{
    ESP_LOGW(APP_TAG, "'disconnect' is deprecated, please use 'sta_disconnect'.");
    return cmd_do_sta_disconnect(argc, argv);
}
#endif

static void wifi_cmd_register_sta_connect(void)
{
    if (!sta_connect_args.end) {
        sta_connect_args.ssid = arg_str0(NULL, NULL, "<ssid>", "SSID of AP");
        sta_connect_args.password = arg_str0(NULL, NULL, "<pass>", "password of AP");
        sta_connect_args.bssid = arg_str0("b", "bssid", "<bssid>", "bssid of AP");
        sta_connect_args.channel = arg_int0("n", "channel", "<channel>", "channel of AP");
        sta_connect_args.transition_disable = arg_lit0(NULL, "transition_disable", "enable transition disable");
        sta_connect_args.no_disconnect = arg_lit0(NULL, "no-disconnect", "For test: do not run esp_wifi_disconnect before connect.");
        sta_connect_args.no_reconnect = arg_lit0(NULL, "no-reconnect", "Disable auto-reconnect in wifi disconnect handler.");
        sta_connect_args.full_scan = arg_lit0(NULL, "full-scan", "do full channel scan during sta connect.");
        sta_connect_args.threshold_rssi = arg_int0(NULL, "rssi", "<rssi>", "rssi threshold");
        sta_connect_args.listen_interval = arg_int0(NULL, "listen_interval", "<listen_interval>", "listen interval");
        sta_connect_args.sort_method = arg_int0(NULL, "sort_method", "<sort_method>", "sort method");
        sta_connect_args.threshold_auth = arg_str0(NULL, "auth", "<auth>", "auth threshold");
        sta_connect_args.no_reconfig = arg_lit0(NULL, "no-reconfig", "sta connect with nvs config, ignore all other parameters");
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0) && !CONFIG_WIFI_CMD_BASIC_ONLY
        sta_connect_args.rssi_5g_offset = arg_int0(NULL, "5g-offset", "<rssi_5g_offset>", "offset value for 5g ssid when do full scan connect");
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 1) /* IDF commit: 52b24918f01a2e90f395f4056050511516aa0d6e */
        sta_connect_args.failure_retry = arg_int0(NULL, "failure_retry", "<int>", "connect sta config: failure_retry_cnt");
#endif
#if WIFI_CMD_STA_ROAM_SUPPORTED
        sta_connect_args.rm_enabled = arg_lit0(NULL, "rm", "rrm enabled");
        sta_connect_args.btm_enabled = arg_lit0(NULL, "btm", "btm enabled");
        sta_connect_args.mbo_enabled = arg_lit0(NULL, "mbo", "mbo enabled");
        sta_connect_args.ft_enabled = arg_lit0(NULL, "ft", "ft enabled");
        sta_connect_args.roam_enabled = arg_lit0(NULL, "roam", "enable all roaming related options: rm,btm,mbo,ft");
#endif /* WIFI_CMD_STA_ROAM_SUPPORTED */
#if WIFI_CMD_STA_OWE_SUPPORTED
        sta_connect_args.owe_enabled = arg_lit0(NULL, "owe", "owe enabled");
#endif
        sta_connect_args.sae_pwe = arg_int0(NULL, "sae_pwe", "<sae pwe method>", "sae pwe method");
        sta_connect_args.disable_pmf_capable = arg_lit0(NULL, "disable_pmf", "disable pmf capable");
        sta_connect_args.pmf_required = arg_lit0(NULL, "pmf_required", "pmf required");
#if WIFI_CMD_STA_SAE_PASSWORD_SUPPORTED
        sta_connect_args.sae_h2e_identifire = arg_str0(NULL, "h2e_identifier", "<str>", "h2e identifier password");
#endif
#if WIFI_CMD_STA_SAE_PK_SUPPORTED
        sta_connect_args.sae_pk_mode = arg_int0(NULL, "sae_pk_mode", "<sae pk mode>", "sae pk mode");
#endif
#if WIFI_CMD_HE_SUPPORTED
        sta_connect_args.he_mcs9 = arg_lit0(NULL, "enable_he_mcs9", "enable HE MCS8-9");
        sta_connect_args.he_dcm_set = arg_lit0(NULL, "he_dcm", "enable HE DCM");
        sta_connect_args.he_dcm_max_constellation_tx = arg_int0(NULL, "dcm_tx", "<int>", "he dcm max constellation tx");
        sta_connect_args.he_dcm_max_constellation_rx = arg_int0(NULL, "dcm_rx", "<int>", "he dcm max constellation rx");
        sta_connect_args.he_su_beamformee_disabled = arg_lit0(NULL, "su_bf_dis", "he su bmforming disabled");
        sta_connect_args.he_trig_su_bmforming_feedback_disabled = arg_lit0(NULL, "trig_su_bf_fb_dis", "he trig su bmforming feedback disabled");
        sta_connect_args.he_trig_mu_bmforming_partial_feedback_disabled = arg_lit0(NULL, "trig_mu_bf_part_fb_dis", "he trig mu bmforming partical feedback disabled");
        sta_connect_args.he_trig_cqi_feedback_disabled = arg_lit0(NULL, "trig_cqi_fb_dis", "he trig cqi feedback disabled");
#endif
#if WIFI_CMD_5G_SUPPORTED
        sta_connect_args.vht_mcs8 = arg_lit0(NULL, "enable_vht_mcs8", "enable VHT MCS8");
        sta_connect_args.vht_su_beamformee_disabled = arg_lit0(NULL, "vht_su_bf_dis", "vht su beamformee disabled");
        sta_connect_args.vht_mu_beamformee_disabled = arg_lit0(NULL, "vht_mu_bf_dis", "vht mu beamformee disabled");
#endif
#if WIFI_CMD_WPA3_COMPATIBLE_SUPPORTED
        sta_connect_args.disable_wpa3_compatible = arg_lit0(NULL, "disable_wpa3_compatible", "disable wpa3 compatible");
#endif
        sta_connect_args.end = arg_end(2);
    }
    const esp_console_cmd_t sta_connect_cmd = {
        .command = "sta_connect",
        .help = "WiFi is station mode, connect specified soft-AP",
        .hint = NULL,
        .func = &cmd_do_sta_connect,
        .argtable = &sta_connect_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&sta_connect_cmd));

#if CONFIG_WIFI_CMD_ENABLE_DEPRECATED
    esp_console_cmd_t sta_deprecated = sta_connect_cmd;
    sta_deprecated.command = "sta";
    sta_deprecated.help = "Station mode, connect to specific AP. Deprecated, please use 'sta_connect'";
    sta_deprecated.func = &cmd_do_sta_deprecated;
    ESP_ERROR_CHECK(esp_console_cmd_register(&sta_deprecated));
#endif
}

static void wifi_cmd_register_sta_disconnect(void)
{
    const esp_console_cmd_t sta_disconnect_cmd = {
        .command = "sta_disconnect",
        .help = "WiFi is station mode, disconnect from current AP or stop reconnect.",
        .hint = NULL,
        .func = &cmd_do_sta_disconnect,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&sta_disconnect_cmd));

#if CONFIG_WIFI_CMD_ENABLE_DEPRECATED
    esp_console_cmd_t disconnect_deprecated = sta_disconnect_cmd;
    disconnect_deprecated.command = "disconnect";
    disconnect_deprecated.help = "stop wifi connect. Deprecated, please use 'sta_disconnect'";
    disconnect_deprecated.func = &cmd_do_disconnect_deprecated;
    ESP_ERROR_CHECK(esp_console_cmd_register(&disconnect_deprecated));
#endif
}

static int cmd_do_sta_scan(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &scan_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, scan_args.end, argv[0]);
        return 1;
    }
    uint8_t ssid[33] = {0};

    wifi_scan_config_t scan_config = {0};
    if (scan_args.ssid->count > 0) {
        strlcpy((char *)ssid, scan_args.ssid->sval[0], 33);
        scan_config.ssid = ssid;
    }
    if (scan_args.bssid->count > 0) {
        if (wifi_cmd_str2mac(scan_args.bssid->sval[0], scan_config.bssid) != ESP_OK) {
            ESP_LOGE(APP_TAG, "Can not parse bssid: %s", scan_args.bssid->sval[0]);
            return 1;
        }
    }
    if (scan_args.channel->count > 0) {
        scan_config.channel = (uint8_t)(scan_args.channel->ival[0]);
    }
    if (scan_args.show_hidden->count > 0) {
        scan_config.show_hidden = true;
    }
    /* passive scan */
    if (scan_args.passive->count > 0) {
        scan_config.scan_type = WIFI_SCAN_TYPE_PASSIVE;
    }
    if (scan_args.passive_time->count > 0) {
        scan_config.scan_time.passive = scan_args.passive_time->ival[0];
        scan_config.scan_type = WIFI_SCAN_TYPE_PASSIVE;
    }

    if (scan_config.scan_type == WIFI_SCAN_TYPE_PASSIVE && scan_args.max->count + scan_args.min->count) {
        ESP_LOGE(APP_TAG, "Can not set active scan and passive scan at the same time");
        return 1;
    }
    /* active scan time */
    if (scan_args.min->count > 0) {
        scan_config.scan_time.active.min = scan_args.min->ival[0];
    }
    if (scan_args.max->count > 0) {
        scan_config.scan_time.active.max = scan_args.max->ival[0];
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    if (scan_args.home_chan_dwell_time->count > 0) {
        scan_config.home_chan_dwell_time = scan_args.home_chan_dwell_time->ival[0];
    }
    /* bitmap */
    if (scan_args.bitmap_2g->count > 0) {
        scan_config.channel_bitmap.ghz_2_channels = (uint16_t)scan_args.bitmap_2g->ival[0];
    }
    if (scan_args.bitmap_5g->count > 0) {
        scan_config.channel_bitmap.ghz_5_channels = (uint32_t)scan_args.bitmap_5g->ival[0];
    }
#endif

    g_wifi_cmd_scan_info_level = SCAN_INFO_LEVEL_1;
    if (scan_args.count_only->count > 0) {
        g_wifi_cmd_scan_info_level = SCAN_INFO_LEVEL_0;
    } else {
        if (scan_args.verbose->count == 1) {
            g_wifi_cmd_scan_info_level = SCAN_INFO_LEVEL_2;
        } else if (scan_args.verbose->count == 2) {
            g_wifi_cmd_scan_info_level = SCAN_INFO_LEVEL_3;
        }
        if (scan_args.ax_info->count > 0) {
            g_wifi_cmd_scan_info_level |= SCAN_SHOW_AX_INFO;
        }
    }
    esp_err_t err = esp_wifi_scan_start(&scan_config, false);
    LOG_WIFI_CMD_DONE(err, "STA_SCAN_START");
    return 0;
}

#if CONFIG_WIFI_CMD_ENABLE_DEPRECATED
static int cmd_do_wifi_scan_deprecated(int argc, char **argv)
{
    ESP_LOGW(APP_TAG, "'wifi_scan' is deprecated, please use 'sta_scan'.");
    return cmd_do_sta_scan(argc, argv);
}
static int cmd_do_scan_deprecated(int argc, char **argv)
{
    ESP_LOGW(APP_TAG, "'scan' is deprecated, please use 'sta_scan'.");
    return cmd_do_sta_scan(argc, argv);
}
#endif

static void wifi_cmd_register_sta_scan(void)
{
    if (!scan_args.end) {
        scan_args.ssid = arg_str0(NULL, NULL, "<ssid>", "SSID of AP");
        scan_args.bssid = arg_str0("b", "bssid", "<bssid>", "bssid of AP");
        scan_args.channel = arg_int0("n", "channel", "<int>", "channel of AP");
        scan_args.show_hidden = arg_lit0("h", "show-hidden", "Show hidden APs");
        scan_args.max = arg_int0(NULL, "max", "<int>", "max active scan time");
        scan_args.min = arg_int0(NULL, "min", "<int>", "min active scan time");
        scan_args.passive = arg_lit0(NULL, "passive", "Set scan type to passive");
        scan_args.passive_time = arg_int0(NULL, "passive-time", "<int>", "Set passive scan time, also set scan type to passive");
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
        scan_args.home_chan_dwell_time = arg_int0(NULL, "dwell", "<int>", "home channel dwell time (ms)");
        scan_args.bitmap_2g = arg_int0("2", "bitmap-2g", "<int/hex>", "scan bitmap 2ghz, eg: '-2 0x842'(channel:1/6/11)");
        scan_args.bitmap_5g = arg_int0("5", "bitmap-5g", "<int/hex>", "scan bitmap 5ghz, eg: '-5 0x6'(channel:36/40)");
#endif
        scan_args.count_only = arg_lit0(NULL, "count-only", "For test: do scan ap count only.");
        scan_args.ax_info = arg_lit0(NULL, "ax", "Show ax info or not.");
        scan_args.verbose = arg_litn("v", NULL, 0, 2, "Scan result info level.");
        scan_args.end = arg_end(2);
    }
    const esp_console_cmd_t sta_scan_cmd = {
        .command = "sta_scan",
        .help = "WiFi is station mode, scan APs",
        .hint = NULL,
        .func = &cmd_do_sta_scan,
        .argtable = &scan_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&sta_scan_cmd));

#if CONFIG_WIFI_CMD_ENABLE_DEPRECATED
    esp_console_cmd_t scan_deprecated = sta_scan_cmd;
    scan_deprecated.command = "wifi_scan";
    scan_deprecated.help = "Scan APs. Deprecated, please use 'sta_scan'";
    scan_deprecated.func = &cmd_do_wifi_scan_deprecated;
    ESP_ERROR_CHECK(esp_console_cmd_register(&scan_deprecated));
    scan_deprecated.command = "scan";
    scan_deprecated.func = &cmd_do_scan_deprecated;
    ESP_ERROR_CHECK(esp_console_cmd_register(&scan_deprecated));
#endif
}

void wifi_cmd_register_sta_basic(void)
{
    wifi_cmd_register_sta_connect();
    wifi_cmd_register_sta_disconnect();
    wifi_cmd_register_sta_scan();
}
