/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_console.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_eth.h"
#include "esp_system.h"

#include "ping_cmd.h"

#include "esp_idf_version.h"
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
typedef struct esp_eth_netif_glue_t* esp_eth_netif_glue_handle_t;
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#include "esp_mac.h"
#endif

static const char *TAG = "test_app";
static esp_netif_t *s_eth_netif = NULL;
static esp_eth_handle_t s_eth_handle = NULL;
static esp_eth_netif_glue_handle_t s_eth_glue = NULL;
static esp_eth_mac_t *s_mac = NULL;
static esp_eth_phy_t *s_phy = NULL;

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Up");
#if CONFIG_LWIP_IPV6
        esp_netif_create_ip6_linklocal(s_eth_netif);
#endif
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;
    ESP_LOGI(TAG, "GOT_IP4:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "  MASK4:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "  GW4:" IPSTR, IP2STR(&ip_info->gw));
}

#if CONFIG_LWIP_IPV6
/* types of ipv6 addresses to be displayed on ipv6 events */
static const char *s_ipv6_addr_types[] = {
    "UNKNOWN",
    "GLOBAL",
    "LINK_LOCAL",
    "SITE_LOCAL",
    "UNIQUE_LOCAL",
    "IPV4_MAPPED_IPV6"
};
/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip6_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
    esp_ip6_addr_type_t ipv6_type = esp_netif_ip6_get_addr_type(&event->ip6_info.ip);
    ESP_LOGI(TAG, "Got IPv6 event: Interface \"%s\" address: " IPV6STR ", type: %s", esp_netif_get_desc(event->esp_netif),
             IPV62STR(event->ip6_info.ip), s_ipv6_addr_types[ipv6_type]);
}
#endif

static void extra_eth_config(esp_eth_handle_t eth_handle)
{
    uint8_t eth_mac[6];
    esp_read_mac(eth_mac, ESP_MAC_ETH);
    esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, eth_mac);
}

void ethernet_connect(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    esp_event_loop_create_default();

    // create TCP/IP netif
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_eth_netif = esp_netif_new(&netif_cfg);

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    s_mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
#else
    s_mac = esp_eth_mac_new_esp32(&mac_config);
#endif
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    s_phy = esp_eth_phy_new_ip101(&phy_config);
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(s_mac, s_phy);

    // install Ethernet driver
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &s_eth_handle));
    extra_eth_config(s_eth_handle);
    // combine driver with netif
    s_eth_glue = esp_eth_new_netif_glue(s_eth_handle);
    ESP_ERROR_CHECK(esp_netif_attach(s_eth_netif, s_eth_glue));
    // register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

#if CONFIG_LWIP_IPV6
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &got_ip6_event_handler, NULL));
#endif
    // start Ethernet driver
    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    repl_config.prompt = "iperf>";
    // init console REPL environment
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    /* connect ethernet */
    ethernet_connect();

    /* Register commands */
    ping_cmd_register_ping();

    // start console REPL
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
