/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_wifi.h"

#include "wifi_cmd.h"

#ifndef APP_TAG
#define APP_TAG "WIFI"
#endif

typedef struct {
    struct arg_int *unit;
    struct arg_lit *reset;
    struct arg_end *end;
} wifi_txpower_args_t;
wifi_txpower_args_t wifi_txpower_args;

static int cmd_do_wifi_txpower(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &wifi_txpower_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, wifi_txpower_args.end, argv[0]);
        return 1;
    }

    esp_err_t ret = ESP_FAIL;
    int8_t power_unit = 0;

    if (wifi_txpower_args.reset->count > 0) {
#ifdef CONFIG_ESP_PHY_MAX_WIFI_TX_POWER
        power_unit = CONFIG_ESP_PHY_MAX_WIFI_TX_POWER * 4;
#elif defined(CONFIG_ESP32_PHY_MAX_WIFI_TX_POWER) /* older esp-idf */
        power_unit = CONFIG_ESP32_PHY_MAX_WIFI_TX_POWER * 4;
#else
        /* For extconn, can not get default value from sdkconfig since phy is disabled */
        power_unit = 78;
#endif
        /* The default max tx power is 78 when tx power in sdkconfig is 20 dBm */
        ret = esp_wifi_set_max_tx_power(power_unit);
        ESP_LOGI(APP_TAG, "SET_WIFI_TXPOWER,%d,%s", power_unit, ret == ESP_OK ? "OK" : "FAIL");
        return 0;
    }
    if (wifi_txpower_args.unit->count > 0) {
        power_unit = wifi_txpower_args.unit->ival[0];
        ret = esp_wifi_set_max_tx_power(power_unit);
        ESP_LOGI(APP_TAG, "SET_WIFI_TXPOWER,%d,%s", power_unit, ret == ESP_OK ? "OK" : "FAIL");
    } else {
        ret = esp_wifi_get_max_tx_power(&power_unit);
        ESP_LOGI(APP_TAG, "GET_WIFI_TXPOWER,%d,%s", power_unit, ret == ESP_OK ? "OK" : "FAIL");
    }
    return 0;
}

void wifi_cmd_register_wifi_txpower(void)
{
    if (!wifi_txpower_args.end) {
        wifi_txpower_args.unit = arg_int0("u", "unit", "<unit>", "tx power unit, each unit is 0.25dBm. 8=2dBm");
        wifi_txpower_args.reset = arg_lit0(NULL, "reset", "reset txpower (from sdkconfig)");
        wifi_txpower_args.end = arg_end(2);
    }
    const esp_console_cmd_t wifi_txpower_cmd = {
        .command = "wifi_txpower",
        .help = "Set wifi tx power <esp_wifi_set_max_tx_power>",
        .hint = NULL,
        .func = &cmd_do_wifi_txpower,
        .argtable = &wifi_txpower_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&wifi_txpower_cmd));
}
