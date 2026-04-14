#pragma once

#include "esp_console.h"

int cmd_net_type(int argc, char **argv);
int cmd_net_mode(int argc, char **argv);
int cmd_net_target(int argc, char **argv);
int cmd_net_port(int argc, char **argv);
int cmd_net_start(int argc, char **argv);
int cmd_net_stop(int argc, char **argv);
int cmd_net_send(int argc, char **argv);
int cmd_net_status(int argc, char **argv);
int cmd_net_localip(int argc, char **argv);
int cmd_net_help(int argc, char **argv);

const esp_console_cmd_t *network_command_table(size_t *count);
