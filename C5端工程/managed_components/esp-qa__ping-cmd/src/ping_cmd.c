/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_system.h"
#include "lwip/inet.h"
#include "lwip/netdb.h" /* struct addrinfo */
#include "lwip/sockets.h"
#include "ping/ping_sock.h"

/* there's no CONFIG_LWIP_IPV4 in idf<5.1 */
#ifndef CONFIG_LWIP_IPV4
#include "lwipopts.h"
#define CONFIG_LWIP_IPV4 LWIP_IPV4
#endif

typedef struct {
    struct arg_dbl *timeout;
    struct arg_dbl *interval;
    struct arg_int *data_size;
    struct arg_int *count;
    struct arg_int *tos;
    struct arg_str *host;
    struct arg_lit *abort;
    struct arg_end *end;
} wifi_ping_args_t;
static wifi_ping_args_t ping_args;

static void cmd_ping_on_ping_success(esp_ping_handle_t hdl, void *args)
{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    if (IP_IS_V4(&target_addr)) {
#if CONFIG_LWIP_IPV4
        printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%d ms\n",
               recv_len, inet_ntoa(*ip_2_ip4(&target_addr)), seqno, ttl, elapsed_time);
#endif
    } else {
#if CONFIG_LWIP_IPV6
        printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%d ms\n",
               recv_len, inet6_ntoa(*ip_2_ip6(&target_addr)), seqno, ttl, elapsed_time);
#endif
    }

}

static void cmd_ping_on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    if (IP_IS_V4(&target_addr)) {
#if CONFIG_LWIP_IPV4
        printf("From %s icmp_seq=%d timeout\n", inet_ntoa(*ip_2_ip4(&target_addr)), seqno);
#endif
    } else {
#if CONFIG_LWIP_IPV6
        printf("From %s icmp_seq=%d timeout\n", inet6_ntoa(*ip_2_ip6(&target_addr)), seqno);
#endif
    }
}

static void cmd_ping_on_ping_end(esp_ping_handle_t hdl, void *args)
{
    ip_addr_t target_addr;
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    uint32_t loss = (uint32_t)((1 - ((float)received) / transmitted) * 100);
    if (IP_IS_V4(&target_addr)) {
#if CONFIG_LWIP_IPV4
        printf("\n--- %s ping statistics ---\n", inet_ntoa(*ip_2_ip4(&target_addr)));
#endif
    } else {
#if CONFIG_LWIP_IPV6
        printf("\n--- %s ping statistics ---\n", inet6_ntoa(*ip_2_ip6(&target_addr)));
#endif
    }
    printf("%d packets transmitted, %d received, %d%% packet loss, time %dms\n",
           transmitted, received, loss, total_time_ms);
    // delete the ping sessions, so that we clean up all resources and can create a new ping session
    // we don't have to call delete function in the callback, instead we can call delete function from other tasks
    esp_ping_delete_session(hdl);
}

static esp_err_t parse_ip_address(const char* input_host, ip_addr_t *p_addr)
{
#if CONFIG_LWIP_IPV6
    struct sockaddr_in6 sock_addr6;
    if (inet_pton(AF_INET6, input_host, &sock_addr6.sin6_addr) == 1) {
        /* convert ip6 string to ip6 address */
        ipaddr_aton(input_host, p_addr);
        return ESP_OK;
    }
#endif

    struct addrinfo hint;
    struct addrinfo *res = NULL;
    memset(&hint, 0, sizeof(hint));

    /* convert ip4 string or hostname to ip4 or ip6 address */
    if (getaddrinfo(input_host, NULL, &hint, &res) != 0) {
        printf("ping: unknown host %s\n", input_host);
        return ESP_FAIL;
    }
    if (res->ai_family == AF_INET) {
#if CONFIG_LWIP_IPV4
        struct in_addr addr4 = ((struct sockaddr_in *)(res->ai_addr))->sin_addr;
        inet_addr_to_ip4addr(ip_2_ip4(p_addr), &addr4);
#else
        printf("Should not happen, got ipv4 address while ipv4 is not enabled!\n");
        freeaddrinfo(res);
        return ESP_FAIL;
#endif
    } else {
#if CONFIG_LWIP_IPV6
        struct in6_addr addr6 = ((struct sockaddr_in6 *)(res->ai_addr))->sin6_addr;
        inet6_addr_to_ip6addr(ip_2_ip6(p_addr), &addr6);
#else
        printf("Should not happen, got ipv6 address while ipv6 is not enabled!\n");
        freeaddrinfo(res);
        return ESP_FAIL;
#endif
    }
    /*set target type*/
#if CONFIG_LWIP_IPV6 && CONFIG_LWIP_IPV4
    p_addr->type = res->ai_family == AF_INET ? IPADDR_TYPE_V4 : IPADDR_TYPE_V6;
#endif

    freeaddrinfo(res);
    return ESP_OK;
}

static int do_ping_cmd(int argc, char **argv)
{
    static esp_ping_handle_t ping = NULL;
    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();

    int nerrors = arg_parse(argc, argv, (void **)&ping_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, ping_args.end, argv[0]);
        return 1;
    }

    if (ping_args.abort->count) {
        esp_ping_stop(ping);
        return 0;
    }
    if (ping_args.timeout->count > 0) {
        config.timeout_ms = (uint32_t)(ping_args.timeout->dval[0] * 1000);
    }

    if (ping_args.interval->count > 0) {
        config.interval_ms = (uint32_t)(ping_args.interval->dval[0] * 1000);
    }

    if (ping_args.data_size->count > 0) {
        config.data_size = (uint32_t)(ping_args.data_size->ival[0]);
    }

    if (ping_args.count->count > 0) {
        config.count = (uint32_t)(ping_args.count->ival[0]);
    }

    if (ping_args.tos->count > 0) {
        config.tos = (uint32_t)(ping_args.tos->ival[0]);
    }

    // parse IP address
    ip_addr_t target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    if (parse_ip_address(ping_args.host->sval[0], &target_addr) != ESP_OK) {
        return 1;
    }
    config.target_addr = target_addr;

    if (IP_IS_V4(&target_addr)) {
#if CONFIG_LWIP_IPV4
        printf("PING %s (%s) %d data bytes\n", ping_args.host->sval[0], inet_ntoa(*ip_2_ip4(&target_addr)), config.data_size);
#endif
    } else {
#if CONFIG_LWIP_IPV6
        printf("PING %s (%s) %d data bytes\n", ping_args.host->sval[0], inet6_ntoa(*ip_2_ip6(&target_addr)), config.data_size);
#endif
    }

    /* set callback functions */
    esp_ping_callbacks_t cbs = {
        .on_ping_success = cmd_ping_on_ping_success,
        .on_ping_timeout = cmd_ping_on_ping_timeout,
        .on_ping_end = cmd_ping_on_ping_end,
        .cb_args = NULL
    };

    esp_ping_new_session(&config, &cbs, &ping);
    esp_ping_start(ping);
    return 0;
}

void ping_cmd_register_ping(void)
{
    /* Same with: https://linux.die.net/man/8/ping */
    ping_args.host = arg_str1(NULL, NULL, "[host]", "domain name or ip address");
    ping_args.timeout = arg_dbl0("W", "timeout", "<timeout>", "time to wait for response");
    ping_args.interval = arg_dbl0("i", "interval", "<interval>", "seconds between sending each packet");
    ping_args.data_size = arg_int0("s", "packetsize", "<size>", "use <size> as number of data bytes to be sent");
    ping_args.count = arg_int0("c", "count", "<count>", "stop after <count> replies, default: 5");
    ping_args.tos = arg_int0("Q", "tos", "<n>", "Set Type of Service related bits in IP datagrams");
    ping_args.abort = arg_lit0(NULL, "abort", "ping abort -> esp_ping_stop");
    ping_args.end = arg_end(1);
    const esp_console_cmd_t ping_cmd = {
        .command = "ping",
        .help = "send ICMP ECHO_REQUEST to network hosts",
        .hint = NULL,
        .func = &do_ping_cmd,
        .argtable = &ping_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&ping_cmd));
}
