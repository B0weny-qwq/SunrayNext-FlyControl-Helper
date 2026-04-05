/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
// TODO: cleanup this file

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"

#include "wifi_cmd.h"

#if WIFI_CMD_HE_DEBUG_ENABLED

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "ping/ping_sock.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_wifi_types.h"
#include "esp_wifi_he.h"
#include "esp_private/esp_wifi_he_private.h"

/*
 * enable/disable rx/tx statistics after wifi started:
 * (1) esp_wifi_enable_rx_statistics(true, true);
 * (2) esp_wifi_enable_tx_statistics(ESP_WIFI_ACI_BE, true);
 */

/*******************************************************
 *                Constants
 *******************************************************/
static const char *TAG = "he_cmd";

/*******************************************************
 *                Structures
 *******************************************************/
typedef struct {
    struct arg_dbl *disable;
    struct arg_end *end;
} wifi_stbc_args_t;

typedef struct {
    struct arg_dbl *disable;
    struct arg_end *end;
} wifi_bmfmee_args_t;

typedef struct {
    struct arg_dbl *ul_mu_disable;
    struct arg_dbl *ul_mu_data_disable;
    struct arg_dbl *ersu_disable;
    struct arg_dbl *report;
    struct arg_dbl *resounding;
    struct arg_end *end;
} wifi_omctrl_args_t;

typedef struct {
    struct arg_dbl *tf_padding;
    struct arg_end *end;
} wifi_tf_padding_args_t;

typedef struct {
    struct arg_int *enable;
    struct arg_int *txq;
    struct arg_end *end;
} wifi_edca_args_t;

typedef struct {
    struct arg_int *reset_timer;
    struct arg_int *read_timer;
    struct arg_end *end;
} wifi_muedca_args_t;

typedef struct {
    struct arg_dbl *enable;
    struct arg_end *end;
} wifi_cca_ignore_args_t;

typedef struct {
    struct arg_str *proto;
    struct arg_end *end;
} wifi_proto_args_t;

typedef struct {
    struct arg_int *val;
    struct arg_end *end;
} wifi_inactive_time_args_t;

typedef struct {
    struct arg_int *ifx;
    struct arg_int *subtype;
    struct arg_int *not_dump_subtype;
    struct arg_end *end;
} wifi_dump_ctrl_frames_args_t;

#if CONFIG_IDF_TARGET_ESP32C5
typedef struct {
    struct arg_int *format;
    struct arg_int *rate;
    struct arg_end *end;
} wifi_sounding_rate_t;

typedef struct {
    struct arg_int *mcs;
    struct arg_int *power;
    struct arg_end *end;
} wifi_tx_pwr_t;
#endif

/*******************************************************
 *                Variable Definitions
 *******************************************************/
static wifi_stbc_args_t stbc_args;
static wifi_bmfmee_args_t bmfmee_args;
static wifi_omctrl_args_t omctrl_args;
static wifi_tf_padding_args_t tf_padding_args;
static wifi_edca_args_t edca_args;
static wifi_cca_ignore_args_t cca_args;
// static static_ip_args_t static_ip_args;
static wifi_inactive_time_args_t inactive_time_args;
static wifi_muedca_args_t muedca_args;
static wifi_dump_ctrl_frames_args_t dump_ctrl_frames_args;
#if CONFIG_IDF_TARGET_ESP32C5
static wifi_tx_pwr_t tx_pwr_args;
static wifi_sounding_rate_t wifi_sounding_rate_args;
#endif
extern EventGroupHandle_t wifi_event_group;
extern const int CONNECTED_BIT;

/*******************************************************
 *                Function Declarations
 *******************************************************/

/*******************************************************
 *                Function Definitions
 *******************************************************/
static int wifi_cmd_set_omc(int argc, char **argv)
{
    //TODO ER-SU
    esp_wifi_htc_omc_t omc = { 0, };
    esp_err_t err = ESP_OK;
    int nerrors = arg_parse(argc, argv, (void **) &omctrl_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, omctrl_args.end, argv[0]);
        return 1;
    }
    do {
        if (!omctrl_args.ul_mu_disable->count && !omctrl_args.ul_mu_data_disable->count) {
            omc.ul_mu_disable = 1;
            omc.ul_mu_data_disable = 0;
            break;
        }
        /* parse inputs */
        if (omctrl_args.ul_mu_disable->count) {
            omc.ul_mu_disable = omctrl_args.ul_mu_disable->dval[0];
        }
        if (omctrl_args.ul_mu_data_disable->count) {
            omc.ul_mu_data_disable = omctrl_args.ul_mu_data_disable->dval[0];
        }
        if (omctrl_args.ersu_disable->count) {
            omc.er_su_disable = omctrl_args.ersu_disable->dval[0];
        }
        if (omctrl_args.resounding->count) {
            omc.dl_mu_mimo_resounding_recommendation = omctrl_args.resounding->dval[0];
        }
    } while (0);

    if (omctrl_args.report->count && omctrl_args.report->dval[0] == 0) {
        /* not report to ap the om control */
        hal_he_set_ul_mu(omc.ul_mu_disable, omc.ul_mu_data_disable);
        ESP_LOGW(TAG, "(omc)(internal)disable ul mu(%d, data:%d) successfully", omc.ul_mu_disable,
                 omc.ul_mu_data_disable);
    } else {
        err = esp_wifi_set_htc_omc(&omc);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "(omc)disable ul mu(%d, data:%d) failed, err:0x%x", omc.ul_mu_disable, omc.ul_mu_data_disable,
                     err);
        } else {
            ESP_LOGW(TAG, "(omc)disable ul mu(%d, data:%d) successfully", omc.ul_mu_disable, omc.ul_mu_data_disable);
        }
    }
    return 0;
}

static int wifi_cmd_edca_tx(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &edca_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, edca_args.end, argv[0]);
        return 1;
    }

    int txq = 2;
    if (!edca_args.enable->count && !edca_args.txq->count) {
        esp_test_disable_edca_tx(txq);
        ESP_LOGW(TAG, "(tx)disable edca, txq[%d]", txq);
        return 0;
    }
    txq = edca_args.txq->count ? edca_args.txq->ival[0] : txq;
    if (edca_args.enable->ival[0] == 0) {
        esp_test_disable_edca_tx(txq);
        ESP_LOGW(TAG, "(tx)disable edca, txq[%d]", txq);
    } else {
        esp_test_enable_edca_tx(txq);
        ESP_LOGW(TAG, "(tx)enable edca, txq[%d]", txq);
    }
    return 0;
}

static int wifi_cmd_set_tf_padding(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &tf_padding_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, tf_padding_args.end, argv[0]);
        return 1;
    }
    if (tf_padding_args.tf_padding->count) {
        esp_wifi_set_tf_padding_duration((int)tf_padding_args.tf_padding->dval[0]);
        ESP_LOGW(TAG, "(test)set trigger frame mac padding duration:%d", (int)tf_padding_args.tf_padding->dval[0]);
    } else {
        printf("Input Error\n");
    }
    return 0;
}

static int wifi_cmd_tb(int argc, char **argv)
{
    dbg_read_axtb_diag();
    dbg_read_ax_diag(1);
    return 0;
}

static int wifi_cmd_stbc(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &stbc_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, stbc_args.end, argv[0]);
        return 1;
    }
    if (stbc_args.disable->count) {
        esp_wifi_enable_rx_stbc(0);
        ESP_LOGI(TAG, "(cfg)disable he stbc");
    } else {
        esp_wifi_enable_rx_stbc(1);
        ESP_LOGI(TAG, "(cfg)enable he stbc");
    }
    return 0;
}

static int wifi_cmd_su_bmfmee(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &bmfmee_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, bmfmee_args.end, argv[0]);
        return 1;
    }
    if (bmfmee_args.disable->count) {
        esp_wifi_enable_su_bmfmee(0);
        ESP_LOGI(TAG, "(cfg)disable he su bmfmee");
    } else {
        esp_wifi_enable_su_bmfmee(1);
        ESP_LOGI(TAG, "(cfg)enable he su bmfmee");
    }
    return 0;
}

static int wifi_cmd_ignore_cca(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &cca_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, cca_args.end, argv[0]);
        return 1;
    }
    if (cca_args.enable->count) {
        dbg_tb_ignore_cca_enable(1);
    } else {
        dbg_tb_ignore_cca_enable(0);
    }
    return 0;
}

extern bool pm_is_waked(void);
extern bool pm_is_sleeping(void);
extern bool pm_is_dream(void);
static int wifi_cmd_get_ps_state(int argc, char **argv)
{
    ESP_LOGW(TAG, "ps: awake:%d, sleep:%d, dream:%d", pm_is_waked(), pm_is_sleeping(), pm_is_dream());
    return 0;
}

// esp_err_t esp_netif_set_static_ip(esp_netif_t *netif_sta, uint32_t ip, uint32_t gw,
//                                   uint32_t netmask)
// {
//     esp_netif_dhcpc_stop(netif_sta);
//     esp_netif_ip_info_t ip_info;
//     esp_netif_set_ip4_addr(&ip_info.ip, ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF,
//                            (ip >> 24) & 0xFF);
//     esp_netif_set_ip4_addr(&ip_info.gw, gw & 0xFF, (gw >> 8) & 0xFF, (gw >> 16) & 0xFF,
//                            (gw >> 24) & 0xFF);
//     esp_netif_set_ip4_addr(&ip_info.netmask, netmask & 0xFF, (netmask >> 8) & 0xFF,
//                            (netmask >> 16) & 0xFF, (netmask >> 24) & 0xFF);
//     esp_netif_set_ip_info(netif_sta, &ip_info);
//     return ESP_OK;
// }

// static int wifi_cmd_set_ip(int argc, char **argv)
// {
//     uint32_t ip = 0, gw = 0, netmask = 0;
//     int nerrors = arg_parse(argc, argv, (void **) &static_ip_args);
//     if (nerrors != 0) {
//         arg_print_errors(stderr, static_ip_args.end, argv[0]);
//         return 0;
//     }
//     if (static_ip_args.ip->count != 0) {
//         ip = esp_ip4addr_aton(static_ip_args.ip->sval[0]);
//     }
//     if (static_ip_args.gw->count != 0) {
//         gw = esp_ip4addr_aton(static_ip_args.gw->sval[0]);
//     }
//     if (static_ip_args.netmask->count != 0) {
//         netmask = esp_ip4addr_aton(static_ip_args.netmask->sval[0]);
//     }
//     if (!ip || !netmask) {
//         return 0;
//     }
//     /* set static IP settings */
//     esp_netif_set_static_ip(netif_sta, ip, gw, netmask);

//     ESP_LOGD(TAG, "ip:%d.%d.%d.%d, gateway:%d.%d.%d.%d, netmask:%d.%d.%d.%d,", ip & 0xFF,
//              (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF, gw & 0xFF, (gw >> 8) & 0xFF,
//              (gw >> 16) & 0xFF, (gw >> 24) & 0xFF, netmask & 0xFF, (netmask >> 8) & 0xFF,
//              (netmask >> 16) & 0xFF, (netmask >> 24) & 0xFF);
//     return 0;
// }

static int wifi_cmd_inactive_time(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&inactive_time_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, inactive_time_args.end, argv[0]);
        return 1;
    }

    esp_err_t err = ESP_OK;
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if ((mode & WIFI_MODE_AP) && inactive_time_args.val->count) {
        err = esp_wifi_set_inactive_time(WIFI_IF_AP, inactive_time_args.val->ival[0]);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "set softAP inactive time to %d seconds, err:0x%x\n", inactive_time_args.val->ival[0], err);
        } else {
            ESP_LOGI(TAG, "set softAP inactive time to %d seconds, err:0x%x\n", inactive_time_args.val->ival[0]);
        }
    }
    //WIFI_MODE_STA or WIFI_MODE_APSTA
    if ((mode & WIFI_MODE_STA) && inactive_time_args.val->count) {
        err = esp_wifi_set_inactive_time(WIFI_IF_STA, inactive_time_args.val->ival[0]);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "set STA inactive time to %d seconds, err:0x%x\n", inactive_time_args.val->ival[0], err);
        } else {
            ESP_LOGI(TAG, "set STA inactive time to %d seconds, err:0x%x\n", inactive_time_args.val->ival[0]);
        }
    }
    uint16_t secs = 0;
    esp_wifi_get_inactive_time(WIFI_IF_STA, &secs);
    printf("inactive time: %d seconds\n", secs);
    return 0;
}

#if CONFIG_IDF_TARGET_ESP32C5
static int wifi_cmd_sounding_rate(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&wifi_sounding_rate_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, wifi_sounding_rate_args.end, argv[0]);
        return 1;
    }

    if (wifi_sounding_rate_args.format->count && wifi_sounding_rate_args.rate->count) {
        if (wifi_sounding_rate_args.format->ival[0] == SIG_MODE_LEGACY) {
            if (wifi_sounding_rate_args.rate->ival[0] < WIFI_PHY_RATE_MCS0_LGI &&
                    wifi_sounding_rate_args.rate->ival[0] >= 0) {
                hal_he_set_bf_report_rate(SIG_MODE_LEGACY, wifi_sounding_rate_args.rate->ival[0], false, false);
            } else {
                ESP_LOGW(TAG, "need correct legacy rate(0-%d)", WIFI_PHY_RATE_9M);
            }
        } else {
            if (wifi_sounding_rate_args.rate->ival[0] >= WIFI_PHY_RATE_MCS0_LGI &&
                    wifi_sounding_rate_args.rate->ival[0] <= WIFI_PHY_RATE_MCS9_SGI) {
                hal_he_set_bf_report_rate(wifi_sounding_rate_args.format->ival[0],
                                          wifi_sounding_rate_args.rate->ival[0], false, false);
            } else {
                ESP_LOGW(TAG, "need correct mcs(%d-%d)", WIFI_PHY_RATE_MCS0_LGI, WIFI_PHY_RATE_MCS9_SGI);
            }
        }
    } else {
        ESP_LOGW(TAG, "set rate fail");
    }
    return 0;
}

static int wifi_cmd_set_tx_pwr(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&tx_pwr_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, tx_pwr_args.end, argv[0]);
        return 1;
    }
    if (tx_pwr_args.mcs->count && tx_pwr_args.power->count) {
        if (tx_pwr_args.mcs->ival[0] <= 9 && tx_pwr_args.mcs->ival[0] >= 0) {
            if (tx_pwr_args.power->ival[0] >= -13 &&
                    tx_pwr_args.power->ival[0] <= 20) {
                hal_set_tx_pwr(tx_pwr_args.mcs->ival[0] + WIFI_PHY_RATE_MCS0_LGI, tx_pwr_args.power->ival[0]);
                ESP_LOGW(TAG, "set MCS%d TX PWR to %d", tx_pwr_args.mcs->ival[0], tx_pwr_args.power->ival[0]);
            } else if (tx_pwr_args.power->ival[0] == 0xff) {
                hal_set_tx_pwr(tx_pwr_args.mcs->ival[0] + WIFI_PHY_RATE_MCS0_LGI, tx_pwr_args.power->ival[0]);
                ESP_LOGW(TAG, "set MCS%d TX PWR to default value", tx_pwr_args.mcs->ival[0], tx_pwr_args.power->ival[0]);
            }
        } else {
            ESP_LOGW(TAG, "Set TX power fail, MCS should in range [0,9], power should in range [-13, 30] or set 0xFF for default");
        }
    }
    return 0;
}
#endif

static int wifi_cmd_muedca(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&muedca_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, muedca_args.end, argv[0]);
        return 1;
    }

    if (muedca_args.reset_timer->count) {
        esp_wifi_sta_reset_muedca_timer(muedca_args.reset_timer->ival[0]);
    }

    uint8_t aci_bitmap = 0;
    if (muedca_args.read_timer->count) {
        aci_bitmap = muedca_args.read_timer->ival[0];
        if (aci_bitmap & BIT(0)) {
            dbg_read_muedca_timer(3);
        }
        if (aci_bitmap & BIT(1)) {
            dbg_read_muedca_timer(2);
        }
        if (aci_bitmap & BIT(2)) {
            dbg_read_muedca_timer(1);
        }
        if (aci_bitmap & BIT(3)) {
            dbg_read_muedca_timer(0);
        }
    }
    return 0;
}

static int wifi_read_avgsnr(int argc, char **argv)
{
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (WIFI_MODE_AP == mode) {
        // TODO
        ESP_LOGI(TAG, "WIFI_MODE_AP, not support");
    } else if (WIFI_MODE_STA == mode || WIFI_MODE_APSTA == mode) {
        printf("%.2f\n", esp_test_get_bfr_avgsnr());
    } else {
        ESP_LOGI(TAG, "NULL mode");
    }
    return 0;
}

extern void hal_set_dump_ctrl_frame_cfg(uint8_t, uint32_t);
extern uint32_t hal_get_dump_ctrl_frame_cfg(uint8_t);
static int wifi_cmd_dump_ctrl_frames(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &dump_ctrl_frames_args);

    if (nerrors != 0) {
        arg_print_errors(stderr, dump_ctrl_frames_args.end, argv[0]);
        return 1;
    }

    uint8_t ifx = 0;
    uint32_t subtype = 0; //CTRL subtype[0, 2] reserved
    uint32_t subtype_bitmap = hal_get_dump_ctrl_frame_cfg(ifx);
    if (dump_ctrl_frames_args.ifx->count) {
        ifx = dump_ctrl_frames_args.ifx->ival[0];
        if (dump_ctrl_frames_args.subtype->count) {
            subtype = dump_ctrl_frames_args.subtype->ival[0];
            subtype_bitmap |= BIT(subtype);
        }
        if (dump_ctrl_frames_args.not_dump_subtype->count) {
            subtype = dump_ctrl_frames_args.not_dump_subtype->ival[0];
            subtype_bitmap &= ~BIT(subtype);
        }
        if (subtype) {
            ESP_LOGW(TAG, "(ctrl)interface:%d, dump subtype:0x%x, subtype_bitmap:0x%x -> 0x%x", ifx, (int) subtype, (int) hal_get_dump_ctrl_frame_cfg(ifx), (int) subtype_bitmap);
            hal_set_dump_ctrl_frame_cfg(ifx, subtype_bitmap);
        }
    }
    ESP_LOGW(TAG, "(ctrl)sta, dump subtype_bitmap:0x%x", (int) hal_get_dump_ctrl_frame_cfg(ifx));
    return 0;
}

void wifi_cmd_register_he_debug(void)
{
    /* disable edca */
    if (!edca_args.end) {
        edca_args.enable = arg_int0("e", "enable", "[enable]", "enable edca tx");
        edca_args.txq = arg_int0("q", "txq", "[txq]", "enable edca txq");
        edca_args.end = arg_end(1);
    }
    const esp_console_cmd_t edca_cmd = {
        .command = "edca",
        .help = "enable/disable edca",
        .hint = NULL,
        .func = &wifi_cmd_edca_tx,
        .argtable = &edca_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&edca_cmd));
    /* om control */
    if (!omctrl_args.end) {
        omctrl_args.ul_mu_disable = arg_dbl0("u", "ulmu", "[ulmu]", "disable ul mu");
        omctrl_args.ul_mu_data_disable = arg_dbl0("d", "uldata", "[uldata]", "disable ul mu data");
        omctrl_args.ersu_disable = arg_dbl0("e", "ersu", "[ersu]", "disable ersu");
        omctrl_args.report = arg_dbl0("r", "report", "[report]", "report om control to ap");
        omctrl_args.resounding = arg_dbl0("s", "resounding", "[resounding]", "DL MU-MIMO resound Recoummendation");
        omctrl_args.end = arg_end(1);
    }
    const esp_console_cmd_t omctrl_cmd = {
        .command = "omc",
        .help = "om control",
        .hint = NULL,
        .func = &wifi_cmd_set_omc,
        .argtable = &omctrl_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&omctrl_cmd));
    /* stbc */
    if (!stbc_args.end) {
        stbc_args.disable = arg_dbl0("d", "disable", "[disable]", "disable stbc");
        stbc_args.end = arg_end(1);
    }
    const esp_console_cmd_t stbc_cmd = {
        .command = "stbc",
        .help = "configure stbc",
        .hint = NULL,
        .func = &wifi_cmd_stbc,
        .argtable = &stbc_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&stbc_cmd));
    /* su bmfmee */
    if (!bmfmee_args.end) {
        bmfmee_args.disable = arg_dbl0("d", "disable", "[disable]", "disable bmfmee");
        bmfmee_args.end = arg_end(1);
    }
    const esp_console_cmd_t bmfmee_cmd = {
        .command = "bmfmee",
        .help = "configure su bmfmee",
        .hint = NULL,
        .func = &wifi_cmd_su_bmfmee,
        .argtable = &bmfmee_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&bmfmee_cmd));
    /* set trigger frame mac padding duration */
    if (!tf_padding_args.end) {
        tf_padding_args.tf_padding = arg_dbl0("p", "padding", "[padding]", "set trigger frame mac padding duration");
        tf_padding_args.end = arg_end(1);
    }
    const esp_console_cmd_t tf_padding_cmd = {
        .command = "tf",
        .help = "set padding",
        .hint = NULL,
        .func = &wifi_cmd_set_tf_padding,
        .argtable = &tf_padding_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&tf_padding_cmd));
    /* ignore cca */
    if (!cca_args.end) {
        cca_args.enable = arg_dbl0("e", "enable", "[enable]", "enable ignore cca");
        cca_args.end = arg_end(1);
    }
    const esp_console_cmd_t cca_cmd = {
        .command = "cca",
        .help = "ignore cca",
        .hint = NULL,
        .func = &wifi_cmd_ignore_cca,
        .argtable = &cca_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cca_cmd));
    /* dump tx tb ppdu */
    const esp_console_cmd_t tb_cmd = {
        .command = "tb",
        .help = "dump tx tb ppdu",
        .hint = NULL,
        .func = &wifi_cmd_tb,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&tb_cmd));
    /* get ps state */
    const esp_console_cmd_t pss_cmd = {
        .command = "pss",
        .help = "get ps state",
        .hint = NULL,
        .func = &wifi_cmd_get_ps_state,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&pss_cmd));
    /* inactive time */
    if (!inactive_time_args.end) {
        inactive_time_args.val = arg_int0("t", "time", "time", "set inactive time, in seconds");
        inactive_time_args.end = arg_end(1);
    }
    const esp_console_cmd_t inactive_cmd = {
        .command = "inactive",
        .help = "inactive time, unit: seconds",
        .hint = NULL,
        .func = &wifi_cmd_inactive_time,
        .argtable = &inactive_time_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&inactive_cmd));

#if CONFIG_IDF_TARGET_ESP32C5
    /* set beamforming report rate */
    if (!wifi_sounding_rate_args.end) {
        wifi_sounding_rate_args.format = arg_int0("f", "format", "format", "set format");
        wifi_sounding_rate_args.rate = arg_int0("r", "rate", "rate", "set rate");
        wifi_sounding_rate_args.end = arg_end(1);
    }
    const esp_console_cmd_t sounding_rate_cmd = {
        .command = "sounding",
        .help = "set beamforming report rate",
        .hint = NULL,
        .func = &wifi_cmd_sounding_rate,
        .argtable = &wifi_sounding_rate_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&sounding_rate_cmd));

    /* tx_pwr */
    if (!tx_pwr_args.end) {
        tx_pwr_args.mcs = arg_int0("m", NULL, "[0, 9]", "force tx power on MCSX");
        tx_pwr_args.power = arg_int0("p", NULL, "[-13, 20]", "set max power, set 0xFF for default");
        tx_pwr_args.end = arg_end(1);
    }
    const esp_console_cmd_t reg_tx_pwr_cmd = {
        .command = "txpwr",
        .help = "force tx power on MCSX",
        .hint = NULL,
        .func = &wifi_cmd_set_tx_pwr,
        .argtable = &tx_pwr_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&reg_tx_pwr_cmd));
#endif
    /* muedca */
    if (!muedca_args.end) {
        muedca_args.reset_timer = arg_int0("r", NULL, "reset timer", "reset muedca timer");
        muedca_args.read_timer = arg_int0("d", NULL, "read timer", "read muedca timer");
        muedca_args.end = arg_end(1);
    }
    const esp_console_cmd_t reg_muedca_cmd = {
        .command = "muedca",
        .help = "Reset/Read muedca timer",
        .hint = NULL,
        .func = &wifi_cmd_muedca,
        .argtable = &muedca_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&reg_muedca_cmd));

    /* avgSNR */
    const esp_console_cmd_t avgsnr_cmd = {
        .command = "avgsnr",
        .help = "show avgSnr in beamforming memory",
        .hint = NULL,
        .func = &wifi_read_avgsnr,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&avgsnr_cmd));

    /* dump control frames dump_ctrl */
    if (!dump_ctrl_frames_args.end) {
        dump_ctrl_frames_args.ifx = arg_int0("i", NULL, "<ifx>", "specify an interface for Control frames dump");
        dump_ctrl_frames_args.subtype = arg_int0("s", NULL, "<subtype>", "specify a subtype for Control frames dump");
        dump_ctrl_frames_args.not_dump_subtype = arg_int0("d", NULL, "<subtype>", "cancel dump a specified subtype");
        dump_ctrl_frames_args.end = arg_end(1);
    }
    const esp_console_cmd_t ctrl_cmd = {
        .command = "dump_ctrl",
        .help = "dump Control frames",
        .hint = NULL,
        .func = &wifi_cmd_dump_ctrl_frames,
        .argtable = &dump_ctrl_frames_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&ctrl_cmd));
}

#endif /* WIFI_CMD_HE_DEBUG_ENABLED */
