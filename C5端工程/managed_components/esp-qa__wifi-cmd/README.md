# WiFi Command

[![Component Registry](https://components.espressif.com/components/esp-qa/wifi-cmd/badge.svg)](https://components.espressif.com/components/esp-qa/wifi-cmd)

This repository contains WiFi commands based esp-idf console.

There is no GitHub repository for this component. Please report issues to [the iperf-cmd repository](https://github.com/espressif/iperf-cmd) instead.

### Installation

- To add a plugin command or any component from IDF component manager into your project, simply include an entry within the `idf_component.yml` file.

  ```yaml
  dependencies:
    esp-qa/wifi-cmd:
      version: "^0.2.9"
  ```
- For more details refer [IDF Component Manager](https://docs.espressif.com/projects/idf-component-manager/en/latest/)

---

## Documentation

### Command list

- The available commands may vary depending on the app, sdkconfig, or IDF version.
- Use the `help` command to list available commands and usage instructions.

  ```bash
  wifi stop/start/restart
  # Config
  wifi_mode
  wifi_protocol
  wifi_bandwidth
  wifi_ps
  wifi_country
  # softAP
  ap_set
  # Station
  sta_scan
  sta_connect
  sta_disconnect
  ```

---

### Get Started

---

#### WiFi Initialize

- Use `wifi_cmd_initialize_wifi` to simply start wifi, enable wifi-cmd default handlers and set wifi-cmd status.
- Example usages:

  ```c
  /* start wifi with default config */
  wifi_cmd_initialize_wifi(NULL);
  ```

  ```c
  /* start wifi with custom config */
  wifi_cmd_initialize_cfg_t cfg = WIFI_CMD_INITIALIZE_CONFIG_DEFAULT();
  cfg.disable_11b_rate = true
  wifi_cmd_initialize_wifi(&cfg);
  ```

#### Advanced WiFi Initialize
- If you need to apply additional WiFi configurations between `wifi init` and `wifi start`, you can manually call `wifi_cmd_wifi_init()` and `wifi_cmd_wifi_start()` instead of using `wifi_cmd_initialize_wifi()`.
- NOTE: In this case, the `wifi start/restart` console command may not function as expected. It is recommended to avoid using that command if you follow this approach.

  ```c
  wifi_cmd_wifi_init(NULL);
  /* add other wifi configs before wifi start here */
  wifi_cmd_wifi_start();
  ```

---

#### Register commands

- Use `wifi_cmd_register_xxx()` functions to register the desired commands.
- Example:

  ```c
  wifi_cmd_register_all_basic();
  wifi_cmd_register_wifi_protocol();
  ```
