/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_wifi.h"

#include "wifi_cmd.h"

#ifndef APP_TAG
#define APP_TAG "WIFI"
#endif

#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]

static int wifi_sta_config_query(void)
{
    wifi_config_t w_config = {};
    esp_wifi_get_config(WIFI_IF_STA, &w_config);
    ESP_LOGI(APP_TAG, "STA_CONFIG:");
    ESP_LOGI(APP_TAG, " SSID:%s,", w_config.sta.ssid);
    ESP_LOGI(APP_TAG, " PASSWORD:%s,", w_config.sta.password);
    ESP_LOGI(APP_TAG, " SCAN_METHOD:%d,",  w_config.sta.scan_method);
    ESP_LOGI(APP_TAG, " BSSID:"MACSTR",",  MAC2STR(w_config.sta.bssid));
    ESP_LOGI(APP_TAG, " BSSID_SET:%d,",  w_config.sta.bssid_set);
    ESP_LOGI(APP_TAG, " CHANNEL:%d,",  w_config.sta.channel);
    ESP_LOGI(APP_TAG, " LISTEN_INTERVAL:%d,",  w_config.sta.listen_interval);
    ESP_LOGI(APP_TAG, " SORT_METHOD:%d,",  w_config.sta.sort_method);
    ESP_LOGI(APP_TAG, " AUTH_MODE_THRESHOLD:%s,",  wifi_cmd_auth_mode_num2str(w_config.sta.threshold.authmode));
    ESP_LOGI(APP_TAG, " RSSI_THRESHOLD:%d,",  w_config.sta.threshold.rssi);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0) && !CONFIG_WIFI_CMD_BASIC_ONLY
    ESP_LOGI(APP_TAG, " RSSI_5G_ADJUSTMENT:%d,",  w_config.sta.threshold.rssi_5g_adjustment);
#endif
    ESP_LOGI(APP_TAG, " PMF_CAPABLE:%d,",  w_config.sta.pmf_cfg.capable);
    ESP_LOGI(APP_TAG, " PMF_REQUIRED:%d,",  w_config.sta.pmf_cfg.required);
#if WIFI_CMD_STA_ROAM_SUPPORTED
    ESP_LOGI(APP_TAG, " RM_ENABLED:%d,",  w_config.sta.rm_enabled);
    ESP_LOGI(APP_TAG, " BTM_ENABLED:%d,",  w_config.sta.btm_enabled);
    ESP_LOGI(APP_TAG, " MBO_ENABLED:%d,",  w_config.sta.mbo_enabled);
    ESP_LOGI(APP_TAG, " FT_ENABLED:%d,",  w_config.sta.ft_enabled);
#endif
#if WIFI_CMD_STA_OWE_SUPPORTED
    ESP_LOGI(APP_TAG, " OWE_ENABLED:%d,",  w_config.sta.owe_enabled);
#endif
    ESP_LOGI(APP_TAG, " TRANSITION_DISABLE:%d,",  w_config.sta.transition_disable);
#if WIFI_CMD_WPA3_COMPATIBLE_SUPPORTED
    ESP_LOGI(APP_TAG, " DISABLE_WPA3_COMPATIBLE_MODE:%d,",  w_config.sta.disable_wpa3_compatible_mode);
#endif
    ESP_LOGI(APP_TAG, " SAE_PWE:%d,",  w_config.sta.sae_pwe_h2e);
#if WIFI_CMD_STA_SAE_PK_SUPPORTED
    ESP_LOGI(APP_TAG, " SAE_PK_MODE:%d,",  w_config.sta.sae_pk_mode);
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 1) /* IDF commit: 52b24918f01a2e90f395f4056050511516aa0d6e */
    ESP_LOGI(APP_TAG, " FAILURE_RETRY_CNT:%d,",  w_config.sta.failure_retry_cnt);
#endif
#if WIFI_CMD_STA_SAE_PASSWORD_SUPPORTED
    ESP_LOGI(APP_TAG, " H2E_IDENTIFIER:%s,",  w_config.sta.sae_h2e_identifier);
#endif
#if WIFI_CMD_5G_SUPPORTED
    ESP_LOGI(APP_TAG, " VHT_MCS8_ENABLED:%d,",  w_config.sta.vht_mcs8_enabled);
    ESP_LOGI(APP_TAG, " VHT_MU_BEAMFORMEE_DISABLED:%d,",  w_config.sta.vht_mu_beamformee_disabled);
    ESP_LOGI(APP_TAG, " VHT_SU_BEAMFORMEE_DISABLED:%d,",  w_config.sta.vht_su_beamformee_disabled);
#endif
#if WIFI_CMD_HE_SUPPORTED
    ESP_LOGI(APP_TAG, " HE_DCM_SET:%d,",  w_config.sta.he_dcm_set);
    ESP_LOGI(APP_TAG, " HE_DCM_MAX_CONSTELLATION_TX:%d,",  w_config.sta.he_dcm_max_constellation_tx);
    ESP_LOGI(APP_TAG, " HE_DCM_MAX_CONSTELLATION_RX:%d,",  w_config.sta.he_dcm_max_constellation_rx);
    ESP_LOGI(APP_TAG, " HE_MCS9_ENABLED:%d,",  w_config.sta.he_mcs9_enabled);
    ESP_LOGI(APP_TAG, " HE_SU_BEAMFORMEE_DISABLED:%d,",  w_config.sta.he_su_beamformee_disabled);
    ESP_LOGI(APP_TAG, " HE_TRIG_SU_BMFORMING_FEEDBACK_DISABLED:%d,",  w_config.sta.he_trig_su_bmforming_feedback_disabled);
    ESP_LOGI(APP_TAG, " HE_TRIG_MU_BMFORMING_PARTIAL_FEEDBACK_DISABLED:%d,",  w_config.sta.he_trig_mu_bmforming_partial_feedback_disabled);
    ESP_LOGI(APP_TAG, " HE_TRIG_CQI_FEEDBACK_DISABLED:%d,",  w_config.sta.he_trig_cqi_feedback_disabled);
#endif

    return 0;
}

static int wifi_ap_config_query(void)
{
    wifi_config_t w_config = {};
    esp_wifi_get_config(WIFI_IF_AP, &w_config);
    ESP_LOGI(APP_TAG, "AP_CONFIG:");
    ESP_LOGI(APP_TAG, " SSID:%s,", w_config.ap.ssid);
    ESP_LOGI(APP_TAG, " PASSWORD:%s,", w_config.ap.password);
    ESP_LOGI(APP_TAG, " AUTH_MODE:%s,",  wifi_cmd_auth_mode_num2str(w_config.ap.authmode));
    ESP_LOGI(APP_TAG, " CHANNEL:%d,",  w_config.ap.channel);
    ESP_LOGI(APP_TAG, " SSID_HIDDEN:%d,",  w_config.ap.ssid_hidden);
    ESP_LOGI(APP_TAG, " MAX_CONNECTION:%d,",  w_config.ap.max_connection);
    ESP_LOGI(APP_TAG, " BEACON_INTERVAL:%d,",  w_config.ap.beacon_interval);
    ESP_LOGI(APP_TAG, " PAIRWISE_CIPHER:%d,",  w_config.ap.pairwise_cipher);
#if WIFI_CMD_AP_PMF_SUPPORTED
    ESP_LOGI(APP_TAG, " PMF_CAPABLE:%d,",  w_config.ap.pmf_cfg.capable);
    ESP_LOGI(APP_TAG, " PMF_REQUIRED:%d,",  w_config.ap.pmf_cfg.required);
#endif
#if CONFIG_ESP_WIFI_FTM_ENABLE
    ESP_LOGI(APP_TAG, " FTM_RESPONDER:%d,",  w_config.ap.ftm_responder);
#endif
#if WIFI_CMD_AP_DTIM_PERIOD_SUPPORTED
    ESP_LOGI(APP_TAG, " DTIM_PERIOD:%d,",  w_config.ap.dtim_period);
    ESP_LOGI(APP_TAG, " CSA_COUNT:%d,",  w_config.ap.csa_count);
#endif
#if WIFI_CMD_AP_MAX_BSS_IDLE_SUPPORTED
    ESP_LOGI(APP_TAG, " BSS_MAX_IDLE:%d,",  w_config.ap.bss_max_idle_cfg.period);
    ESP_LOGI(APP_TAG, " BSS_MAX_IDLE_PROTECT:%d,",  w_config.ap.bss_max_idle_cfg.protected_keep_alive);
#endif
#if WIFI_CMD_AP_SAE_PWE_SUPPORTED
    ESP_LOGI(APP_TAG, " SAE_PWE:%d,",  w_config.ap.sae_pwe_h2e);
#endif
#if WIFI_CMD_AP_SAE_EXT_SUPPORTED
    ESP_LOGI(APP_TAG, " SAE_EXT:%d,",  w_config.ap.sae_ext);
#endif
#if WIFI_CMD_AP_TRANSITION_DISABLE_SUPPORTED
    ESP_LOGI(APP_TAG, " TRANSITION_DISABLE:%d,",  w_config.ap.transition_disable);
#endif
#if WIFI_CMD_AP_GTK_REKEY_SUPPORTED
    ESP_LOGI(APP_TAG, " GTK_REKEY_INTERVAL:%d,",  w_config.ap.gtk_rekey_interval);
#endif
#if WIFI_CMD_WPA3_COMPATIBLE_SUPPORTED
    ESP_LOGI(APP_TAG, " WPA3_COMPATIBLE_MODE:%d,",  w_config.ap.wpa3_compatible_mode);
#endif
    return 0;
}

typedef struct {
    struct arg_str *action;
    struct arg_end *end;
} wifi_query_args_t;
wifi_query_args_t wifi_query_args;

static int cmd_do_wifi_config_query(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &wifi_query_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, wifi_query_args.end, argv[0]);
        return 1;
    }

    if (strcmp(wifi_query_args.action->sval[0], "sta") == 0) {
        wifi_sta_config_query();
    } else if (strcmp(wifi_query_args.action->sval[0], "ap") == 0) {
        wifi_ap_config_query();
    } else if (strcmp(wifi_query_args.action->sval[0], "all") == 0) {
        wifi_sta_config_query();
        wifi_ap_config_query();
    }
    return 0;
}

void wifi_cmd_register_wifi_config_query(void)
{
    if (!wifi_query_args.end) {
        wifi_query_args.action = arg_str1(NULL, NULL, "<action>", "sta; ap; all;");
        wifi_query_args.end = arg_end(2);
    }
    const esp_console_cmd_t wifi_query_cmd = {
        .command = "wifi_config_query",
        .help = "query wifi config info.",
        .hint = NULL,
        .func = &cmd_do_wifi_config_query,
        .argtable = &wifi_query_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&wifi_query_cmd));
}
