/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"
#include "wifi_cmd_config.h"
#if CONFIG_ESP_WIFI_SOFTAP_SUPPORT

#include <string.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "wifi_cmd.h"

#ifndef APP_TAG
#define APP_TAG "WIFI"
#endif

typedef struct {
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_str *authmode;
    struct arg_int *channel;
    struct arg_int *max_conn;
    struct arg_int *pairwise_cipher;
    struct arg_int *beacon_interval;
    struct arg_lit *hidden;
#if WIFI_CMD_AP_SAE_PWE_SUPPORTED
    struct arg_int *sae_pwe;
#endif
#if CONFIG_ESP_WIFI_FTM_ENABLE
    struct arg_lit *ftm_responder;
#endif
#if WIFI_CMD_AP_PMF_SUPPORTED
    struct arg_lit *disable_pmf_capable;
    struct arg_lit *pmf_required;
#endif
#if WIFI_CMD_AP_TRANSITION_DISABLE_SUPPORTED
    struct arg_int *transition_disable;
#endif
#if WIFI_CMD_AP_DTIM_PERIOD_SUPPORTED
    struct arg_int *dtim_period;
    struct arg_int *csa_count;
#endif
#if WIFI_CMD_AP_MAX_BSS_IDLE_SUPPORTED
    struct arg_int *ap_bmi_period;
    struct arg_lit *protected_keep_alive;
#endif
#if WIFI_CMD_AP_SAE_EXT_SUPPORTED
    struct arg_lit *sae_ext;
#endif
#if WIFI_CMD_AP_GTK_REKEY_SUPPORTED
    struct arg_int *gtk_rekey_interval;
#endif
#if WIFI_CMD_WPA3_COMPATIBLE_SUPPORTED
    struct arg_lit *wpa3_compatible;
#endif
    struct arg_end *end;
} ap_set_args_t;
static ap_set_args_t ap_set_args;

static int cmd_do_ap_set(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &ap_set_args);

    if (nerrors != 0) {
        arg_print_errors(stderr, ap_set_args.end, argv[0]);
        return 1;
    }

    wifi_config_t wifi_config = {};

    const char *ssid = ap_set_args.ssid->sval[0];
    if (strlen(ssid) >= sizeof(wifi_config.ap.ssid)) {
        memcpy((char *) wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    } else {
        strlcpy((char *) wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    }
    const char *pass = ap_set_args.password->sval[0];
    if (ap_set_args.password->count > 0) {
        if (strlen(pass) >= sizeof(wifi_config.ap.password)) {
            memcpy((char *) wifi_config.ap.password, pass, sizeof(wifi_config.ap.password));
        } else {
            strlcpy((char *) wifi_config.ap.password, pass, sizeof(wifi_config.ap.password));
        }
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK; // set default auth mode
    }
    if (ap_set_args.channel->count > 0) {
        wifi_config.ap.channel = (uint8_t)(ap_set_args.channel->ival[0]);
    }
    const char *auth_str = ap_set_args.authmode->sval[0];
    if (ap_set_args.authmode->count > 0) {
        wifi_config.ap.authmode = wifi_cmd_auth_mode_str2num(auth_str);
    }
    if (ap_set_args.max_conn->count > 0) {
        wifi_config.ap.max_connection = ap_set_args.max_conn->ival[0];
    } else {
        wifi_config.ap.max_connection = 2;
    }

    if (ap_set_args.pairwise_cipher->count > 0) {
        wifi_config.ap.pairwise_cipher = (wifi_cipher_type_t)ap_set_args.pairwise_cipher->ival[0];
    }

#if WIFI_CMD_AP_PMF_SUPPORTED
    if (ap_set_args.disable_pmf_capable->count > 0) {
        wifi_config.ap.pmf_cfg.capable = false;
    } else {
        wifi_config.ap.pmf_cfg.capable = true;
    }

    if (ap_set_args.pmf_required->count > 0) {
        wifi_config.ap.pmf_cfg.required = true;
    } else {
        wifi_config.ap.pmf_cfg.required = false;
    }
#endif
#if WIFI_CMD_AP_TRANSITION_DISABLE_SUPPORTED
    if (ap_set_args.transition_disable->count > 0) {
        wifi_config.ap.transition_disable = (uint8_t)ap_set_args.transition_disable->ival[0];
    } else {
        wifi_config.ap.transition_disable = 0;
    }
#endif

#if WIFI_CMD_AP_DTIM_PERIOD_SUPPORTED
    if (ap_set_args.dtim_period->count > 0) {
        wifi_config.ap.dtim_period = (uint8_t)ap_set_args.dtim_period->ival[0];
    } else {
        wifi_config.ap.dtim_period = 2;
    }

    if (ap_set_args.csa_count->count > 0) {
        wifi_config.ap.csa_count = (uint8_t)ap_set_args.csa_count->ival[0];
    } else {
        wifi_config.ap.csa_count = 3;
    }
#endif

#if WIFI_CMD_AP_MAX_BSS_IDLE_SUPPORTED
    if (ap_set_args.ap_bmi_period->count > 0) {
        wifi_config.ap.bss_max_idle_cfg.period = (uint16_t)ap_set_args.ap_bmi_period->ival[0];
    }

    if (ap_set_args.protected_keep_alive->count > 0) {
        wifi_config.ap.bss_max_idle_cfg.protected_keep_alive = true;
    }
#endif

#if WIFI_CMD_AP_SAE_EXT_SUPPORTED
    if (ap_set_args.sae_ext->count > 0) {
        wifi_config.ap.sae_ext = 1;
    }
#endif

    if (ap_set_args.hidden->count > 0) {
        wifi_config.ap.ssid_hidden = 1;
    }
#if WIFI_CMD_AP_SAE_PWE_SUPPORTED
    if (ap_set_args.sae_pwe->count > 0) {
        wifi_config.ap.sae_pwe_h2e = ap_set_args.sae_pwe->ival[0];
    }
#endif

    if (ap_set_args.beacon_interval->count > 0) {
        wifi_config.ap.beacon_interval = ap_set_args.beacon_interval->ival[0];
    }

#if CONFIG_ESP_WIFI_FTM_ENABLE
    if (ap_set_args.ftm_responder->count > 0) {
        wifi_config.ap.ftm_responder = true;
    }
#endif
#if WIFI_CMD_AP_GTK_REKEY_SUPPORTED
    if (ap_set_args.gtk_rekey_interval->count > 0) {
        wifi_config.ap.gtk_rekey_interval = ap_set_args.gtk_rekey_interval->ival[0];
    }
#endif
#if WIFI_CMD_WPA3_COMPATIBLE_SUPPORTED
    if (ap_set_args.wpa3_compatible->count > 0) {
        wifi_config.ap.wpa3_compatible_mode = 1;
    }
#endif
    esp_err_t err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
#if WIFI_CMD_AP_PMF_SUPPORTED
    if ((!wifi_config.ap.pmf_cfg.capable) && esp_wifi_disable_pmf_config(WIFI_IF_AP) != ESP_OK) {
        ESP_LOGE(APP_TAG, "Failed to disable PMF config");
        return -1;
    }
#endif
    LOG_WIFI_CMD_DONE(err, "SET_AP_CONFIG");
    return 0;
}

#if CONFIG_WIFI_CMD_ENABLE_DEPRECATED
static int cmd_do_ap_deprecated(int argc, char **argv)
{
    ESP_LOGW(APP_TAG, "'ap' is deprecated, please use 'ap_set'.");
    return cmd_do_ap_set(argc, argv);
}
#endif

static void wifi_cmd_register_ap_set(void)
{
    if (!ap_set_args.end) {
        ap_set_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
        ap_set_args.password = arg_str0(NULL, NULL, "<pass>", "password of AP");
        ap_set_args.authmode = arg_str0("a", "authmode", "<authmode>", "wifi auth type (ie. open | wep| wpa2 | wpa2_enterprise)");
        ap_set_args.channel = arg_int0("n", "channel", "<channel>", "channel of AP");
        ap_set_args.max_conn = arg_int0("m", "max_conn", "<max_conn>", "Max station number, default: 2");
#if WIFI_CMD_AP_PMF_SUPPORTED
        ap_set_args.disable_pmf_capable = arg_lit0(NULL, "disable_pmf", "Disable PMF capable");
        ap_set_args.pmf_required = arg_lit0(NULL, "pmf_required", "Enable PMF required");
#endif
#if WIFI_CMD_AP_TRANSITION_DISABLE_SUPPORTED
        ap_set_args.transition_disable = arg_int0(NULL, "transition_disable", "<transition_disable>", "enable transition disable wth numbers");
#endif
        ap_set_args.pairwise_cipher = arg_int0(NULL, "cipher", "<pairwise_cipher>", "Pairwise cipher type");
#if WIFI_CMD_AP_DTIM_PERIOD_SUPPORTED
        ap_set_args.dtim_period = arg_int0(NULL, "dtim", "<dtim_period>", "DTIM period of AP");
        ap_set_args.csa_count = arg_int0(NULL, "csa_count", "<csa_count>", "CSA count of AP");
#endif
#if WIFI_CMD_AP_MAX_BSS_IDLE_SUPPORTED
        ap_set_args.ap_bmi_period = arg_int0("b", "ap_bmi_period", "<ap_bmi_period>", "BSS max idle period of AP");
        ap_set_args.protected_keep_alive = arg_lit0(NULL, "protected_keep_alive", "Enable protected keep alive");
#endif
#if WIFI_CMD_AP_SAE_EXT_SUPPORTED
        ap_set_args.sae_ext = arg_lit0(NULL, "sae_ext", "SAE extension");
#endif
#if WIFI_CMD_AP_SAE_PWE_SUPPORTED
        ap_set_args.sae_pwe = arg_int0(NULL, "sae_pwe", "<sae_pwe_method>", "SAE PWE Method");
#endif
        ap_set_args.hidden = arg_lit0(NULL, "hidden", "hidden AP");
#if CONFIG_ESP_WIFI_FTM_ENABLE
        ap_set_args.ftm_responder = arg_lit0(NULL, "ftm_responder", "ftm responder");
#endif
        ap_set_args.beacon_interval = arg_int0("i", "beacon_interval", "<beacon_interval>", "beacon interval");
#if WIFI_CMD_AP_GTK_REKEY_SUPPORTED
        ap_set_args.gtk_rekey_interval = arg_int0(NULL, "gtk_rekey_interval", "<gtk_rekey_interval>", "GTK rekey interval");
#endif
#if WIFI_CMD_WPA3_COMPATIBLE_SUPPORTED
        ap_set_args.wpa3_compatible = arg_lit0(NULL, "wpa3_compatible", "wpa3 compatible");
#endif
        ap_set_args.end = arg_end(2);
    }
    const esp_console_cmd_t ap_set_cmd = {
        .command = "ap_set",
        .help = "WiFi is ap mode, set ap config.",
        .hint = NULL,
        .func = &cmd_do_ap_set,
        .argtable = &ap_set_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&ap_set_cmd));

#if CONFIG_WIFI_CMD_ENABLE_DEPRECATED
    esp_console_cmd_t ap_deprecated = ap_set_cmd;
    ap_deprecated.command = "ap";
    ap_deprecated.help = "Set ap config. Deprecated, please use 'ap_set'";
    ap_deprecated.func = &cmd_do_ap_deprecated;
    ESP_ERROR_CHECK(esp_console_cmd_register(&ap_deprecated));
#endif
}

void wifi_cmd_register_ap_basic(void)
{
    wifi_cmd_register_ap_set();
}

#endif  /* CONFIG_ESP_WIFI_SOFTAP_SUPPORT */
