/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "sdkconfig.h"
#include "esp_wifi_types.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_idf_version.h"

#include "wifi_cmd.h"

#ifndef APP_TAG
#define APP_TAG "WIFI"
#endif

typedef struct {
    const int8_t value;
    const char *auth_mode;
} wifi_cmd_auth_mode_t;

static const wifi_cmd_auth_mode_t wifi_auth_mode_map[] = {
    {WIFI_AUTH_OPEN, "open"},
    {WIFI_AUTH_WEP, "wep"},
    {WIFI_AUTH_WPA_PSK, "wpa"},
    {WIFI_AUTH_WPA2_PSK, "wpa2"},
    {WIFI_AUTH_WPA_WPA2_PSK, "wpa_wpa2"},
    {WIFI_AUTH_WPA2_ENTERPRISE, "wpa2_enterprise"},
    {WIFI_AUTH_WPA3_PSK, "wpa3"},
    {WIFI_AUTH_WPA2_WPA3_PSK, "wpa2_wpa3"},
    {WIFI_AUTH_WAPI_PSK, "wapi"},
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    {WIFI_AUTH_OWE, "owe"},
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    {WIFI_AUTH_WPA3_ENT_192, "wpa3_ent_192"},
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0) /* IDFMR: 33229 */
    {WIFI_AUTH_WPA3_ENTERPRISE, "wpa3_enterprise"},
    {WIFI_AUTH_WPA2_WPA3_ENTERPRISE, "wpa2_wpa3_enterprise"},
#endif
#if WIFI_CMD_WPA_EAP_V1_SUPPORTED
    {WIFI_AUTH_WPA_ENTERPRISE, "wpa_enterprise"},
#endif
};

#define WIFI_AUTH_TYPE_NUM  (sizeof(wifi_auth_mode_map) / sizeof(wifi_cmd_auth_mode_t))

uint8_t wifi_cmd_auth_mode_str2num(const char *auth_str)
{
    uint8_t auth_mode;
    for (auth_mode = 0; auth_mode < WIFI_AUTH_TYPE_NUM; auth_mode++) {
        if (strcmp(wifi_auth_mode_map[auth_mode].auth_mode, auth_str) == 0) {
            break;
        }
    }
    return wifi_auth_mode_map[auth_mode].value;
}

const char *wifi_cmd_auth_mode_num2str(uint8_t value)
{
    uint8_t auth_mode;
    for (auth_mode = 0; auth_mode < WIFI_AUTH_TYPE_NUM; auth_mode++) {
        if (wifi_auth_mode_map[auth_mode].value == value) {
            break;
        }
    }
    // Do not abort if cannot covert autmode to str
    if (auth_mode == WIFI_AUTH_TYPE_NUM) {
        ESP_LOGE(APP_TAG, "Can not convert auth mode %d to str", value);
        return "unknown";
    }
    return wifi_auth_mode_map[auth_mode].auth_mode;
}

wifi_interface_t app_wifi_interface_str2ifx(const char *interface)
{
    if (!strncmp(interface, "ap", 3)) {
        return WIFI_IF_AP;
    } else if (!strncmp(interface, "sta", 4)) {
        return WIFI_IF_STA;
    } else {
        ESP_LOGE(APP_TAG, "Can not get interface from str: %s", interface);
        /* Do not abort */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
        return WIFI_IF_MAX;
#else /* older IDF does not have IF_MAX*/
        return WIFI_IF_STA;
#endif
    }
    return WIFI_IF_STA;
}

typedef struct {
    const char *mode_str;
    wifi_mode_t mode;
} app_wifi_mode_t;

static const app_wifi_mode_t s_wifi_mode_map[] = {
    {"null", WIFI_MODE_NULL},
    {"ap", WIFI_MODE_AP},
    {"sta", WIFI_MODE_STA},
    {"apsta", WIFI_MODE_APSTA},
};

#define WIFI_MODE_NUM  (sizeof(s_wifi_mode_map) / sizeof(app_wifi_mode_t))

wifi_mode_t app_wifi_mode_str2num(const char *mode_str)
{
    wifi_mode_t mode = 0;
    int i = 0;
    for (i = 0; i < WIFI_MODE_NUM; i++) {
        if (strcmp(s_wifi_mode_map[i].mode_str, mode_str) == 0) {
            mode = s_wifi_mode_map[i].mode;
            break;
        }
    }
    if (i == WIFI_MODE_NUM) {
        ESP_LOGE(APP_TAG, "Can not convert mode %s from str to value.", mode_str);
        /* Do not abort */
        return WIFI_MODE_NULL;
    }
    return mode;
}

const char *app_wifi_mode_num2str(const wifi_mode_t mode)
{
    const char *mode_str = NULL;
    int i = 0;
    for (i = 0; i < WIFI_MODE_NUM; i++) {
        if (s_wifi_mode_map[i].mode == mode) {
            mode_str = s_wifi_mode_map[i].mode_str;
            break;
        }
    }
    if (i == WIFI_MODE_NUM) {
        ESP_LOGE(APP_TAG, "Can not convert mode %d to str", mode);
        /* Do not abort */
        return "unknown";
    }
    return mode_str;
}

esp_err_t wifi_cmd_str2mac(const char *str, uint8_t *mac_addr)
{
    unsigned int mac_tmp[6];
    if (6 != sscanf(str, "%02x:%02x:%02x:%02x:%02x:%02x%*c",
                    &mac_tmp[0], &mac_tmp[1], &mac_tmp[2],
                    &mac_tmp[3], &mac_tmp[4], &mac_tmp[5])) {
        return ESP_ERR_INVALID_MAC;
    }
    for (int i = 0; i < 6; i++) {
        mac_addr[i] = (uint8_t)mac_tmp[i];
    }
    return ESP_OK;

}

#if WIFI_CMD_5G_SUPPORTED
#define MAX_CHANNEL_STR_LEN 128
uint32_t wifi_cmd_channel_str2bitmsk(wifi_band_t band, const char *channel_list_str)
{
    char input_copy[MAX_CHANNEL_STR_LEN];
    uint32_t channel_bitmap = 0;
    uint8_t channel = 0;
    strcpy(input_copy, channel_list_str);
    char *endptr;

    char *token = strtok(input_copy, ",");
    while (token != NULL) {

        channel = (uint8_t)strtol(token, &endptr, 10);
        // return 0 if input is not all numbers
        if (*endptr != '\0') {
            return 0;
        }

        if (band == WIFI_BAND_2G && channel > 14) {
            return 0;
        } else if (band == WIFI_BAND_5G && channel < 36) {
            return 0;
        }
        channel_bitmap |= CHANNEL_TO_BIT(channel);

        token = strtok(NULL, ",");
    }
    return channel_bitmap;
}

const char *wifi_cmd_channel_bitmsk2str(wifi_band_t band, uint32_t bitmap)
{
    static char channel_str[MAX_CHANNEL_STR_LEN];
    memset(channel_str, 0, sizeof(channel_str));

    int first_channel = 1;
    for (int bit_number = 0; bit_number < 32; bit_number++) {
        if (bitmap & (1 << bit_number)) {
            int channel = BIT_NUMBER_TO_CHANNEL(bit_number, band);
            if (channel != 0) {
                if (!first_channel) {
                    strcat(channel_str, ",");
                }
                char temp[8];
                snprintf(temp, sizeof(temp), "%d", channel);
                strcat(channel_str, temp);
                first_channel = 0;
            }
        }
    }

    if (strlen(channel_str) == 0) {
        return "UNKNOWN";
    }

    return channel_str;
}
#endif  /* WIFI_CMD_5G_SUPPORTED */

#if WIFI_PHY_MODE_SUPPORTED
const char *wifi_cmd_phy_mode_num2str(wifi_phy_mode_t phymode)
{
    switch (phymode) {
    case WIFI_PHY_MODE_LR:
        return "lr";
    case WIFI_PHY_MODE_11B:
        return "b";
    case WIFI_PHY_MODE_11G:
        return "g";

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 1) /* IDF commit: f0365ba6b5d832a2e5999fda56b89c1f18c02858 */
    case WIFI_PHY_MODE_11A:
        return "a";
#endif /* v5.3.1 */

    case WIFI_PHY_MODE_HT20:
        return "ht20";
    case WIFI_PHY_MODE_HT40:
        return "ht40";
    case WIFI_PHY_MODE_HE20:
        return "he20";

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 1) /* IDF commit: f0365ba6b5d832a2e5999fda56b89c1f18c02858 */
    case WIFI_PHY_MODE_VHT20:
        return "vht20";
#endif /* v5.3.1 */

    default:
        return "?";
    }
    return "?";
}
#endif /* WIFI_PHY_MODE_SUPPORTED */

typedef struct {
    const char *protocol;
    uint32_t bitmap;
} wifi_cmd_protocol_map_t;

static const wifi_cmd_protocol_map_t s_wifi_protocol_map[] = {
    {"lr", WIFI_PROTOCOL_LR},
    {"b", WIFI_PROTOCOL_11B},
    {"g", WIFI_PROTOCOL_11G},
    {"n", WIFI_PROTOCOL_11N},
    {"lr/b", WIFI_PROTOCOL_LR | WIFI_PROTOCOL_11B},
    {"b/g", WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G},
    {"lr/b/g", WIFI_PROTOCOL_LR | WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G},
    {"b/g/n", WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N},
    {"lr/b/g/n", WIFI_PROTOCOL_LR | WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N},
#if WIFI_CMD_HE_SUPPORTED
    {"ax", WIFI_PROTOCOL_11AX},
    {"b/g/n/ax", WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_11AX},
    {"lr/b/g/n/ax", WIFI_PROTOCOL_LR | WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_11AX},
#endif
#if WIFI_CMD_5G_SUPPORTED
    {"a", WIFI_PROTOCOL_11A},
    {"ac", WIFI_PROTOCOL_11AC},
    {"a/n", WIFI_PROTOCOL_11A | WIFI_PROTOCOL_11N},
    {"a/n/ac", WIFI_PROTOCOL_11A | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_11AC},
    {"a/n/ac/ax", WIFI_PROTOCOL_11A | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_11AC | WIFI_PROTOCOL_11AX},
#endif
};

#define WIFI_PROTOCOL_NUM  (sizeof(s_wifi_protocol_map) / sizeof(wifi_cmd_protocol_map_t))

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
#define INVALID_WIFI_PROTO WIFI_PROTOCOL_11AX << 1
#else
#define INVALID_WIFI_PROTO WIFI_PROTOCOL_LR << 1
#endif

static uint8_t is_hex_string(const char *str)
{
    if (str == NULL) {
        return 0;
    }

    if (str[0] != '0' || str[1] != 'x') {
        return 0;  // Must start with 0x
    }

    str += 2; // Skip '0x'
    while (*str) {
        if (!isxdigit((unsigned char) * str)) {
            return 0;
        }
        str++;
    }

    return 1;
}

uint32_t wifi_cmd_protocol_str2bitmap(const char *config)
{
    // parse wifi protocol is bitmap(decimal)
    if (is_hex_string(config)) {
        return (uint32_t)strtoul(config, NULL, 16);
    }

    // parse if proto str start with lr
    bool has_lr = (strncmp(config, "lr/", 3) == 0);
    const char *sub_proto = has_lr ? config + 3 : config;

    for (size_t i = 0; i < WIFI_PROTOCOL_NUM; i++) {
        if (strcmp(sub_proto, s_wifi_protocol_map[i].protocol) == 0) {
            return has_lr ? (WIFI_PROTOCOL_LR | s_wifi_protocol_map[i].bitmap) : s_wifi_protocol_map[i].bitmap;
        }
    }

    // return invalid bitmap if not match any item
    return INVALID_WIFI_PROTO;
}

const char *wifi_cmd_protocol_bitmap2str(const uint32_t bitmap)
{
    const char *protocol_str = NULL;
    int i = 0;
    for (i = 0; i < WIFI_PROTOCOL_NUM; i++) {
        if (s_wifi_protocol_map[i].bitmap == bitmap) {
            protocol_str = s_wifi_protocol_map[i].protocol;
            break;
        }
    }
    if (i == WIFI_PROTOCOL_NUM) {
        ESP_LOGE(APP_TAG, "Can not convert bitmap 0x%x to protocol string.", bitmap);
        /* Do not abort */
        return "unknown";
    }
    return protocol_str;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 1)   /* IDF commit: f0365ba6b5d832a2e5999fda56b89c1f18c02858 */
const char *wifi_cmd_ap_bandwidth_to_str(wifi_bandwidth_t bandwidth)
{
    switch (bandwidth) {
    case WIFI_BW20:        return "20MHz";
    case WIFI_BW40:        return "40MHz";
    case WIFI_BW80:        return "80MHz";
    case WIFI_BW160:       return "160MHz";
    case WIFI_BW80_BW80:   return "80+80MHz";
    default:               return "unknown";
    }
}
#endif

uint32_t wifi_cmd_get_phy_bitmap(const wifi_phy_support_info_t *phy_support_info)
{
    uint32_t phy_bitmap = 0;

    struct {
        bool phy_supported;
        uint32_t protocol_flag;
    } phy_map[] = {
        {phy_support_info->phy_11b, WIFI_PROTOCOL_11B},
        {phy_support_info->phy_11g, WIFI_PROTOCOL_11G},
        {phy_support_info->phy_11n, WIFI_PROTOCOL_11N},
        {phy_support_info->phy_lr, WIFI_PROTOCOL_LR},
#if WIFI_CMD_5G_SUPPORTED
        {phy_support_info->phy_11a, WIFI_PROTOCOL_11A},
        {phy_support_info->phy_11ac, WIFI_PROTOCOL_11AC},
#endif
#if WIFI_CMD_HE_SUPPORTED
        {phy_support_info->phy_11ax, WIFI_PROTOCOL_11AX},
#endif
    };

    for (size_t i = 0; i < sizeof(phy_map) / sizeof(phy_map[0]); i++) {
        if (phy_map[i].phy_supported) {
            phy_bitmap |= phy_map[i].protocol_flag;
        }
    }

    return phy_bitmap;
}
