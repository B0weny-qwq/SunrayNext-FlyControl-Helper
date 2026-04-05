/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_netif_ip_addr.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_system.h"

#include "wifi_cmd.h"

#ifndef APP_TAG
#define APP_TAG "WIFI"
#endif

typedef struct {
    struct arg_str *proto_basic;
    struct arg_str *proto_2ghz;
    struct arg_str *proto_5ghz;
    struct arg_str *interface;
    struct arg_end *end;
} wifi_protocol_args_t;
static wifi_protocol_args_t wifi_protocol_args;

typedef struct {
    struct arg_int *cbw_basic;
    struct arg_int *cbw_2ghz;
    struct arg_int *cbw_5ghz;
    struct arg_str *interface;
    struct arg_end *end;
} wifi_bandwidth_args_t;
static wifi_bandwidth_args_t wifi_bandwidth_args;

static int cmd_do_wifi_protocol(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &wifi_protocol_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, wifi_protocol_args.end, argv[0]);
        return 1;
    }

    if ((wifi_protocol_args.proto_basic->count > 0 && wifi_protocol_args.proto_2ghz->count > 0)
            || (wifi_protocol_args.proto_basic->count > 0 && wifi_protocol_args.proto_5ghz->count > 0)) {
        ESP_LOGE(APP_TAG, "Cannot call esp_wifi_set_protocol and esp_wifi_set_protocols at same time");
        return 1;
    }

    esp_err_t err = ESP_FAIL;
    const char *interface = NULL;
    if (wifi_protocol_args.interface->count == 0) {
        interface = "sta";
    } else {
        interface = wifi_protocol_args.interface->sval[0];
    }
    wifi_interface_t ifx = app_wifi_interface_str2ifx(interface);

    if (wifi_protocol_args.proto_basic->count == 0
            && wifi_protocol_args.proto_2ghz->count == 0
            && wifi_protocol_args.proto_5ghz->count == 0) {
#if WIFI_CMD_5G_SUPPORTED
        wifi_protocols_t proto_config = {0, };
        err = esp_wifi_get_protocols(ifx, &proto_config);
        if (err == ESP_OK) {
            ESP_LOGI(APP_TAG, "(%s)GET_WIFI_PROTO:%s, GET_WIFI_PROTO_5G:%s",
                     interface,
                     wifi_cmd_protocol_bitmap2str((uint32_t)proto_config.ghz_2g),
                     wifi_cmd_protocol_bitmap2str((uint32_t)proto_config.ghz_5g));
        } else {
            ESP_LOGE(APP_TAG, "GET_WIFI_PROTO:FAIL,%d,%s", err, esp_err_to_name(err));
        }

#else
        uint8_t protocol = 0;
        err = esp_wifi_get_protocol(ifx, &protocol);
        if (err == ESP_OK) {
            ESP_LOGI(APP_TAG, "(%s)GET_WIFI_PROTO:%s", interface, wifi_cmd_protocol_bitmap2str((uint32_t)protocol));
        } else {
            ESP_LOGE(APP_TAG, "GET_WIFI_PROTO:FAIL,%d,%s", err, esp_err_to_name(err));
        }

#endif  /* WIFI_CMD_5G_SUPPORTED */
        return 0;
    }

    if (wifi_protocol_args.proto_basic->count > 0) {
        uint32_t protocol = wifi_cmd_protocol_str2bitmap(wifi_protocol_args.proto_basic->sval[0]);
        err = esp_wifi_set_protocol(ifx, protocol);
        LOG_WIFI_CMD_DONE(err, "WIFI_PROTOCOL");
        return 0;
    }

    if (wifi_protocol_args.proto_2ghz->count > 0 || wifi_protocol_args.proto_5ghz->count > 0) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
        wifi_protocols_t proto_config = {0, };

        if (wifi_protocol_args.proto_2ghz->count > 0) {
            proto_config.ghz_2g = wifi_cmd_protocol_str2bitmap(wifi_protocol_args.proto_2ghz->sval[0]);
        } else {
            proto_config.ghz_2g = 0;
        }
        if (wifi_protocol_args.proto_5ghz->count > 0) {
            proto_config.ghz_5g = wifi_cmd_protocol_str2bitmap(wifi_protocol_args.proto_5ghz->sval[0]);
        } else {
            proto_config.ghz_5g = 0;
        }
        ESP_LOGD(APP_TAG, "WiFi 2g protocol: %x, WiFi 5g protocol: %x, interface: %s", proto_config.ghz_2g, proto_config.ghz_5g, interface);
        err = esp_wifi_set_protocols(ifx, &proto_config);
        LOG_WIFI_CMD_DONE(err, "WIFI_PROTOCOL");
#else
        if (wifi_protocol_args.proto_2ghz->count > 0 && wifi_protocol_args.proto_5ghz->count == 0) {
            err = esp_wifi_set_protocol(ifx, wifi_cmd_protocol_str2bitmap(wifi_protocol_args.proto_2ghz->sval[0]));
            LOG_WIFI_CMD_DONE(err, "WIFI_PROTOCOL");
            ESP_LOGW(APP_TAG, "Not support 'esp_wifi_set_protocols' api before IDF version 5.4.0, change 'esp_wifi_set_protocol' instead");
        } else {
            ESP_LOGW(APP_TAG, "Not support 'esp_wifi_set_protocols' api before IDF version 5.4.0");
        }
#endif  /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0) */
    }
    return 0;
}

void wifi_cmd_register_wifi_protocol(void)
{
    if (!wifi_protocol_args.end) {
        wifi_protocol_args.proto_basic = arg_str0(NULL, NULL, "<protocol>",
                                                  "api: esp_wifi_set_protocol.\n"
                                                  "protocol: use `/` to split protocols.\n"
                                                  "must input complete protocol bitmap config.\n"
                                                  "eg: 'b', 'b/g','b/g/n', g/n is invalid.");
        wifi_protocol_args.proto_2ghz = arg_str0(NULL, "2g", "<2g_proto>",
                                                 "api: esp_wifi_set_protocols.\n"
                                                 "2g protocol\n"
                                                 "protocol string: 'lr', 'b', 'g', 'n', 'ax'\n"
                                                 "combine wiht lr mode: 'lr/b', 'lr/g', etc\n"
                                                 "protocol bitmap value: raw bitmap value with hex, eg: 0x40");
        wifi_protocol_args.proto_5ghz = arg_str0(NULL, "5g", "<5g_proto>", "Same as <2g_proto>");
        wifi_protocol_args.interface = arg_str0("i", "interface", "<interface>", "interface, 'ap', 'sta'. Default: sta");
        wifi_protocol_args.end = arg_end(2);
    }
    const esp_console_cmd_t wifi_protocol_cmd = {
        .command = "wifi_protocol",
        .help = "Set Wifi Protocol <esp_wifi_set_protocol[s]>\n"
        "Cannot call esp_wifi_set_protocol and esp_wifi_set_protocols at same time\n"
        "If not given any config params will query current wifi protocol config",
        .hint = NULL,
        .func = &cmd_do_wifi_protocol,
        .argtable = &wifi_protocol_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&wifi_protocol_cmd));
}

static int cmd_do_wifi_bandwidth(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &wifi_bandwidth_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, wifi_bandwidth_args.end, argv[0]);
        return 1;
    }

    if ((wifi_bandwidth_args.cbw_basic->count > 0 && wifi_bandwidth_args.cbw_2ghz->count > 0)
            || (wifi_bandwidth_args.cbw_basic->count > 0 && wifi_bandwidth_args.cbw_5ghz->count > 0)) {
        ESP_LOGE(APP_TAG, "Cannot call esp_wifi_set_bandwidht and esp_wifi_set_bandwidhts at same time");
        return 1;
    }

    const char *interface = NULL;
    esp_err_t err = ESP_FAIL;

    if (wifi_bandwidth_args.interface->count == 0) {
        interface = "sta";
    } else {
        interface = wifi_bandwidth_args.interface->sval[0];
    }

    wifi_interface_t ifx = app_wifi_interface_str2ifx(interface);

    if (wifi_bandwidth_args.cbw_basic->count == 0
            && wifi_bandwidth_args.cbw_2ghz->count == 0
            && wifi_bandwidth_args.cbw_5ghz->count == 0) {
#if WIFI_CMD_5G_SUPPORTED
        wifi_bandwidths_t cbw_config = {0, };
        err = esp_wifi_get_bandwidths(ifx, &cbw_config);
        if (err == ESP_OK) {
            ESP_LOGI(APP_TAG, "(%s)GET_WIFI_CBW:%dMHz, GET_WIFI_CBW_5G:%dMHz",
                     interface,
                     cbw_config.ghz_2g == WIFI_BW40 ? 40 : 20,
                     cbw_config.ghz_5g == WIFI_BW40 ? 40 : 20);
        } else {
            ESP_LOGE(APP_TAG, "GET_WIFI_CBW:FAIL,%d,%s", err, esp_err_to_name(err));
        }

#else
        wifi_bandwidth_t cbw = 0;
        err = esp_wifi_get_bandwidth(ifx, &cbw);
        if (err == ESP_OK) {
            ESP_LOGI(APP_TAG, "(%s)GET_WIFI_CBW:%dMHz", interface, cbw == WIFI_BW40 ? 40 : 20);
        } else {
            ESP_LOGE(APP_TAG, "GET_WIFI_CBW:FAIL,%d,%s", err, esp_err_to_name(err));
        }
#endif  /* WIFI_CMD_5G_SUPPORTED */
        return 0;
    }

    if (wifi_bandwidth_args.cbw_basic->count > 0) {
        const int bandwidth = wifi_bandwidth_args.cbw_basic->ival[0];
        if (bandwidth == 20) {
            err = esp_wifi_set_bandwidth(ifx, WIFI_BW20);
        } else if (bandwidth == 40) {
            err = esp_wifi_set_bandwidth(ifx, WIFI_BW40);
        }
        LOG_WIFI_CMD_DONE(err, "WIFI_BANDWIDTH");
        return 0;
    }

    if (wifi_bandwidth_args.cbw_2ghz->count > 0 || wifi_bandwidth_args.cbw_5ghz->count > 0) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
        wifi_bandwidths_t cbw_config = {0};

        if (wifi_bandwidth_args.cbw_2ghz->count > 0) {
            if (wifi_bandwidth_args.cbw_2ghz->ival[0] == 20) {
                cbw_config.ghz_2g = WIFI_BW20;
            } else if (wifi_bandwidth_args.cbw_2ghz->ival[0] == 40) {
                cbw_config.ghz_2g = WIFI_BW40;
            }
        }
        if (wifi_bandwidth_args.cbw_5ghz->count > 0) {
            if (wifi_bandwidth_args.cbw_5ghz->ival[0] == 20) {
                cbw_config.ghz_5g = WIFI_BW20;
            } else if (wifi_bandwidth_args.cbw_5ghz->ival[0] == 40) {
                cbw_config.ghz_5g = WIFI_BW40;
            }
        }

        err = esp_wifi_set_bandwidths(ifx, &cbw_config);
        LOG_WIFI_CMD_DONE(err, "WIFI_BANDWIDTH");
#else
        if (wifi_bandwidth_args.cbw_2ghz->count > 0 && wifi_bandwidth_args.cbw_5ghz->count == 0) {
            if (wifi_bandwidth_args.cbw_2ghz->ival[0] == 20) {
                err = esp_wifi_set_bandwidth(ifx, WIFI_BW20);
            } else if (wifi_bandwidth_args.cbw_2ghz->ival[0] == 40) {
                err = esp_wifi_set_bandwidth(ifx, WIFI_BW40);
            }
            LOG_WIFI_CMD_DONE(err, "WIFI_BANDWIDTH");
            ESP_LOGW(APP_TAG, "Not support 'esp_wifi_set_bandwidths' api before IDF version 5.4.0, change 'esp_wifi_set_bandwidth' instead");
        } else {
            ESP_LOGW(APP_TAG, "Not support 'esp_wifi_set_bandwidths' api before IDF version 5.4.0");
        }
#endif /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0) */
    }
    return 0;
}

void wifi_cmd_register_wifi_bandwidth(void)
{
    if (!wifi_bandwidth_args.end) {
        wifi_bandwidth_args.cbw_basic = arg_int0(NULL, NULL, "<cbw>", "20 or 40, cbw config with 'esp_wifi_set_bandwidth()'");
        wifi_bandwidth_args.cbw_2ghz = arg_int0(NULL, "2g", "<2g_cbw>", "20 or 40, 2g cbw config with 'esp_wifi_set_bandwidths()'");
        wifi_bandwidth_args.cbw_5ghz = arg_int0(NULL, "5g", "<5g_cbw>", "20 or 40, 5g cbw config with 'esp_wifi_set_bandwidths()'");
        wifi_bandwidth_args.interface = arg_str0("i", "interface", "<interface>", "interface, 'ap', 'sta'. Default: sta");
        wifi_bandwidth_args.end = arg_end(2);
    }
    const esp_console_cmd_t wifi_bandwidth_cmd = {
        .command = "wifi_bandwidth",
        .help = "Set Wifi bandwidth <esp_wifi_set_bandwidth[s]>\n"
        "Cannot call esp_wifi_set_bandwidht and esp_wifi_set_bandwidhts at same time\n"
        "If not given config any params will query current wifi cbw config",
        .hint = NULL,
        .func = &cmd_do_wifi_bandwidth,
        .argtable = &wifi_bandwidth_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&wifi_bandwidth_cmd));
}
