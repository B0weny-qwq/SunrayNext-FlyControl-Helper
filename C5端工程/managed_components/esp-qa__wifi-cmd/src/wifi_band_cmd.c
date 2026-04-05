/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <sdkconfig.h>
#include <ctype.h>

#include "wifi_cmd.h"

#if WIFI_CMD_BAND_ENABLED
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"

#ifndef APP_TAG
#define APP_TAG "WIFI"
#endif

typedef struct {
    struct arg_str *band_cfg;
    struct arg_lit *curr_band_flag;
    struct arg_end *end;
} wifi_band_args_t;
static wifi_band_args_t wifi_band_args;

typedef struct {
    const char *band_str;
    uint8_t value;
} app_wifi_band_t;

static const app_wifi_band_t s_wifi_band_map[] = {
    {"2g", WIFI_BAND_MODE_2G_ONLY},
    {"5g", WIFI_BAND_MODE_5G_ONLY},
    {"auto", WIFI_BAND_MODE_AUTO},
};

#define WIFI_BAND_CONFIG_NUM  (sizeof(s_wifi_band_map) / sizeof(app_wifi_band_t))

static uint8_t app_wifi_band_str2bitmap(const char *band_str)
{
    uint8_t band = 0;
    int i = 0;
    for (i = 0; i < WIFI_BAND_CONFIG_NUM; i++) {
        if (strcmp(s_wifi_band_map[i].band_str, band_str) == 0) {
            band = s_wifi_band_map[i].value;
            break;
        }
    }
    if (i == WIFI_BAND_CONFIG_NUM) {
        ESP_LOGE(APP_TAG, "Can not convert wifi band %s from str to value.", band_str);
        return 0;
    }
    return band;
}

static const char *app_wifi_band_num2str(const uint8_t band)
{
    const char *mode_str = NULL;
    int i = 0;
    for (i = 0; i < WIFI_BAND_CONFIG_NUM; i++) {
        if (s_wifi_band_map[i].value == band) {
            mode_str = s_wifi_band_map[i].band_str;
            break;
        }
    }
    if (i == WIFI_BAND_CONFIG_NUM) {
        ESP_LOGE(APP_TAG, "Can not convert wifi band %u to str", band);
        return "UNKOWN";
    }

    static char upper_mode_str[16];
    strncpy(upper_mode_str, mode_str, sizeof(upper_mode_str));
    upper_mode_str[sizeof(upper_mode_str) - 1] = '\0';

    for (int i = 0; upper_mode_str[i] != '\0'; i++) {
        upper_mode_str[i] = toupper((unsigned char)upper_mode_str[i]);
    }

    return upper_mode_str;
}

static int cmd_do_set_wifi_band(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &wifi_band_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, wifi_band_args.end, argv[0]);
        return 1;
    }

    esp_err_t err = ESP_FAIL;
    if (wifi_band_args.band_cfg->count == 0) {
        if (wifi_band_args.curr_band_flag->count == 0) {
            wifi_band_mode_t band = 0;
            err = esp_wifi_get_band_mode(&band);
            if (err == ESP_OK) {
                ESP_LOGI(APP_TAG, "GET_WIFI_BAND_MODE:0x%x,%s", band, app_wifi_band_num2str(band));
            } else {
                ESP_LOGE(APP_TAG, "GET_WIFI_BAND_MODE:FAIL,%d,%s", err, esp_err_to_name(err));
            }
        } else {
            wifi_band_t band = 0;
            err = esp_wifi_get_band(&band);
            if (err == ESP_OK) {
                ESP_LOGI(APP_TAG, "GET_CURR_WIFI_BAND:0x%x,%s", band, app_wifi_band_num2str(band));
            } else {
                ESP_LOGE(APP_TAG, "GET_CURR_WIFI_BAND:FAIL,%d,%s", err, esp_err_to_name(err));
            }
        }
        return 0;
    } else {
        if (wifi_band_args.curr_band_flag->count == 0) {
            wifi_band_mode_t band_mode = app_wifi_band_str2bitmap(wifi_band_args.band_cfg->sval[0]);
            err = esp_wifi_set_band_mode(band_mode);
            LOG_WIFI_CMD_DONE(err, "SET_WIFI_BAND_MODE");
        } else {
            wifi_band_t band = app_wifi_band_str2bitmap(wifi_band_args.band_cfg->sval[0]);
            err = esp_wifi_set_band(band);
            LOG_WIFI_CMD_DONE(err, "SET_CURR_WIFI_BAND");
        }
    }
    return 0;
}

void wifi_cmd_register_wifi_band(void)
{
    if (!wifi_band_args.end) {
        wifi_band_args.band_cfg = arg_str0(NULL, NULL, "<band>", "Wi-Fi band config: 2g, 5g or auto, None: show current band.");
        wifi_band_args.curr_band_flag = arg_lit0(NULL, "current", "Set/Get current working band or not");
        wifi_band_args.end = arg_end(1);
    }
    const esp_console_cmd_t wifi_band_cmd = {
        .command = "wifi_band",
        .help = "set/get Wifi band",
        .hint = NULL,
        .func = &cmd_do_set_wifi_band,
        .argtable = &wifi_band_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&wifi_band_cmd));
}
#endif /* WIFI_CMD_BAND_ENABLED */
