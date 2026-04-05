/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"

#include "wifi_cmd.h"

#if CONFIG_WIFI_CMD_ENABLE_WIFI_STATS
#if CONFIG_ESP_WIFI_ENABLE_WIFI_TX_STATS || CONFIG_ESP_WIFI_ENABLE_WIFI_RX_STATS

#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_private/esp_wifi_he_private.h"
#include "esp_wifi_he.h"
#include "esp_wifi_he_types.h"

#ifndef APP_TAG
#define APP_TAG "STATS"
#endif

/*******************************************************
 *                Macros
 *******************************************************/
/*
 * enable/disable rx/tx statistics after Wi-Fi started:
 * (1) esp_wifi_enable_rx_statistics(true, true);            //rx_stats=true, rx_mu_stats=true
 * (2) esp_wifi_enable_tx_statistics(ESP_WIFI_ACI_BE, true); //aci=ESP_WIFI_ACI_BE, tx_stats=true
 */

/*******************************************************
 *                Structures
 *******************************************************/
typedef struct {
    struct arg_str *direction;
    struct arg_str *action;
    struct arg_str *aci;
    struct arg_end *end;
} wifi_stats_args_t;

typedef struct {
    uint8_t value;
    const char *aci;
} app_wifi_aci_t;

/*******************************************************
 *                Variable Definitions
 *******************************************************/
#if CONFIG_ESP_WIFI_ENABLE_DUMP_MU_CFO
#define MU_CFO_COUNT    (1000)
extern int16_t g_mu_cfo[MU_CFO_COUNT];
extern int g_mu_cfo_idx;
extern bool g_mu_cfo_dump_disable;
#endif

static wifi_stats_args_t wifi_stats_args;

static const app_wifi_aci_t wifi_aci_map[] = {
    {ESP_WIFI_ACI_VO, "vo"},
    {ESP_WIFI_ACI_VI, "vi"},
    {ESP_WIFI_ACI_BE, "be"},
    {ESP_WIFI_ACI_BK, "bk"},
};

/*******************************************************
 *                Function Declarations
 *******************************************************/
void print_hw_rx_statistics(void);

/*******************************************************
 *                Function Definitions
 *******************************************************/
const char *tx_fail_error2str(esp_test_tx_fail_error_t error)
{
    switch (error) {
    case TEST_TX_FAIL_ERROR_H00:
        return "0x00";
    case TEST_TX_FAIL_ERROR_H53:
        return "0x53";
    case TEST_TX_FAIL_ERROR_H63:
        return "0x63";
    case TEST_TX_FAIL_ERROR_H75:
        return "0x75";
    case TEST_TX_FAIL_ERROR_H41:
        return "0x41";
    case TEST_TX_FAIL_ERROR_H42:
        return "0x42";
    case TEST_TX_FAIL_ERROR_H47:
        return "0x47";
    case TEST_TX_FAIL_ERROR_H80:
        return "0x80";
    case TEST_TX_FAIL_ERROR_H5A:
        return "0x5A";
    case TEST_TX_FAIL_ERROR_HXX:
        return "Others";
    case TEST_TX_FAIL_ERROR_MAX:
        return "Undefined";
    }
    return "Undefined";
}

const char *tx_fail_match2str(esp_test_tx_fail_match_t match)
{
    switch (match) {
    case TEST_TX_WAIT_MATCH:
        return "MATCH";
    case TEST_TX_WAIT_NOT2SELF:
        return "NOT2SELF";
    case TEST_TX_MISMATCH:
        return "MISMATCH";
    case TEST_TX_WAIT_TIMEOUT:
        return "TIMEOUT";
    case TEST_TX_WAIT_MAX:
        return "Undefined";
    }
    return "Undefined";
}

const char *tx_fail_state2str(esp_test_tx_fail_state_t state)
{
    switch (state) {
    case TEST_TX_SUCCESS:
        return "TX Success";
    case TEST_TX_FAIL_RTS:
        return "TX RTS";
    case TEST_TX_WAIT_CTS:    //RX
        return "Wait CTS";
    case TEST_TX_FAIL_CTS:
        return "TX RTS";
    case TEST_TX_FAIL_DATA:
        return "TX DATA";
    case TEST_TX_WAIT_ACK:    //RX
        return "Wait ACK/BA";
    case TEST_TX_FAIL_MAX:
        return "Undefined";
    }
    return "Undefined";
}

static uint8_t app_wifi_aci_str2num(const char *aci_str)
{
    uint8_t aci;

    for (aci = 0; aci < 4; aci++) {
        if (strcmp(wifi_aci_map[aci].aci, aci_str) == 0) {
            break;
        }
    }
    if (aci == 4) {
        return ESP_WIFI_ACI_MAX;
    }
    return wifi_aci_map[aci].value;
}

int wifi_cmd_clr_tx_statistics(int argc, char **argv)
{
    ESP_LOGW(APP_TAG, "Clear tx statistics");
    int i;
    for (i = 0; i < 4; i++) {
        esp_wifi_clr_tx_statistics(i); //BE
        esp_wifi_clr_tx_tb_statistics(i);
    }
    esp_test_clr_hw_statistics();
    return 0;
}

void print_hw_tb_statistics(void)
{
    esp_test_hw_tb_statistics_t hw_tb_stats = { 0, };
    esp_test_get_hw_tb_statistics(&hw_tb_stats);
    printf("(test)rx_trig:%d, tx_bfrpt:%d,"
#if CONFIG_SOC_WIFI_MAC_VERSION_NUM >= 3
           "vht_bfrpt:%d, polled_vht_bfrpt:%d,"
#endif  /* CONFIG_SOC_WIFI_MAC_VERSION_NUM >= 3 */
           "tb_times:%d, tb_qos_null:%d, tb_qos_data:%d, tb_cca_cancel:%d, tb_sifs_abort:%d, tb_pwr_outof_range:%d, min heap:%d\n",
           hw_tb_stats.rx_trig,
           hw_tb_stats.tx_bfrpt, //including TB and Non-TB
#if CONFIG_SOC_WIFI_MAC_VERSION_NUM >= 3
           hw_tb_stats.vht_bfrpt,
           hw_tb_stats.polled_vht_bfrpt,
#endif  /* CONFIG_SOC_WIFI_MAC_VERSION_NUM >= 3 */
           hw_tb_stats.tb_times,
           hw_tb_stats.tb_qos_null,
           hw_tb_stats.tb_times - hw_tb_stats.tb_qos_null,
           hw_tb_stats.tb_cca_cancel,
           hw_tb_stats.tb_sifs_abort,
           hw_tb_stats.tb_pwr_outof_range,
           heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
}

int wifi_cmd_get_tx_statistics(int argc, char **argv)
{
    uint8_t i, h, j, k;

    ESP_LOGW(APP_TAG, "Get tx statistics\n");
    esp_test_tx_tb_statistics_t *tb_stats = (esp_test_tx_tb_statistics_t *) malloc(sizeof(esp_test_tx_tb_statistics_t));
    if (tb_stats == NULL) {
        ESP_LOGE(APP_TAG, "malloc tb_stats failed");
        return ESP_FAIL;
    }
    esp_test_tx_statistics_t *tx_stats = (esp_test_tx_statistics_t *) malloc(sizeof(esp_test_tx_statistics_t));
    if (tx_stats == NULL) {
        ESP_LOGE(APP_TAG, "malloc tx_stats failed");
        free(tb_stats);
        return ESP_FAIL;
    }
    esp_test_tx_fail_statistics_t *tx_fail = (esp_test_tx_fail_statistics_t *) malloc(TEST_TX_FAIL_MAX * sizeof(esp_test_tx_fail_statistics_t));
    if (tx_fail == NULL) {
        ESP_LOGE(APP_TAG, "malloc tx_fail failed");
        free(tb_stats);
        free(tx_stats);
        return ESP_FAIL;
    }
    uint8_t aci_bitmap = BIT(2);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    aci_bitmap = esp_wifi_get_tx_statistics_ena_acibitmap();
#endif
    print_hw_tb_statistics();
    //only check BE
    for (i = 0; i < 4; i++) {
        if ((aci_bitmap & BIT(i)) == 0) {
            continue;
        }
        esp_wifi_get_tx_statistics(i, tx_stats, (esp_test_tx_fail_statistics_t *) tx_fail);
        if (tx_stats->tx_enable == 0) {
            continue;
        }
        esp_wifi_get_tx_tb_statistics(i, tb_stats);
        /* TB */
        printf("(test)aci:%d, tb(suc:%d, ack:%d, err:%d), "
               "count(suc:%d, ack:%d, err:%d, tot:%d, max_sent:%d)\n",
               i,
               tb_stats->complete_suc_tb,
               tb_stats->complete_ack_tb,
               tb_stats->complete_err_tb,
               tb_stats->complete_tb_suc_count,
               tb_stats->complete_tb_ack_count,
               tb_stats->complete_tb_err_count,
               tb_stats->complete_tb_tot_count,
               tb_stats->complete_tb_pack_sent);

        int tot_tx_times = tx_stats->tb_times + (tx_stats->tx_enable - tx_stats->tb_last); //TB + EDCA
        int tot_fail = tx_fail[1].count + tx_fail[2].count + tx_fail[3].count + tx_fail[4].count + tx_fail[5].count;
        printf("(test)aci:%d, enable:%d, complete:%d, tb_times:%d, tb_last:%d, edca:%d, "
               "succ:%d, fail(%d,%d,%d, cts:%d/%2.2f%%, ack:%d/%2.2f%%, tot:%d, %.2f%%), "
               "edca(ack:%d, ba:%d), tb(hw-ba:%d, sw-ba:%d)\n",
               i, tx_stats->tx_enable,
               tx_stats->tx_complete,
               tx_stats->tb_times,
               tx_stats->tb_last,
               tx_stats->tx_enable - tx_stats->tb_last,
               tx_fail[0].count,
               tx_fail[1].count,
               tx_fail[3].count,
               tx_fail[4].count,
               tx_fail[2].count,
               (float)((float) tx_fail[2].count / (float) tot_tx_times) * 100,  //rx cts
               tx_fail[5].count, (float)((float) tx_fail[5].count / (float) tot_tx_times) * 100,  //rx ack
               tot_fail,
               (float)((float) tot_fail / (float) tot_tx_times) * 100,
               tx_stats->rx_ack,
               tx_stats->rx_ba,
               tx_stats->tb_rx_ba, //including ACKs
               tx_stats->rx_dump_ba);

        printf("(test)aci:%d, txFrames:%d, s-mpdu:%d(%.2f%%), "
               "bitmap(max:%d, min:%d, tot:%d, avg:%.2f), "
               "retry(edca:%d, tb:%d, %.2f%%), collision:%d, timeout:%d\n",
               i,
               tx_stats->tx_succ,
               tx_stats->rx_ack,
               ((float)(tx_stats->rx_ack) / (float) tot_tx_times) * 100,
               tx_stats->rx_max_bitmap,
               tx_stats->rx_min_bitmap,
               tx_stats->rx_tot_bitmap,
               (float) tx_stats->rx_tot_bitmap / (float)(tx_stats->tb_rx_ba + tx_stats->rx_ba),
               tx_stats->retry_edca, tx_stats->retry_tb, (float)(tx_stats->retry_edca + tx_stats->retry_tb) / (float) tx_stats->tx_succ * 100,
               tx_stats->collision, tx_stats->timeout);

        float tot_rtt_ms = (float) tx_stats->tx_tot_rtt / (float) 1000;
        printf("(test)aci:%d, seqno_rtt[%d,%d], hw_rtt[%d, %d], muedca[enable:%d, times:%d, %.2f, %.2f, tot:%.2f], avg:%.3f ms, tot:%.3f secs\n",
               i,
               tx_stats->tx_seq_min_rtt,
               tx_stats->tx_seq_max_rtt,
               tx_stats->tx_min_rtt,
               tx_stats->tx_max_rtt,
               tx_stats->tx_muedca_enable,
               tx_stats->muedca_times,
               (float) tx_stats->tx_min_muedca_time / (float) 1000,
               (float) tx_stats->tx_max_muedca_time / (float) 1000,
               (float) tx_stats->tx_tot_muedca_time / (float) 1000, //ms
               (float) tot_rtt_ms / (float) tot_tx_times, //ms
               (float) tot_rtt_ms / (float) 1000); //seconds
        /* fail state */
        for (h = 1; h < TEST_TX_FAIL_MAX; h++) { //state
            for (j = 0; j < TEST_TX_WAIT_MAX; j++) { //match
                for (k = 0; k < TEST_TX_FAIL_ERROR_MAX; k++) { //error
                    if (tx_fail[h].match[j][k]) {
                        printf("(test)[%d][%d][%d](%16s + %16s + %16s)%3d/%3d(%.2f%%)\n", h, j, k, tx_fail_state2str(h),
                               tx_fail_match2str(j), tx_fail_error2str(k),
                               tx_fail[h].match[j][k], tx_fail[h].count,
                               ((float) tx_fail[h].match[j][k] / (float) tx_fail[h].count) * 100);
                    }
                }
            }
        }
    }
    printf("\n");
    /* hw rx statistics */
    print_hw_rx_statistics();
    wifi_cmd_clr_tx_statistics(0, 0);

    free(tb_stats);
    free(tx_stats);
    free(tx_fail);
    return 0;
}

void print_rx_statistics_nonmimo(const esp_test_rx_mu_statistics_t *mu_stats)
{
    if (!mu_stats->nonmimo_rx) {
        return;
    }

    int i, j;
    int tot_rx_nonmimo = 0;
    ESP_LOGW(APP_TAG, "(nonmimo)dut rx:%d", mu_stats->nonmimo_rx);
#if CONFIG_SOC_WIFI_MAC_VERSION_NUM >= 3
    ESP_LOGW(APP_TAG, "(nonmimo)bcast_ru(id_0:%d, id_bssidx:%d, id_2047:%d), unallocated_ru_id_2046:%d",
             mu_stats->mu_bru_id_0, mu_stats->mu_bru_id_bssidx, mu_stats->mu_bru_id_2047, mu_stats->mu_uru_id_2046);
#else
    ESP_LOGW(APP_TAG, "(nonmimo)ru_alloc_96_num_2046:%d, ru_alloc_112_num_2046:%d", mu_stats->ru_alloc_96_num_2046, mu_stats->ru_alloc_112_num_2046);
#endif

    ESP_LOGW(APP_TAG, "(nonmimo)sigb, mcs0:%d(%2.2f%%), mcs1:%d(%2.2f%%), mcs2:%d(%2.2f%%), mcs3:%d(%2.2f%%), mcs4:%d(%2.2f%%), mcs5:%d(%2.2f%%)",
             mu_stats->nonmimo_sigb_mcs[0], ((float) mu_stats->nonmimo_sigb_mcs[0] / (float) mu_stats->nonmimo_rx) * 100,
             mu_stats->nonmimo_sigb_mcs[1], ((float) mu_stats->nonmimo_sigb_mcs[1] / (float) mu_stats->nonmimo_rx) * 100,
             mu_stats->nonmimo_sigb_mcs[2], ((float) mu_stats->nonmimo_sigb_mcs[2] / (float) mu_stats->nonmimo_rx) * 100,
             mu_stats->nonmimo_sigb_mcs[3], ((float) mu_stats->nonmimo_sigb_mcs[3] / (float) mu_stats->nonmimo_rx) * 100,
             mu_stats->nonmimo_sigb_mcs[4], ((float) mu_stats->nonmimo_sigb_mcs[4] / (float) mu_stats->nonmimo_rx) * 100,
             mu_stats->nonmimo_sigb_mcs[5], ((float) mu_stats->nonmimo_sigb_mcs[5] / (float) mu_stats->nonmimo_rx) * 100);
    ESP_LOGW(APP_TAG, "(nonmimo)users, num1:%d(%2.2f%%), num2:%d(%2.2f%%), num3:%d(%2.2f%%), num4:%d(%2.2f%%), num5:%d(%2.2f%%), num6:%d(%2.2f%%), num7:%d(%2.2f%%), num8:%d(%2.2f%%), num9:%d(%2.2f%%)",
             mu_stats->nonmimo_user_num_occu[0], ((float) mu_stats->nonmimo_user_num_occu[0] / (float) mu_stats->nonmimo_rx) * 100,
             mu_stats->nonmimo_user_num_occu[1], ((float) mu_stats->nonmimo_user_num_occu[1] / (float) mu_stats->nonmimo_rx) * 100,
             mu_stats->nonmimo_user_num_occu[2], ((float) mu_stats->nonmimo_user_num_occu[2] / (float) mu_stats->nonmimo_rx) * 100,
             mu_stats->nonmimo_user_num_occu[3], ((float) mu_stats->nonmimo_user_num_occu[3] / (float) mu_stats->nonmimo_rx) * 100,
             mu_stats->nonmimo_user_num_occu[4], ((float) mu_stats->nonmimo_user_num_occu[4] / (float) mu_stats->nonmimo_rx) * 100,
             mu_stats->nonmimo_user_num_occu[5], ((float) mu_stats->nonmimo_user_num_occu[5] / (float) mu_stats->nonmimo_rx) * 100,
             mu_stats->nonmimo_user_num_occu[6], ((float) mu_stats->nonmimo_user_num_occu[6] / (float) mu_stats->nonmimo_rx) * 100,
             mu_stats->nonmimo_user_num_occu[7], ((float) mu_stats->nonmimo_user_num_occu[7] / (float) mu_stats->nonmimo_rx) * 100,
             mu_stats->nonmimo_user_num_occu[8], ((float) mu_stats->nonmimo_user_num_occu[8] / (float) mu_stats->nonmimo_rx) * 100);
    for (i = 0; i < 256; i++) {
        for (j = 0; j < 9; j++) {
            if (!mu_stats->nonmimo_ru_alloc[i][j]) {
                continue;
            }
            ESP_LOGI(APP_TAG, "(nonmimo)ru_allocation:0x%2x(%3d), position:%d, %5d(%2.2f%%)", i, i, j + 1, mu_stats->nonmimo_ru_alloc[i][j],
                     ((float) mu_stats->nonmimo_ru_alloc[i][j] / (float) mu_stats->nonmimo_rx) * 100);
        }
    }
    for (i = 0; i < ESP_TEST_RX_MU_USER_NUM; i++) {
        if (!mu_stats->nonmimo[i].aid) {
            continue;
        }
        if (mu_stats->aid != mu_stats->nonmimo[i].aid) {
            continue;
        }
        tot_rx_nonmimo = mu_stats->nonmimo[i].occu_nsts[0] + mu_stats->nonmimo[i].occu_nsts[1] + mu_stats->nonmimo[i].occu_nsts[2] + mu_stats->nonmimo[i].occu_nsts[3];
        printf("[%d]%said:0x%x, txbf:%d, dcm:%d\n", i, (mu_stats->aid == mu_stats->nonmimo[i].aid) ? "#" : " ", mu_stats->nonmimo[i].aid,
               mu_stats->nonmimo[i].txbf, mu_stats->nonmimo[i].dcm);
        printf("[%d]%said:0x%x, "
               "mcs0:%d(%2.2f%%), mcs1:%d(%2.2f%%), mcs2:%d(%2.2f%%), mcs3:%d(%2.2f%%), mcs4:%d(%2.2f%%), "
               "mcs5:%d(%2.2f%%), mcs6:%d(%2.2f%%), mcs7:%d(%2.2f%%), mcs8:%d(%2.2f%%), mcs9:%d(%2.2f%%), "
               "mcs10:%d(%2.2f%%), mcs11:%d(%2.2f%%)\n",
               i, (mu_stats->aid == mu_stats->nonmimo[i].aid) ? "#" : " ", mu_stats->nonmimo[i].aid,
               mu_stats->nonmimo[i].occu_mcs[0], ((float) mu_stats->nonmimo[i].occu_mcs[0] / (float) tot_rx_nonmimo) * 100,
               mu_stats->nonmimo[i].occu_mcs[1], ((float) mu_stats->nonmimo[i].occu_mcs[1] / (float) tot_rx_nonmimo) * 100,
               mu_stats->nonmimo[i].occu_mcs[2], ((float) mu_stats->nonmimo[i].occu_mcs[2] / (float) tot_rx_nonmimo) * 100,
               mu_stats->nonmimo[i].occu_mcs[3], ((float) mu_stats->nonmimo[i].occu_mcs[3] / (float) tot_rx_nonmimo) * 100,
               mu_stats->nonmimo[i].occu_mcs[4], ((float) mu_stats->nonmimo[i].occu_mcs[4] / (float) tot_rx_nonmimo) * 100,
               mu_stats->nonmimo[i].occu_mcs[5], ((float) mu_stats->nonmimo[i].occu_mcs[5] / (float) tot_rx_nonmimo) * 100,
               mu_stats->nonmimo[i].occu_mcs[6], ((float) mu_stats->nonmimo[i].occu_mcs[6] / (float) tot_rx_nonmimo) * 100,
               mu_stats->nonmimo[i].occu_mcs[7], ((float) mu_stats->nonmimo[i].occu_mcs[7] / (float) tot_rx_nonmimo) * 100,
               mu_stats->nonmimo[i].occu_mcs[8], ((float) mu_stats->nonmimo[i].occu_mcs[8] / (float) tot_rx_nonmimo) * 100,
               mu_stats->nonmimo[i].occu_mcs[9], ((float) mu_stats->nonmimo[i].occu_mcs[9] / (float) tot_rx_nonmimo) * 100,
               mu_stats->nonmimo[i].occu_mcs[10], ((float) mu_stats->nonmimo[i].occu_mcs[10] / (float) tot_rx_nonmimo) * 100,
               mu_stats->nonmimo[i].occu_mcs[11], ((float) mu_stats->nonmimo[i].occu_mcs[11] / (float) tot_rx_nonmimo) * 100);
        printf("[%d]%said:0x%x, "
               "nsts0:%d(%2.2f%%), nsts1:%d(%2.2f%%), nsts2:%d(%2.2f%%), nsts3:%d(%2.2f%%)\n",
               i, (mu_stats->aid == mu_stats->nonmimo[i].aid) ? "#" : " ", mu_stats->nonmimo[i].aid,
               mu_stats->nonmimo[i].occu_nsts[0], ((float) mu_stats->nonmimo[i].occu_nsts[0] / (float) tot_rx_nonmimo) * 100,
               mu_stats->nonmimo[i].occu_nsts[1], ((float) mu_stats->nonmimo[i].occu_nsts[1] / (float) tot_rx_nonmimo) * 100,
               mu_stats->nonmimo[i].occu_nsts[2], ((float) mu_stats->nonmimo[i].occu_nsts[2] / (float) tot_rx_nonmimo) * 100,
               mu_stats->nonmimo[i].occu_nsts[3], ((float) mu_stats->nonmimo[i].occu_nsts[3] / (float) tot_rx_nonmimo) * 100);
        printf("[%d]%said:0x%x, "
               "tot_rx_nonmimo:%8d, sta/dut:%2.2f%%\n",
               i, (mu_stats->aid == mu_stats->nonmimo[i].aid) ? "#" : " ", mu_stats->nonmimo[i].aid,
               tot_rx_nonmimo, ((float) tot_rx_nonmimo / (float) mu_stats->nonmimo_rx) * 100);
    }
#if CONFIG_ESP_WIFI_ENABLE_DUMP_MU_CFO
    //dump cfo
    if (g_mu_cfo_dump_disable == true) {
        ESP_LOGW(APP_TAG, "CFO, g_mu_cfo_dump_disable:%d, g_mu_cfo_idx:%d", g_mu_cfo_dump_disable, g_mu_cfo_idx);
        return;
    }
    int k;
    for (k = 0; k < 1000; k ++) {
        printf("[%3d]%d\r\n", k, mu_cfo[k]);
    }
    printf("\r\n");
#endif
}

void print_rx_statistics_mimo(const esp_test_rx_mu_statistics_t *mu_stats)
{
    if (!mu_stats->mimo_rx) {
        return;
    }

    int i;
    int tot_rx_mimo = 0;
    ESP_LOGW(APP_TAG, "(mimo)dut rx:%d", mu_stats->mimo_rx);
    ESP_LOGW(APP_TAG, "(mimo)sigb, mcs0:%d(%2.2f%%), mcs1:%d(%2.2f%%), mcs2:%d(%2.2f%%), mcs3:%d(%2.2f%%), mcs4:%d(%2.2f%%), mcs5:%d(%2.2f%%)",
             mu_stats->mimo_sigb_mcs[0], ((float) mu_stats->mimo_sigb_mcs[0] / (float) mu_stats->mimo_rx) * 100,
             mu_stats->mimo_sigb_mcs[1], ((float) mu_stats->mimo_sigb_mcs[1] / (float) mu_stats->mimo_rx) * 100,
             mu_stats->mimo_sigb_mcs[2], ((float) mu_stats->mimo_sigb_mcs[2] / (float) mu_stats->mimo_rx) * 100,
             mu_stats->mimo_sigb_mcs[3], ((float) mu_stats->mimo_sigb_mcs[3] / (float) mu_stats->mimo_rx) * 100,
             mu_stats->mimo_sigb_mcs[4], ((float) mu_stats->mimo_sigb_mcs[4] / (float) mu_stats->mimo_rx) * 100,
             mu_stats->mimo_sigb_mcs[5], ((float) mu_stats->mimo_sigb_mcs[5] / (float) mu_stats->mimo_rx) * 100);
    ESP_LOGW(APP_TAG, "(mimo)users num2:%d(%2.2f%%), num3:%d(%2.2f%%), num4:%d(%2.2f%%), num5:%d(%2.2f%%), num6:%d(%2.2f%%), num7:%d(%2.2f%%), num8:%d(%2.2f%%)",
             mu_stats->mimo_user_num_occu[0], ((float) mu_stats->mimo_user_num_occu[0] / (float) mu_stats->mimo_rx) * 100,
             mu_stats->mimo_user_num_occu[1], ((float) mu_stats->mimo_user_num_occu[1] / (float) mu_stats->mimo_rx) * 100,
             mu_stats->mimo_user_num_occu[2], ((float) mu_stats->mimo_user_num_occu[2] / (float) mu_stats->mimo_rx) * 100,
             mu_stats->mimo_user_num_occu[3], ((float) mu_stats->mimo_user_num_occu[3] / (float) mu_stats->mimo_rx) * 100,
             mu_stats->mimo_user_num_occu[4], ((float) mu_stats->mimo_user_num_occu[4] / (float) mu_stats->mimo_rx) * 100,
             mu_stats->mimo_user_num_occu[5], ((float) mu_stats->mimo_user_num_occu[5] / (float) mu_stats->mimo_rx) * 100,
             mu_stats->mimo_user_num_occu[6], ((float) mu_stats->mimo_user_num_occu[6] / (float) mu_stats->mimo_rx) * 100);
    for (i = 0; i < ESP_TEST_RX_MU_USER_NUM; i++) {
        if (!mu_stats->mimo[i].aid) {
            continue;
        }
        tot_rx_mimo = mu_stats->mimo[i].occu_ss[0] + mu_stats->mimo[i].occu_ss[1] + mu_stats->mimo[i].occu_ss[2] + mu_stats->mimo[i].occu_ss[3];
        printf("[%d]%said:0x%x, "
               "mcs0:%d(%2.2f%%), mcs1:%d(%2.2f%%), mcs2:%d(%2.2f%%), mcs3:%d(%2.2f%%), mcs4:%d(%2.2f%%), "
               "mcs5:%d(%2.2f%%), mcs6:%d(%2.2f%%), mcs7:%d(%2.2f%%), mcs8:%d(%2.2f%%), mcs9:%d(%2.2f%%), "
               "mcs10:%d(%2.2f%%), mcs11:%d(%2.2f%%)\n",
               i, (mu_stats->aid == mu_stats->mimo[i].aid) ? "#" : " ", mu_stats->mimo[i].aid,
               mu_stats->mimo[i].occu_mcs[0], ((float) mu_stats->mimo[i].occu_mcs[0] / (float) tot_rx_mimo) * 100,
               mu_stats->mimo[i].occu_mcs[1], ((float) mu_stats->mimo[i].occu_mcs[1] / (float) tot_rx_mimo) * 100,
               mu_stats->mimo[i].occu_mcs[2], ((float) mu_stats->mimo[i].occu_mcs[2] / (float) tot_rx_mimo) * 100,
               mu_stats->mimo[i].occu_mcs[3], ((float) mu_stats->mimo[i].occu_mcs[3] / (float) tot_rx_mimo) * 100,
               mu_stats->mimo[i].occu_mcs[4], ((float) mu_stats->mimo[i].occu_mcs[4] / (float) tot_rx_mimo) * 100,
               mu_stats->mimo[i].occu_mcs[5], ((float) mu_stats->mimo[i].occu_mcs[5] / (float) tot_rx_mimo) * 100,
               mu_stats->mimo[i].occu_mcs[6], ((float) mu_stats->mimo[i].occu_mcs[6] / (float) tot_rx_mimo) * 100,
               mu_stats->mimo[i].occu_mcs[7], ((float) mu_stats->mimo[i].occu_mcs[7] / (float) tot_rx_mimo) * 100,
               mu_stats->mimo[i].occu_mcs[8], ((float) mu_stats->mimo[i].occu_mcs[8] / (float) tot_rx_mimo) * 100,
               mu_stats->mimo[i].occu_mcs[9], ((float) mu_stats->mimo[i].occu_mcs[9] / (float) tot_rx_mimo) * 100,
               mu_stats->mimo[i].occu_mcs[10], ((float) mu_stats->mimo[i].occu_mcs[10] / (float) tot_rx_mimo) * 100,
               mu_stats->mimo[i].occu_mcs[11], ((float) mu_stats->mimo[i].occu_mcs[11] / (float) tot_rx_mimo) * 100);
        printf("[%d]%said:0x%x, "
               "ss0:%d(%2.2f%%), ss1:%d(%2.2f%%), ss2:%d(%2.2f%%), ss3:%d(%2.2f%%)\n",
               i, (mu_stats->aid == mu_stats->mimo[i].aid) ? "#" : " ", mu_stats->mimo[i].aid,
               mu_stats->mimo[i].occu_ss[0], ((float) mu_stats->mimo[i].occu_ss[0] / (float) tot_rx_mimo) * 100,
               mu_stats->mimo[i].occu_ss[1], ((float) mu_stats->mimo[i].occu_ss[1] / (float) tot_rx_mimo) * 100,
               mu_stats->mimo[i].occu_ss[2], ((float) mu_stats->mimo[i].occu_ss[2] / (float) tot_rx_mimo) * 100,
               mu_stats->mimo[i].occu_ss[3], ((float) mu_stats->mimo[i].occu_ss[3] / (float) tot_rx_mimo) * 100);
        printf("[%d]%said:0x%x, "
               "tot_rx_mimo:%8d, sta/dut:%2.2f%%\n",
               i, (mu_stats->aid == mu_stats->mimo[i].aid) ? "#" : " ", mu_stats->mimo[i].aid,
               tot_rx_mimo, ((float) tot_rx_mimo / (float) mu_stats->mimo_rx) * 100);
    }
}

void print_hw_rx_statistics(void)
{
    esp_test_hw_rx_statistics_t hw_rx_stats = { 0, };
    esp_test_get_hw_rx_statistics(&hw_rx_stats);
    printf(
        "WDEVRX_FCS_ERR          :%d\n"
        "WDEVRX_ABORT            :%d\n"
        "WDEVRX_ABORT_FCS_PASS   :%d\n"
        "WDEVRX_LASTUNMATCH_ERR  :%d\n"
        "WDEVRX_BUF_FULLCNT      :%d\n"
        "WDEVRX_SAMEBM_ERRCNT    :%d\n"
        "NRX_ERR_PWRDROP         :%d\n"
        "NRX_HESIGB_ERR          :%d\n"
        "WDEVRX_MPDU             :%d\n"
        "WDEVRX_END_CNT          :%d\n"
        "WDEVRX_DATASUC          :%d\n"
        "RXHUNG_STATIS           :%d\n"
        "TXHUNG_STATIS           :%d\n"
        "RXTXHUNG                :%d\n"
#if CONFIG_SOC_WIFI_MAC_VERSION_NUM >= 3
        "RXTXPANIC               :%d\n"
        "BF_NDP_TIMEOUT          :%d\n"
        "BF_REPORT_ERR           :%d\n"
        "NRX_VHT_UNSUPPORT        :%d\n"
        "NRX_VHTSIGA_CRC          :%d\n"
#endif
        "WDEVRX_CFO              :%d\n"
        "WDEVRX_SF               :%d\n"
        "WDEVRX_OTHER_UCAST      :%d\n"
        "WDEVRX_FIFO_OVFCNT      :%d\n"
        "WDEVRX_TKIP_ERRCNT      :%d\n"
        "WDEVRX_BTBLOCK_ERR      :%d\n"
        "WDEVRX_FREQHOP_ERR      :%d\n"
        "WDEVRX_ACK_INT_CNT      :%d\n"
        "WDEVRX_RTS_INT_CNT      :%d\n"
        "BRX_ERR                 :%d\n"
        "BRX_ERR_AGC             :%d\n"
        "NRX_ERR                 :%d\n"
        "NRX_ERR_ABORT           :%d\n"
        "NRX_ERR_AGCEXIT         :%d\n"
        "NRX_ERR_BBOFF           :%d\n"
        "NRX_ERR_FDM_WDG         :%d\n"
        "NRX_ERR_RESTART         :%d\n"
        "NRX_ERR_SERV            :%d\n"
        "NRX_ERR_TXOVER          :%d\n"
        "NRX_ERR_UNSUPPORT       :%d\n"
        "NRX_HTSIG_ERR           :%d\n"
        "NRX_HE_UNSUPPORT        :%d\n"
        "NRX_HESIGA_CRC          :%d\n",
        hw_rx_stats.rx_fcs_err,
        hw_rx_stats.rx_abort,
        hw_rx_stats.rx_abort_fcs_pass,
        hw_rx_stats.rx_lastunmatch_err,
        hw_rx_stats.rx_buf_fullcnt,
        hw_rx_stats.rx_samebm_errcnt,
        hw_rx_stats.nrx_err_pwrdrop,
        hw_rx_stats.nrx_hesigb_err,
        hw_rx_stats.rx_mpdu,
        hw_rx_stats.rx_end_cnt,
        hw_rx_stats.rx_datasuc,
        hw_rx_stats.rxhung_statis,
        hw_rx_stats.txhung_statis,
        hw_rx_stats.rxtxhung,
#if CONFIG_SOC_WIFI_MAC_VERSION_NUM >= 3
        hw_rx_stats.rxtxpanic,
        hw_rx_stats.bf_ndp_timeout,
        hw_rx_stats.bf_report_err,
        hw_rx_stats.nrx_vhtunsupport,
        hw_rx_stats.nrx_vhtsiga_crc,
#endif
        hw_rx_stats.rx_cfo_hz,
        hw_rx_stats.rx_sf,
        hw_rx_stats.rx_other_ucast,
        hw_rx_stats.rx_fifo_ovfcnt,
        hw_rx_stats.rx_tkip_errcnt,
        hw_rx_stats.rx_btblock_err,
        hw_rx_stats.rx_freqhop_err,
        hw_rx_stats.rx_ack_int_cnt,
        hw_rx_stats.rx_rts_int_cnt,
        hw_rx_stats.brx_err,
        hw_rx_stats.brx_err_agc,
        hw_rx_stats.nrx_err,
        hw_rx_stats.nrx_err_abort,
        hw_rx_stats.nrx_err_agcexit,
        hw_rx_stats.nrx_err_bboff,
        hw_rx_stats.nrx_err_fdm_wdg,
        hw_rx_stats.nrx_err_restart,
        hw_rx_stats.nrx_err_serv,
        hw_rx_stats.nrx_err_txover,
        hw_rx_stats.nrx_err_unsupport,
        hw_rx_stats.nrx_htsig_err,
        hw_rx_stats.nrx_heunsupport,
        hw_rx_stats.nrx_hesiga_crc
    );
}

int wifi_cmd_clr_rx_statistics(int argc, char **argv)
{
    ESP_LOGW(APP_TAG, "Clear rx statistics");
    esp_wifi_clr_rx_statistics(0);
    esp_wifi_clr_rx_statistics(7);
#if CONFIG_ESP_WIFI_ENABLE_WIFI_RX_MU_STATS
    esp_test_clr_rx_error_occurs();
    esp_wifi_clr_rx_mu_statistics();
#endif
    esp_test_clr_hw_statistics();
#if CONFIG_ESP_WIFI_ENABLE_DUMP_MU_CFO
    ESP_LOGI(APP_TAG, "Clear CFO, count:%d, g_mu_cfo_idx:%d", MU_CFO_COUNT, g_mu_cfo_idx);
    memset(g_mu_cfo, 0, MU_CFO_COUNT * sizeof(int16_t));
    g_mu_cfo_idx = 0;
    ESP_LOGI(APP_TAG, "Clear CFO done, count:%d, g_mu_cfo_idx:%d, g_mu_cfo[0]:%d, g_mu_cfo[%d]:%d", MU_CFO_COUNT, g_mu_cfo_idx, g_mu_cfo[0], MU_CFO_COUNT - 1, g_mu_cfo[MU_CFO_COUNT - 1]);
#endif

#if CONFIG_ESP_WIFI_ENABLE_DUMP_CTRL_NDPA
    esp_test_clr_rx_ctrls();
#endif

#if CONFIG_ESP_WIFI_ENABLE_DUMP_CTRL_BFRP
    ESP_LOGI(APP_TAG, "Clear BFRP, count:%d, g_mu_cfo_idx:%d", RX_TRIG_COUNT, g_rx_trig_idx);
    memset(test_rx_trig_bfrp, 0, RX_TRIG_COUNT * sizeof(test_rx_trig_bfrp_t));
    ESP_LOGI(APP_TAG, "Clear BFRP done, count:%d, g_rx_trig_idx:%d, test_rx_trig_bfrp[0].rssi:%d, test_rx_trig_bfrp[%d].rssi:%d",
             RX_TRIG_COUNT, g_rx_trig_idx, test_rx_trig_bfrp[0].rssi, RX_TRIG_COUNT - 1, test_rx_trig_bfrp[RX_TRIG_COUNT - 1].rssi);
    g_rx_trig_idx = 0;
#endif
    return 0;
}

#if CONFIG_ESP_WIFI_ENABLE_WIFI_RX_MU_STATS
void print_rx_mu_statistics(void)
{
    esp_test_rx_mu_statistics_t *rx_mu_stats = (esp_test_rx_mu_statistics_t *)malloc(sizeof(esp_test_rx_mu_statistics_t));
    if (rx_mu_stats == NULL) {
        ESP_LOGE(APP_TAG, "Failed to malloc rx_mu_stats");
        return;
    }
    /* mu */
    esp_wifi_get_rx_mu_statistics(rx_mu_stats);
    /* MIMO */
    print_rx_statistics_mimo(rx_mu_stats);
    /* non-MIMO */
    print_rx_statistics_nonmimo(rx_mu_stats);
    free(rx_mu_stats);
}
#endif

#if WIFI_CMD_5G_SUPPORTED
void print_vht_rx_mu_statistics(esp_test_rx_statistics_t *rx_stats)
{
    int i, j, k;
    uint32_t vht_mu_tot = 0;
    uint32_t vht_mu_tot_retry = 0;
    uint32_t vht_mu_tot_ppdu = 0;

    for (i = 1; i <= 62; i ++) {
        for (j = 0; j < 4; j ++) {
            if (rx_stats->vht_mu[i][j]) {
                vht_mu_tot_ppdu = 0;
                for (k = 0; k <= 7; k ++) {
                    vht_mu_tot_ppdu += rx_stats->vht_mu_mcs[i][j][k];
                }
                ESP_LOGI(APP_TAG, "(0)vht(vht_mu[%d][%d]:%" PRIu32 ", vht_mu_retry[%d][%d]:%" PRIu32 "/%2.2f%%, vht_mu_tot_ppdu:%" PRIu32 ", "
                         "vht_mu_mcs:[%d]%2.2f%%, [%d]%2.2f%%, [%d]%2.2f%%, [%d]%2.2f%%, "
                         "[%d]%2.2f%%, [%d]%2.2f%%, [%d]%2.2f%%, [%d]%2.2f%%)",
                         i, j, rx_stats->vht_mu[i][j],
                         i, j, rx_stats->vht_mu_retry[i][j], rx_stats->vht_mu[i][j] ? (((float) rx_stats->vht_mu_retry[i][j] / (float) rx_stats->vht_mu[i][j]) * 100) : 0,
                         vht_mu_tot_ppdu,
                         7, vht_mu_tot_ppdu ? (((float) rx_stats->vht_mu_mcs[i][j][7] / (float) vht_mu_tot_ppdu) * 100) : 0,
                         6, vht_mu_tot_ppdu ? (((float) rx_stats->vht_mu_mcs[i][j][6] / (float) vht_mu_tot_ppdu) * 100) : 0,
                         5, vht_mu_tot_ppdu ? (((float) rx_stats->vht_mu_mcs[i][j][5] / (float) vht_mu_tot_ppdu) * 100) : 0,
                         4, vht_mu_tot_ppdu ? (((float) rx_stats->vht_mu_mcs[i][j][4] / (float) vht_mu_tot_ppdu) * 100) : 0,
                         3, vht_mu_tot_ppdu ? (((float) rx_stats->vht_mu_mcs[i][j][3] / (float) vht_mu_tot_ppdu) * 100) : 0,
                         2, vht_mu_tot_ppdu ? (((float) rx_stats->vht_mu_mcs[i][j][2] / (float) vht_mu_tot_ppdu) * 100) : 0,
                         1, vht_mu_tot_ppdu ? (((float) rx_stats->vht_mu_mcs[i][j][1] / (float) vht_mu_tot_ppdu) * 100) : 0,
                         0, vht_mu_tot_ppdu ? (((float) rx_stats->vht_mu_mcs[i][j][0] / (float) vht_mu_tot_ppdu) * 100) : 0);
                vht_mu_tot += rx_stats->vht_mu[i][j];
                vht_mu_tot_retry += rx_stats->vht_mu_retry[i][j];
            }
        }
    }
    ESP_LOGI(APP_TAG, "(0)vht(vht_mu:%" PRIu32 ", vht_mu_retry:%" PRIu32 "/%2.2f%%, vht_mu_noeb:%" PRIu32 "/%2.2f%%)",
             vht_mu_tot,
             vht_mu_tot_retry, vht_mu_tot ? (((float) vht_mu_tot_retry / (float) vht_mu_tot) * 100) : 0,
             rx_stats->vht_mu_noeb, vht_mu_tot ? (((float) rx_stats->vht_mu_noeb / (float) vht_mu_tot) * 100) : 0);

}
#endif

int wifi_cmd_get_rx_statistics(int argc, char **argv)
{
    ESP_LOGW(APP_TAG, "Get rx statistics");

#if WIFI_CMD_5G_SUPPORTED
    wifi_band_t band = WIFI_BAND_2G;
    esp_wifi_get_band(&band);
#endif
    esp_test_rx_statistics_t *rx_stats = (esp_test_rx_statistics_t *)malloc(sizeof(esp_test_rx_statistics_t));
    if (rx_stats == NULL) {
        ESP_LOGE(APP_TAG, "Failed to malloc rx_stats");
        return ESP_FAIL;
    }
    esp_test_rx_error_occurs_t rx_error_occurs = { 0, };

    esp_wifi_get_rx_statistics(0, rx_stats); //tid=0
    print_hw_tb_statistics();
#if CONFIG_SOC_WIFI_MAC_VERSION_NUM >= 3
    ESP_LOGW(APP_TAG, "(0)legacy:%d, ht(ht:%d, ht_stbc:%d, ht_retry:%d/%2.2f%%, ht_noeb:%d/%2.2f%%)",
             rx_stats->legacy,
             rx_stats->ht,
             rx_stats->ht_stbc,
             rx_stats->ht_retry,
             rx_stats->ht_retry ? ((float)((float) rx_stats->ht_retry / (float) rx_stats->ht) * 100) : 0,
             rx_stats->ht_noeb, rx_stats->ht_noeb ? ((float)((float) rx_stats->ht_noeb / (float) rx_stats->ht) * 100) : 0);
    ESP_LOGW(APP_TAG, "(0)su(su:%d, su_txbf:%d, su_stbc:%d, su_retry:%d/%2.2f%%, su_frag:%d, su_noeb:%d/%2.2f%%)",
             rx_stats->su,
             rx_stats->su_txbf, rx_stats->su_stbc,
             rx_stats->su_retry,
             rx_stats->su_retry ? ((float)((float) rx_stats->su_retry / (float) rx_stats->su) * 100) : 0,
             rx_stats->su_frag,
             rx_stats->su_noeb, rx_stats->su_noeb ? ((float)((float) rx_stats->su_noeb / (float) rx_stats->su) * 100) : 0);
    ESP_LOGW(APP_TAG, "(0)mu(mu:%d, mimo:%d, non-mimo:%d, txbf:%d, stbc:%d, mu_retry:%d/%2.2f%%, mu_frag:%d, bw20:%d, bw40:%d, bw80:%d, mu_noeb:%d/%2.2f%%), sigb_dump:%d",
             rx_stats->mu,
             rx_stats->mu_mimo,
             rx_stats->mu_ofdma, rx_stats->mu_txbf, rx_stats->mu_stbc,
             rx_stats->mu_retry,
             rx_stats->mu_retry ? ((float)((float) rx_stats->mu_retry / (float) rx_stats->mu) * 100) : 0,
             rx_stats->mu_frag,
             rx_stats->mu_bw[0], rx_stats->mu_bw[1], rx_stats->mu_bw[2],
             rx_stats->mu_noeb, rx_stats->mu_noeb ? ((float)((float) rx_stats->mu_noeb / (float) rx_stats->mu) * 100) : 0,  rx_stats->mu_sigb_dump);
#else
    ESP_LOGW(APP_TAG, "(0)legacy:%d, ht(ht:%d, ht_retry:%d/%2.2f%%, ht_noeb:%d/%2.2f%%)",
             rx_stats->legacy,
             rx_stats->ht, rx_stats->ht_retry,
             rx_stats->ht_retry ? ((float)((float) rx_stats->ht_retry / (float) rx_stats->ht) * 100) : 0,
             rx_stats->ht_noeb, rx_stats->ht_noeb ? ((float)((float) rx_stats->ht_noeb / (float) rx_stats->ht) * 100) : 0);
    ESP_LOGW(APP_TAG, "(0)su(su:%d, su_txbf:%d, su_stbc:%d, su_retry:%d/%2.2f%%, ersu:%d, ersu_dcm:%d, su_noeb:%d/%2.2f%%)",
             rx_stats->su,
             rx_stats->su_txbf, rx_stats->su_stbc,
             rx_stats->su_retry,
             rx_stats->su_retry ? ((float)((float) rx_stats->su_retry / (float) rx_stats->su) * 100) : 0,
             rx_stats->ersu,
             rx_stats->ersu_dcm,
             rx_stats->su_noeb, rx_stats->su_noeb ? ((float)((float) rx_stats->su_noeb / (float) rx_stats->su) * 100) : 0);
    ESP_LOGW(APP_TAG, "(0)mu(mu:%d, mimo:%d, non-mimo:%d, txbf:%d, stbc:%d, mu_retry:%d/%2.2f%%, mu_noeb:%d/%2.2f%%)",
             rx_stats->mu,
             rx_stats->mu_mimo,
             rx_stats->mu_ofdma, rx_stats->mu_txbf, rx_stats->mu_stbc,
             rx_stats->mu_retry,
             rx_stats->mu_retry ? ((float)((float) rx_stats->mu_retry / (float) rx_stats->mu) * 100) : 0,
             rx_stats->mu_noeb, rx_stats->mu_noeb ? ((float)((float) rx_stats->mu_noeb / (float) rx_stats->mu) * 100) : 0);
#endif

#if CONFIG_SOC_WIFI_MAC_VERSION_NUM >= 3
    ESP_LOGW(APP_TAG, "(0)ersu(ersu:%d, ersu_txbf:%d, ersu_dcm:%d, ersu_dcm_txbf:%d, ersu_retry:%d/%2.2f%%, ersu_noeb:%d/%2.2f%%)",
             rx_stats->ersu,
             rx_stats->ersu_txbf,
             rx_stats->ersu_dcm,
             rx_stats->ersu_dcm_txbf,
             rx_stats->ersu_retry,
             rx_stats->ersu_retry ? ((float)((float) rx_stats->ersu_retry / (float) rx_stats->ersu) * 100) : 0,
             rx_stats->ersu_noeb, rx_stats->ersu_noeb ? ((float)((float) rx_stats->ersu_noeb / (float) rx_stats->ersu) * 100) : 0);
#if WIFI_CMD_5G_SUPPORTED
    if (band == WIFI_BAND_5G) {
        ESP_LOGW(APP_TAG, "(0)vht(vht:%d, vht_txbf:%d, vht_stbc:%d, vht_retry:%d/%2.2f%%, vht_noeb:%d/%2.2f%%)",
                 rx_stats->vht,
                 rx_stats->vht_txbf, rx_stats->vht_stbc,
                 rx_stats->vht_retry,
                 rx_stats->vht_retry ? ((float)((float) rx_stats->vht_retry / (float) rx_stats->vht) * 100) : 0,
                 rx_stats->vht_noeb, rx_stats->vht_noeb ? ((float)((float) rx_stats->vht_noeb / (float) rx_stats->vht) * 100) : 0);
    }
#endif
#endif

#if WIFI_CMD_5G_SUPPORTED
    if (band == WIFI_BAND_5G) {
        print_vht_rx_mu_statistics(rx_stats);
    }
#endif
    memset(rx_stats, 0, sizeof(esp_test_rx_statistics_t));
    esp_wifi_get_rx_statistics(7, rx_stats); //tid=7
    ESP_LOGW(APP_TAG, "(7)legacy:%d, ht:%d, su:%d, su_txbf:%d, ersu:%d, mu:%d", rx_stats->legacy,
             rx_stats->ht, rx_stats->su, rx_stats->su_txbf, rx_stats->ersu, rx_stats->mu);
    ESP_LOGW(APP_TAG, "(hw)isr:%d, nblks:%d", rx_stats->rx_isr, rx_stats->rx_nblks);

#if CONFIG_SOC_WIFI_MAC_VERSION_NUM >= 3
    ESP_LOGW(APP_TAG, "(hw)ndpa:%d rx_reload_base_cnt:%d rx_base_null_cnt:%d", rx_stats->rx_ndpa, rx_stats->rx_reset_rxbase_cnt, rx_stats->rx_base_null_cnt);
#endif
#if CONFIG_ESP_WIFI_ENABLE_WIFI_RX_MU_STATS
    print_rx_mu_statistics();
#endif
    esp_test_get_rx_error_occurs(&rx_error_occurs);
    ESP_LOGW(APP_TAG, "(rx)tot_errors:%d", rx_error_occurs.tot);
    int known_errors = 0; //rx error: 0x40-0xff
    int i;
    for (i = 0; i < 2; i++) {
        if (rx_error_occurs.occurs[i]) {
            known_errors += rx_error_occurs.occurs[i];
            printf("[%3d]  0x%x, %8d, %2.2f%%\n", i, (i ? 0xf5 : 0xc6), rx_error_occurs.occurs[i], ((float) rx_error_occurs.occurs[i] / (float) rx_error_occurs.tot) * 100);
        }
    }
    if (rx_error_occurs.tot - known_errors) {
        printf("[%3d]others, %8d, %2.2f%%\n\n", i, rx_error_occurs.tot - known_errors, ((float) known_errors / (float) rx_error_occurs.tot) * 100);
    }
#if CONFIG_ESP_WIFI_ENABLE_DUMP_CTRL_NDPA
    esp_test_rx_ctrl_t rx_ctrl = { 0, };
    esp_test_get_rx_ctrls(&rx_ctrl);
#if CONFIG_SOC_WIFI_MAC_VERSION_NUM >= 3
    printf("(ctrl)ndpa(%d, ra_bcast:%d, ra_ucast:%d, su_feedback:%d, mu_feedback:%d, cqi:%d), \
            trig(basic(all:%d, for_self:%d, id_0:%d, id_2045:%d, id_2046:%d, id_other:%d), \
            bfrp:%d, bsrp:%d, mu-bar:%d, nfrp:%d, bfpoll:%d)\r\n",
           rx_ctrl.ndpa,
           rx_ctrl.ndpa_su_bcast,
           rx_ctrl.ndpa_su_ucast,
           rx_ctrl.ndpa_su,
           rx_ctrl.ndpa_mu,
           rx_ctrl.ndpa_cqi,
           rx_ctrl.trigger_basic_cnt,
           rx_ctrl.basic,
           rx_ctrl.trigger_basic_id_0_cnt,
           rx_ctrl.trigger_basic_id_2045_cnt,
           rx_ctrl.trigger_basic_id_2046_cnt,
           rx_ctrl.trigger_basic_id_other_cnt,
           rx_ctrl.bfrp,
           rx_ctrl.bsrp,
           rx_ctrl.mubar,
           rx_ctrl.nfrp,
           rx_ctrl.bfpoll);
#else
    printf("(ctrl)ndpa(%d, ra_bcast:%d, ra_ucast:%d, su_feedback:%d, mu_feedback:%d, cqi:%d), trig(basic:%d, bfrp:%d, bsrp:%d, mu-bar:%d, nfrp:%d)\r\n",
           rx_ctrl.ndpa,
           rx_ctrl.ndpa_su_bcast,
           rx_ctrl.ndpa_su_ucast,
           rx_ctrl.ndpa_su,
           rx_ctrl.ndpa_mu,
           rx_ctrl.ndpa_cqi,
           rx_ctrl.basic,
           rx_ctrl.bfrp,
           rx_ctrl.bsrp,
           rx_ctrl.mubar,
           rx_ctrl.nfrp);
#endif
#endif

#if CONFIG_ESP_WIFI_ENABLE_DUMP_CTRL_BFRP
    //dump bfrp
    int k;
    for (k = 0; k < RX_TRIG_COUNT; k ++) {
        printf("[%3d]rssi:%2d, rate:0x%x, ap_tx_pwr:%d, target_rssi:%d\r\n",
               k, test_rx_trig_bfrp[k].rssi, test_rx_trig_bfrp[k].rate, test_rx_trig_bfrp[k].ap_tx_pwr, test_rx_trig_bfrp[k].target_rssi);
    }
    printf("\r\n");
#endif

    /* hw rx statistics */
    print_hw_rx_statistics();

    wifi_cmd_clr_rx_statistics(0, 0);
    free(rx_stats);
    return 0;
}

#if CONFIG_ESP_WIFI_ENABLE_DUMP_CTRL_NDPA || CONFIG_ESP_WIFI_ENABLE_DUMP_CTRL_BFRP
extern void hal_set_dump_ctrl_frame_cfg(uint8_t ifx, uint32_t subtype_bitmap);
extern uint32_t hal_get_dump_ctrl_frame_cfg(uint8_t ifx);
static void wifi_cmd_dump_ctrl_frames(bool enable)
{
    uint32_t subtype_bitmap = 0;
    subtype_bitmap = hal_get_dump_ctrl_frame_cfg(WIFI_IF_STA);
#if CONFIG_ESP_WIFI_ENABLE_DUMP_CTRL_NDPA
    /* NDPA: 0x5 */
    uint8_t ndpa_subtype = 0x5;
#endif
#if CONFIG_ESP_WIFI_ENABLE_DUMP_CTRL_BFRP
    /* BFRP: 0x2*/
    uint8_t bfrp_subtype = 0x2;
#endif

#if CONFIG_ESP_WIFI_ENABLE_DUMP_CTRL_NDPA
    if (enable) {
        subtype_bitmap |= BIT(ndpa_subtype);
    } else {
        subtype_bitmap &= ~BIT(ndpa_subtype);
    }
    hal_set_dump_ctrl_frame_cfg(WIFI_IF_STA, subtype_bitmap);
    ESP_LOGI(APP_TAG, "(ndpa)interface:%u, subtype_bitmap:0x%x", WIFI_IF_STA, subtype_bitmap);
#endif

#if CONFIG_ESP_WIFI_ENABLE_DUMP_CTRL_BFRP
    if (enable) {
        subtype_bitmap |= BIT(bfrp_subtype);
    } else {
        subtype_bitmap &= ~BIT(bfrp_subtype);
    }
    hal_set_dump_ctrl_frame_cfg(WIFI_IF_STA, subtype_bitmap);
    ESP_LOGI(APP_TAG, "(bfrp)interface:%u, subtype_bitmap:0x%x", WIFI_IF_STA, subtype_bitmap);
#endif
}
#endif /* CONFIG_ESP_WIFI_ENABLE_DUMP_CTRL_NDPA || CONFIG_ESP_WIFI_ENABLE_DUMP_CTRL_BFRP */

static int cmd_wifi_stats(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&wifi_stats_args);

    if (nerrors != 0) {
        arg_print_errors(stderr, wifi_stats_args.end, argv[0]);
        return 1;
    }

    const char *direction = wifi_stats_args.direction->sval[0];
    const char *aci_str = wifi_stats_args.aci->sval[0];
    const char *action = NULL;
    uint8_t aci = ESP_WIFI_ACI_BE;
    esp_err_t err = ESP_OK;

    if (wifi_stats_args.action->count > 0) {
        action = wifi_stats_args.action->sval[0];
    } else {
        action = "get";
    }

    if (wifi_stats_args.aci->count > 0) {
        aci = app_wifi_aci_str2num(aci_str);
    }
#if CONFIG_ESP_WIFI_ENABLE_WIFI_TX_STATS
    if (strcmp(direction, "tx") == 0) {
        if (strcmp(action, "enable") == 0) {
            err = esp_wifi_enable_tx_statistics(aci, true);
            LOG_WIFI_CMD_DONE(err, "ENABLE_WIFI_TX_STATS");
        } else if (strcmp(action, "disable") == 0) {
            err = esp_wifi_enable_tx_statistics(aci, false);
            LOG_WIFI_CMD_DONE(err, "DISABLE_WIFI_TX_STATS");
        } else if (strcmp(action, "clear") == 0) {
            err = wifi_cmd_clr_tx_statistics(0, NULL);
            LOG_WIFI_CMD_DONE(err, "CLEAR_WIFI_TX_STATS");
        } else if (strcmp(action, "get") == 0) {
            err = wifi_cmd_get_tx_statistics(0, NULL);
            LOG_WIFI_CMD_DONE(err, "GET_WIFI_TX_STATS");
        } else {
            ESP_LOGE(APP_TAG, "Invalid wifi_stats action: %s", action);
            return 1;
        }
    }
#endif  /* CONFIG_ESP_WIFI_ENABLE_WIFI_TX_STATS */

#if CONFIG_ESP_WIFI_ENABLE_WIFI_RX_STATS
    if (strcmp(direction, "rx") == 0) {
        if (strcmp(action, "enable") == 0) {
#if CONFIG_ESP_WIFI_ENABLE_WIFI_RX_MU_STATS
            err = esp_wifi_enable_rx_statistics(true, true);
#else
            err = esp_wifi_enable_rx_statistics(true, false);
#endif
#if CONFIG_ESP_WIFI_ENABLE_DUMP_CTRL_NDPA || CONFIG_ESP_WIFI_ENABLE_DUMP_CTRL_BFRP
            wifi_cmd_dump_ctrl_frames(true);
#endif
            LOG_WIFI_CMD_DONE(err, "ENABLE_WIFI_RX_STATS");
        } else if (strcmp(action, "disable") == 0) {
            err = esp_wifi_enable_rx_statistics(false, false);
#if CONFIG_ESP_WIFI_ENABLE_DUMP_CTRL_NDPA || CONFIG_ESP_WIFI_ENABLE_DUMP_CTRL_BFRP
            wifi_cmd_dump_ctrl_frames(false);
#endif
            LOG_WIFI_CMD_DONE(err, "DISABLE_WIFI_RX_STATS");
        } else if (strcmp(action, "clear") == 0) {
            err = wifi_cmd_clr_rx_statistics(0, NULL);
            LOG_WIFI_CMD_DONE(err, "CLEAR_WIFI_RX_STATS");
        } else if (strcmp(action, "get") == 0) {
            err = wifi_cmd_get_rx_statistics(0, NULL);
            LOG_WIFI_CMD_DONE(err, "GET_WIFI_RX_STATS");
        } else {
            ESP_LOGE(APP_TAG, "Invalid wifi_stats action: %s", action);
            return 1;
        }
    }
#endif  /* CONFIG_ESP_WIFI_ENABLE_WIFI_RX_STATS */

    if (strcmp(direction, "tx") != 0 && strcmp(direction, "rx") != 0) {
        ESP_LOGE(APP_TAG, "Invalid direction: %s", direction);
        return 1;
    }

    return 0;
}

#if CONFIG_WIFI_CMD_ENABLE_DEPRECATED
#if CONFIG_ESP_WIFI_ENABLE_WIFI_TX_STATS
static int wifi_cmd_get_tx_statistics_deprecated(int argc, char **argv)
{
    ESP_LOGW(APP_TAG, "'txstats' is deprecated, please use 'wifi_stats tx get'.");
    return wifi_cmd_get_tx_statistics(argc, argv);
}

static int wifi_cmd_clr_tx_statistics_deprecated(int argc, char **argv)
{
    ESP_LOGW(APP_TAG, "'clrtxstats' is deprecated, please use 'wifi_stats tx clear'.");
    return wifi_cmd_clr_tx_statistics(argc, argv);
}
#endif

#if CONFIG_ESP_WIFI_ENABLE_WIFI_RX_STATS
static int wifi_cmd_get_rx_statistics_deprecated(int argc, char **argv)
{
    ESP_LOGW(APP_TAG, "'rxstats' is deprecated, please use 'wifi_stats rx get'.");
    return wifi_cmd_get_rx_statistics(argc, argv);

}

static int wifi_cmd_clr_rx_statistics_deprecated(int argc, char **argv)
{
    ESP_LOGW(APP_TAG, "'clrrxstats' is deprecated, please use 'wifi_stats rx clear'.");
    return wifi_cmd_clr_rx_statistics(argc, argv);
}
#endif
#endif /* CONFIG_WIFI_CMD_ENABLE_DEPRECATED */

void wifi_cmd_register_stats(void)
{
    if (!wifi_stats_args.end) {
        wifi_stats_args.direction = arg_str1(NULL, NULL, "<tx | rx>", "Direction: tx or rx");
        wifi_stats_args.action = arg_str0(NULL, NULL, "<enable | disable | clear | get>", "Action: enable, disable, clear, get(default)");
        wifi_stats_args.aci = arg_str0("a", "aci", "<aci>", "ACI option, only valid for tx, support: [vo, vi, be, bk]");
        wifi_stats_args.end = arg_end(2);
    }
    const esp_console_cmd_t wifi_stats_cmd = {
        .command = "wifi_stats",
        .help = "WiFi tx/rx packets stats command(only support on WiFi6 SDK)",
        .hint = NULL,
        .func = &cmd_wifi_stats,
        .argtable = &wifi_stats_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&wifi_stats_cmd));

#if CONFIG_WIFI_CMD_ENABLE_DEPRECATED
#if CONFIG_ESP_WIFI_ENABLE_WIFI_TX_STATS
    /* get tx statistics */
    const esp_console_cmd_t tx_stats_cmd_deprecated = {
        .command = "txstats",
        .help = "get tx statistics",
        .hint = NULL,
        .func = &wifi_cmd_get_tx_statistics_deprecated,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&tx_stats_cmd_deprecated));
    /* clear tx statistics */
    const esp_console_cmd_t clr_tx_cmd_deprecated = {
        .command = "clrtxstats",
        .help = "clear tx statistics",
        .hint = NULL,
        .func = &wifi_cmd_clr_tx_statistics_deprecated,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&clr_tx_cmd_deprecated));
#endif

#if CONFIG_ESP_WIFI_ENABLE_WIFI_RX_STATS
    /* get rx statistics */
    const esp_console_cmd_t rx_stats_cmd_deprecated = {
        .command = "rxstats",
        .help = "get rx statistics",
        .hint = NULL,
        .func = &wifi_cmd_get_rx_statistics_deprecated,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&rx_stats_cmd_deprecated));
    /* clear rx statistics */
    const esp_console_cmd_t clr_rx_cmd_deprecated = {
        .command = "clrrxstats",
        .help = "clear rx statistics",
        .hint = NULL,
        .func = &wifi_cmd_clr_rx_statistics_deprecated,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&clr_rx_cmd_deprecated));
#endif
#endif /* CONFIG_WIFI_CMD_ENABLE_DEPRECATED */
}

#endif /* CONFIG_ESP_WIFI_ENABLE_WIFI_TX_STATS || CONFIG_ESP_WIFI_ENABLE_WIFI_RX_STATS */
#endif /* CONFIG_WIFI_CMD_ENABLE_WIFI_STATS */
