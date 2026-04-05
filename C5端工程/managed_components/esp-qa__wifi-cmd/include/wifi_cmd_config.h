/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"
#include "esp_wifi.h"
#include "esp_idf_version.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CONFIG_ESP_WIFI_ENABLED added in idf v5.1
 * define it in header file to simplify the application code
 */
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#define CONFIG_ESP_WIFI_ENABLED (1)
#elif ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 1, 0)
#ifdef CONFIG_ESP32_WIFI_ENABLED
#define CONFIG_ESP_WIFI_ENABLED (CONFIG_ESP32_WIFI_ENABLED)
#endif
#endif

/* esp-hosted uses CONFIG_ESP_WIFI_REMOTE_ENABLED, esp-extconn uses CONFIG_ESP_HOST_WIFI_ENABLED */
#if CONFIG_ESP_WIFI_ENABLED || CONFIG_ESP_WIFI_REMOTE_ENABLED || CONFIG_ESP_HOST_WIFI_ENABLED
#define WIFI_CMD_WIFI_ENABLED (1)
#endif

/* wifi_phy_mode_t supported at IDF commit: c681c92e4c125e5ba0599d4d51d3c423c8afe518, backports: v5.0.3, v4.4.7 */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 1) || \
    (ESP_IDF_VERSION_MINOR == 0 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 3)) || \
    (ESP_IDF_VERSION_MAJOR == 4 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 7))
#define WIFI_PHY_MODE_SUPPORTED (1)
#endif

/* CONFIG_ESP_WIFI_SOFTAP_SUPPORT supported at IDF commit: a568b4fddfb7dfdd11b80f285cdb136bd1f7fbca, backports: v5.0.0, v4.4.1 */
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 4, 1)
#define CONFIG_ESP_WIFI_SOFTAP_SUPPORT (1)
#endif

/* CONFIG_SOC_WIFI_HE_SUPPORT_5G is an old config */
/* CONFIG_SLAVE_SOC_WIFI_SUPPORT_5G was set from esp-hosted */
#if CONFIG_SOC_WIFI_SUPPORT_5G || CONFIG_SOC_WIFI_HE_SUPPORT_5G || CONFIG_SLAVE_SOC_WIFI_SUPPORT_5G
#define WIFI_CMD_5G_SUPPORTED (1)
#endif

/* CONFIG_SLAVE_SOC_WIFI_HE_SUPPORT was set from esp-hosted */
#if CONFIG_SOC_WIFI_HE_SUPPORT || CONFIG_SLAVE_SOC_WIFI_HE_SUPPORT
#define WIFI_CMD_HE_SUPPORTED (1)
#endif

#if WIFI_CMD_HE_SUPPORTED && CONFIG_WIFI_CMD_ENABLE_ITWT
#define WIFI_CMD_ITWT_ENABLED (1)
#endif

#if WIFI_CMD_5G_SUPPORTED && CONFIG_WIFI_CMD_ENABLE_BAND
#define WIFI_CMD_BAND_ENABLED (1)
#endif

/* Disable wifi he debug command for esp_wifi_remote/wifi_hosted */
#if WIFI_CMD_HE_SUPPORTED && CONFIG_WIFI_CMD_ENABLE_HE_DEBUG
#define WIFI_CMD_HE_DEBUG_ENABLED (1)
#endif

/* CONFIG_LWIP_IPV4 added at IDF commit: 956e62c46144181ee173d2f8d065e37b6b068618  (v5.1.0) */
#if CONFIG_LWIP_IPV4 || ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 1, 0)
#define WIFI_CMD_IPV4_ENABLED (1)
#endif

/* Backport IDF 4.3: d7e680828a4798725232475d1afd04a832d046f5 */
#if CONFIG_LWIP_IPV6
#define WIFI_CMD_IPV6_ENABLED (1)
#endif

/* Needn't check sdkconfig for api usage */
/* mbo_enabled was supported at IDF commit: 834afad47e866071ae9e60552f64f25ab9068b48  (v4.4.0) */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) /* Ignore EOL versions */
#define WIFI_CMD_STA_ROAM_SUPPORTED (1)
#endif

/*
 * Version compatibility: feature support determined by IDF tag versions
 * - Feature added after va.b-dev but before va.b → supported from va.b.1
 * - Feature added after va.b but before va.b.1   → supported from va.b.1
 * - Feature added after va.b.1 but before va.b.2 → supported from va.b.2
 */

/* Needn't check sdkconfig for api usage */
/* owe_enabled was supported at IDF commit: 6d55761f60837272ef9e6f3a39847f8d1ddf7836  (v5.0.0) */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 1)
#define WIFI_CMD_STA_OWE_SUPPORTED (1)
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 1) /* IDF commit: c3a661c0fdf0deb2fba1757ba9b410b356d92296*/
#define WIFI_CMD_AP_PMF_SUPPORTED (1)
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 1) /* IDF commit: c4f592679bc4a8af33b1006b587cf0be88966ce8*/
#define WIFI_CMD_AP_MAX_BSS_IDLE_SUPPORTED (1)
#endif

/* Do not enabled these features if WIFI_CMD_BASIC_ONLY is enabled */
#if !CONFIG_WIFI_CMD_BASIC_ONLY
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 6) /* IDF commit: 979cd25e605658e127cd957dc8f343155ba7efb1*/
#define WIFI_CMD_AP_TRANSITION_DISABLE_SUPPORTED (1)
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 1) /* IDF commit: 63b288ba84a80f3297a59b49a6a5b4e464c526fa */
#define WIFI_CMD_AP_SAE_PWE_SUPPORTED (1)
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 4) /* IDF commit: cf2bdb08f4aa583a816771eb99aa2815c2434cc1 */
#define WIFI_CMD_AP_DTIM_PERIOD_SUPPORTED (1)
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 1) /* IDF commit: 4355fc8fbc6ba2216d8911587c2a5b687eed7d96*/
#define WIFI_CMD_AP_SAE_EXT_SUPPORTED (1)
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 1) /* IDF commit: 60ab2598c5494318d82c7491620a5efe919d19c3*/
#define WIFI_CMD_AP_GTK_REKEY_SUPPORTED (1)
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 1) /* IDF commit: d2f6a3dacc1cbec8ebc0deb64cc7211f7c7750c5*/
#define WIFI_CMD_STA_SAE_PK_SUPPORTED (1)
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 1) /* IDF commit: a3b5472d99ea24e6a1dbf4144d4cb21e3c653fba*/
#define WIFI_CMD_STA_SAE_PASSWORD_SUPPORTED (1)
#endif

// frag  tx is internal only,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 6) && !CONFIG_ESP_WIFI_REMOTE_ENABLED && !CONFIG_ESP_HOST_WIFI_ENABLED /* IDF commit: c678cf94322b2d13c0d0c8016dceee234eed06d7 */
#define WIFI_CMD_FRAG_TX_SUPPORTED (1)
#endif

/* supported at IDF commit: a7f32f5a2a3cd0e08d8ed0706ecf01ebffaec810 */
/* There is a new kconfig CONFIG_ESP_WIFI_WPA3_COMPATIBLE_SUPPORT, needn't check idf version */
#if CONFIG_ESP_WIFI_WPA3_COMPATIBLE_SUPPORT && !CONFIG_ESP_WIFI_REMOTE_ENABLED && !CONFIG_ESP_HOST_WIFI_ENABLED /* IDF MR 36991*/
#define WIFI_CMD_WPA3_COMPATIBLE_SUPPORTED (1)
#endif

/* v5.1.6 commit id 53e113f87ab475cd6d18eeeb6c30db363a3917f8
   v5.2.5 commit id f5c14ba5a026b26e2c8e2f6b2e584ee499f25898
   v5.3.3 commit id 3b8740860e0ec981c027e194d54decde059e24ec
   v5.4.2 commit id 68d8b1e7ca40fb5e8d8ae4c169157e409b12dbaa
   v5.5.1 commit id de17b6ff94bf2d00a9189974572ad074ba0d30ed
*/
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 6) || \
    ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 5) || \
    ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 3) || \
    ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 2) || \
    ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 1)
#define WIFI_CMD_WPA_EAP_V1_SUPPORTED (1)
#endif

#endif /* CONFIG_BASIC_ONLY */

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0) /* IDF commit: 27f61966cdd65da99138eaa61b4c02ed5b75fcb0*/
#define WIFI_BW20 WIFI_BW_HT20
#define WIFI_BW40 WIFI_BW_HT40
#endif

#ifdef __cplusplus
}
#endif
