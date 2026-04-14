#include "network/command/network_command_internal.h"

#include <stdio.h>

int cmd_net_help(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("\n========== 网络通信命令 ==========\n");
    printf("net_type <udp|tcp>\n");
    printf("net_mode <client|server>\n");
    printf("net_target <IP> <PORT>\n");
    printf("net_port <PORT>\n");
    printf("net_start\n");
    printf("net_stop\n");
    printf("net_send <MESSAGE>\n");
    printf("net_status\n");
    printf("net_localip\n");
    printf("net_help\n");
    printf("===================================\n\n");
    return 0;
}
