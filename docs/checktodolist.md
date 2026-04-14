# P4 / C5 Main 解耦与 Check Todo List

适合谁看：
- 准备重构 `firmware-p4/main` 或 `firmware-c5/main` 的外包工程师
- 需要把超线文件拆开、补静态检查、补测试设计的 agent
- 需要审核重构边界和执行顺序的维护者

读完会得到什么：
- 知道当前 `main` 目录里哪些文件已经超过 `maxline = 300`
- 知道 P4 / C5 后续应该拆成什么样的目标结构
- 知道每个大文件该怎么拆、先拆什么、后拆什么
- 知道后续必须补哪些静态检查、单元测试设计、集成测试设计
- 知道做到什么程度，才算这轮解耦真正通过

> 本文是执行型说明，不是设计讨论稿。默认读者对本项目背景有限，因此本文已经把后续重构需要做的关键决策写死，避免二次决策。

## 固定约束

| 项目 | 固定值 |
| --- | --- |
| 目标范围 | `firmware-p4/main`、`firmware-c5/main` |
| 本轮交付物 | 文档与 checklist，不直接改代码 |
| `maxline` | `300` |
| 目录风格 | `hybrid`，顶层按域，域内按 `service / adapter / model / command / infra` |
| 拆分粒度 | 中等粒度，单个超线文件优先拆成 `3-5` 个职责文件 |
| P4 接口策略 | 允许重构 console、bridge、network 对外接口，不强制兼容旧实现 |
| C5 接口策略 | 允许重组内部接口，但必须区分“对 P4 协作面”和“仅内部实现面” |
| C5 技术主线 | `ESP32-C5 + SDIO`，其他 transport 只作为迁移期兼容负担记录 |
| 测试深度 | 必须写清单元测试设计项与集成测试设计项，但本轮不写测试代码 |

## 当前问题基线

### 当前目录症状

- `firmware-p4/main` 与 `firmware-c5/main` 仍以平铺文件为主，入口、服务、适配、命令、配置交织在一起。
- 多个核心文件同时承载启动装配、状态存储、I/O、协议处理、命令解析和统计，导致任何改动都容易跨层扩散。
- C5 侧还承载了上游 `esp-hosted` 的多 transport、多特性历史负担，和当前 `C5 + SDIO` 主线不一致。
- 当前尚未形成可执行的“拆分完成判定”，因此后续即使做了目录拆分，也可能继续增长成新的大文件。

### P4 超线文件基线

| 文件 | 行数 | 分类 | 当前职责 | 入口 / 耦合点 | 编译条件 | 拆分优先级 |
| --- | ---: | --- | --- | --- | --- | --- |
| `firmware-p4/main/mavlink_bridge.cpp` | 993 | 运行时核心文件 | 桥接编排、UART 初始化、收发任务、MAVLink 解析、网络转发、状态统计 | 被 `main.c`、`bridge_cmd.c`、`network_app.cpp` 间接依赖 | 无条件编译 | P0 |
| `firmware-p4/main/mavlink_bridge.h` | 475 | 运行时核心头文件 | 对外接口、结构体、状态定义、常量、配置声明 | 被 bridge / network / command 多处 include | 无条件编译 | P0 |
| `firmware-p4/main/network_app.cpp` | 611 | 运行时核心文件 | 网络配置状态、UDP/TCP 启停、客户端/服务端任务、发送接口 | 被 `network_cmd.c`、`mavlink_bridge.cpp` 依赖 | 无条件编译 | P0 |
| `firmware-p4/main/network_cmd.c` | 366 | 命令层文件 | `net_*` 命令解析、参数校验、命令注册 | 直接耦合 `network_app` 运行时细节 | 无条件编译 | P1 |

### P4 关注但未超线的文件

| 文件 | 行数 | 分类 | 关注点 |
| --- | ---: | --- | --- |
| `firmware-p4/main/wifi_app.c` | 265 | 运行时服务 | 后续如加入更多状态切换逻辑，容易突破上限 |
| `firmware-p4/main/network_app.h` | 213 | 接口头文件 | 后续应避免继续增长成“大一统声明头” |
| `firmware-p4/main/main.c` | 183 | 启动装配层 | 必须限制为装配，不得继续承载桥接策略 |
| `firmware-p4/main/bridge_cmd.c` | 230 | 命令层 | 改命令时不得回流到底层 UART 实现 |
| `firmware-p4/main/console_app.c` | 152 | 启动 / 注册层 | 保持为命令注册入口，不承载业务逻辑 |

### C5 超线文件基线

| 文件 | 行数 | 分类 | 当前职责 | 入口 / 耦合点 | 编译条件 | 拆分优先级 |
| --- | ---: | --- | --- | --- | --- | --- |
| `firmware-c5/main/slave_wifi_std.c` | 2772 | 运行时核心文件 | Wi-Fi 初始化、配置处理、扫描、事件转发、RPC 请求处理、状态查询 | 被控制分发、协处理器主流程、RPC 层共同牵引 | `CONFIG_ESP_HOSTED_CP_WIFI` | P0 |
| `firmware-c5/main/slave_control.c` | 1845 | 运行时核心文件 | RPC/控制请求分发、OTA、心跳、内存监控、自定义事件编码 | 被 `esp_hosted_coprocessor.c`、`protocomm_pserial.c` 依赖 | 无条件编译 | P0 |
| `firmware-c5/main/Kconfig.projbuild` | 1661 | 配置治理文件 | 暴露大量 feature / transport / example 配置项 | 直接决定编译矩阵和结构复杂度 | 配置入口 | P0 |
| `firmware-c5/main/esp_hosted_coprocessor.c` | 1299 | 启动装配核心 | 协处理器启动、任务编排、收发处理、能力声明、host reset、netif 组织 | 全局入口，牵引 transport / control / wifi | 无条件编译 | P0 |
| `firmware-c5/main/spi_slave_api.c` | 930 | 条件编译 transport 文件 | SPI transport 初始化、队列、收发任务 | 与 transport 选择逻辑耦合 | `CONFIG_ESP_SPI_HOST_INTERFACE` | P2 |
| `firmware-c5/main/spi_hd_slave_api.c` | 927 | 条件编译 transport 文件 | SPI HD transport 初始化、队列、收发任务 | 与 transport 选择逻辑耦合 | `CONFIG_ESP_SPI_HD_MODE` | P2 |
| `firmware-c5/main/sdio_slave_api.c` | 905 | 运行时核心 transport 文件 | SDIO transport 初始化、缓冲区、收发任务、事件上报 | 当前主线 transport | `CONFIG_ESP_SDIO_HOST_INTERFACE` | P0 |
| `firmware-c5/main/uart_slave_api.c` | 719 | 条件编译 transport 文件 | UART transport 初始化、收发、事件队列 | 与 transport 选择逻辑耦合 | 其他 transport 未选中时编译 | P2 |
| `firmware-c5/main/nw_split_router.c` | 527 | 条件能力文件 | 端口规则、路由与过滤策略 | 与 network split / host forwarding 相关 | 无条件编译但功能受配置影响 | P2 |
| `firmware-c5/main/host_power_save.c` | 501 | 运行时服务文件 | 主机唤醒、GPIO、电源状态管理 | 与 transport / host 协作耦合 | 无条件编译 | P1 |
| `firmware-c5/main/slave_wifi_enterprise.c` | 391 | 条件能力文件 | Enterprise Wi-Fi 配置与接入 | 与标准 Wi-Fi 配置共享上下文 | `CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT` | P2 |
| `firmware-c5/main/stats.c` | 389 | 运行时服务文件 | 统计采样、吞吐调试、运行状态任务 | 被多模块调用，容易形成共享状态堆积 | 无条件编译 | P1 |
| `firmware-c5/main/slave_network_split.c` | 380 | 条件能力文件 | network split 相关控制逻辑 | 与路由、控制分发耦合 | `CONFIG_ESP_HOSTED_NETWORK_SPLIT_ENABLED` | P2 |
| `firmware-c5/main/slave_bt.c` | 370 | 条件能力文件 | BT 协处理相关处理 | 与 UART / VHCI / 控制分发相连 | `CONFIG_SOC_BT_SUPPORTED` | P2 |
| `firmware-c5/main/mempool_ll.c` | 352 | 底层支撑文件 | 底层内存池实现 | 被 transport 与收发链路共同依赖 | 无条件编译 | P1 |
| `firmware-c5/main/mempool_ll.h` | 349 | 底层支撑头文件 | 内存池结构、宏、接口声明 | 被多 transport 和核心流程 include | 无条件编译 | P1 |
| `firmware-c5/main/protocomm_pserial.c` | 342 | RPC 适配文件 | TLV / protocomm 适配、请求收发、事件回传 | 连接 control 与 transport | 无条件编译 | P1 |

### C5 关注但未超线的文件

| 文件 | 行数 | 分类 | 关注点 |
| --- | ---: | --- | --- |
| `firmware-c5/main/slave_control.h` | 236 | 接口头文件 | 后续拆分 `control` 时要避免继续堆叠声明 |
| `firmware-c5/main/stats.h` | 246 | 接口头文件 | 应拆出公共统计模型与调试接口 |
| `firmware-c5/main/example_mqtt_client.c` | 279 | 示例文件 | 不属于主链路重构主体，避免误纳入核心层 |
| `firmware-c5/main/example_http_client.c` | 265 | 示例文件 | 同上 |
| `firmware-c5/main/slave_bt_uart_esp32c3_s3.c` | 257 | 条件适配文件 | 若 BT 维持可选能力，应继续压在适配层 |
| `firmware-c5/main/Kconfig.light_sleep` | 224 | 配置文件 | 后续需与 `Kconfig.projbuild` 的主线裁剪保持一致 |

## 目标目录结构

### `firmware-p4/main` 目标结构

```text
firmware-p4/main/
  CMakeLists.txt
  app/
    service/
      app_boot.c
      app_context.c
    model/
      app_context.h
  bridge/
    service/
      bridge_runtime.cpp
      bridge_forward_policy.cpp
      bridge_stats.cpp
    adapter/
      uart_port.cpp
      mavlink_parser.cpp
    model/
      bridge_types.h
      bridge_stats.h
      bridge_config.h
    command/
      bridge_command.c
    infra/
      bridge_defaults.cpp
  network/
    service/
      network_runtime.cpp
      network_session_tcp.cpp
      network_session_udp.cpp
      network_config_store.cpp
    adapter/
      socket_adapter.cpp
    model/
      network_types.h
      network_config.h
      network_status.h
    command/
      network_command.c
    infra/
      network_defaults.cpp
  wifi/
    service/
      wifi_service.c
      wifi_state_machine.c
    adapter/
      wifi_event_adapter.c
    model/
      wifi_status.h
      wifi_config.h
  console/
    service/
      console_boot.c
    command/
      command_registry.c
  transport/
    adapter/
      uart_driver_port.cpp
      network_io_port.cpp
  config/
    model/
      project_defaults.h
  shared/
    model/
      result.h
      error_code.h
```

### P4 分层规则

- `app/` 只放启动装配和应用上下文，不放桥接策略。
- `bridge/` 只负责网络与 UART 之间的桥接编排、转发策略、状态统计。
- `network/` 只负责网络会话与配置状态，不直接调用 CLI。
- `wifi/` 只负责 Wi-Fi 状态与事件适配，不承担桥接逻辑。
- `console/command/` 只做参数解析和命令注册，不直接碰底层驱动。
- `transport/adapter/` 收敛 UART、socket 等 I/O 细节，为 service 层提供稳定接口。
- `shared/model/` 只放多个域都要用的轻量类型，禁止成长为新的“大一统公共头”。

### `firmware-c5/main` 目标结构

```text
firmware-c5/main/
  CMakeLists.txt
  Kconfig.projbuild
  app/
    service/
      coprocessor_boot.c
      coprocessor_runtime.c
    model/
      coprocessor_caps.h
      coprocessor_context.h
    infra/
      app_defaults.c
  control/
    service/
      control_dispatch.c
      control_heartbeat.c
      control_ota.c
      control_monitor.c
    adapter/
      control_event_encoder.c
      control_rpc_adapter.c
    model/
      control_request.h
      control_event.h
  wifi/
    service/
      wifi_runtime.c
      wifi_config_apply.c
      wifi_scan_service.c
      wifi_status_service.c
    adapter/
      wifi_event_forwarder.c
      wifi_scan_adapter.c
      wifi_enterprise_adapter.c
    model/
      wifi_config_model.h
      wifi_scan_model.h
      wifi_status_model.h
  transport/
    service/
      transport_select.c
      transport_buffer_runtime.c
    adapter/
      sdio_transport.c
    legacy/
      spi_transport_legacy.c
      spi_hd_transport_legacy.c
      uart_transport_legacy.c
    model/
      transport_packet.h
      transport_state.h
  power/
    service/
      host_power_policy.c
      host_wakeup_runtime.c
    adapter/
      wakeup_gpio_adapter.c
  rpc/
    service/
      protocomm_session.c
      rpc_event_sender.c
    adapter/
      protocomm_tlv_codec.c
    model/
      rpc_packet_model.h
  buffer/
    service/
      mempool_runtime.c
    adapter/
      mempool_ll_adapter.c
    model/
      mempool_types.h
  stats/
    service/
      stats_runtime.c
      stats_reporter.c
    model/
      stats_snapshot.h
  board/
    adapter/
      gpio_expander_adapter.c
      ext_coex_adapter.c
      light_sleep_adapter.c
      bt_uart_adapter.c
  config/
    model/
      hosted_feature_matrix.h
      project_feature_flags.h
```

### C5 分层规则

- `app/` 只保留协处理器启动装配、上下文和能力注册，不承载 Wi-Fi 控制细节。
- `control/` 负责控制请求分发、心跳、OTA、监控，不直接实现 transport。
- `wifi/` 负责 Wi-Fi 配置与状态，不直接承担 RPC 编排。
- `transport/adapter/` 只保留 `SDIO` 为主线实现，其他 transport 只进入 `legacy/` 并作为待删除对象。
- `rpc/` 负责 `protocomm` / TLV 编解码和事件发送，不直接持有 Wi-Fi 业务状态。
- `buffer/` 负责内存池和缓冲区生命周期，不参与业务决策。
- `board/adapter/` 只放板级相关实现，避免污染控制与 Wi-Fi 主流程。
- `config/` 用于收敛 feature matrix、Kconfig 拆分结果和迁移期兼容选项。

## 通用拆分规则

### 文件与层级规则

- 所有 `*.c`、`*.cpp`、`*.h`、`Kconfig*` 都受 `maxline = 300` 约束。
- 启动层只负责装配，不承载业务策略、不直接拼接 CLI 行为。
- `command` 层不允许直接操作底层驱动，必须通过 `service` 或稳定接口调用。
- `service` 层不允许直接依赖 CLI / REPL 细节。
- `adapter` 层允许接触 ESP-IDF、socket、UART、protocomm、GPIO 等外部接口。
- `model` 层只放轻量结构、枚举、配置快照、状态对象，不放任务、锁、socket、UART 句柄。
- `infra` 层只放默认值、组装辅助、条件编译收口，不承载核心策略。

### 头文件治理规则

- 超过 300 行的头文件必须按“类型 / 常量 / 对外接口”拆开。
- 禁止新的“大一统头文件”继续增长，例如单个头里同时放状态、宏、对外 API、私有结构。
- 所有对外头文件必须标注归属层级，例如 `bridge/model/bridge_types.h`、`control/service/control_dispatch.h`。
- 迁移期兼容头可以保留，但必须是薄封装，且在文件头部写明“待删除”。

### 条件编译规则

- 条件编译尽量集中在 `app/infra/`、`transport/legacy/`、`board/adapter/` 等边缘层。
- 核心策略层禁止使用条件编译替代模块边界。
- 对 `firmware-c5` 来说，`SDIO` 是主路径，`SPI / SPI_HD / UART transport` 不再作为一等结构中心。
- 关闭的功能项必须在 `Kconfig.projbuild` 与 `CMakeLists.txt` 两侧同步裁剪，不能只关编译宏不收目录。

## 重构执行顺序

### 第 1 阶段：盘点与边界划分

- 建立 `main` 目录文件分类表：运行时核心、配置、条件编译、示例、板级适配。
- 为 P4 和 C5 各自绘制 include 关系图，标出跨层直连点。
- 确认所有超线文件的“当前职责”和“目标职责拆分”是否与本文一致。
- 在 C5 侧显式写死 `C5 + SDIO` 主线，其他 transport 进入迁移期名单。

### 第 2 阶段：先拆 P4 的桥接与网络

- 先拆 `mavlink_bridge.cpp` / `mavlink_bridge.h`，把桥接核心从 UART 和解析细节里抽开。
- 再拆 `network_app.cpp`，把网络状态、TCP、UDP、发送会话分离。
- 最后拆 `network_cmd.c`，让命令层只调用服务层。
- 第 2 阶段完成前，不要动 C5 侧核心流程，避免双端同时失稳。

### 第 3 阶段：再拆 C5 的 Wi-Fi / control / transport

- 先拆 `esp_hosted_coprocessor.c`，把入口装配和运行时任务分开。
- 再拆 `slave_control.c` 与 `slave_wifi_std.c`，优先收敛控制分发和 Wi-Fi 请求处理。
- 最后处理 `sdio_slave_api.c`、`protocomm_pserial.c`、`mempool_ll.*`、`stats.c`、`host_power_save.c`。
- 非主线 transport 只做隔离，不做深度重构。

### 第 4 阶段：补静态检查与测试清单

- 让 `check-maxline` 覆盖 `main` 目录核心文件。
- 建立 include 方向检查，确认没有 `command -> adapter` 直连。
- 把本文中的单元测试设计项和集成测试设计项转换成真实测试任务。

### 第 5 阶段：清理旧接口与过渡层

- 删除迁移期兼容头和旧文件别名。
- 把已废弃的非 SDIO transport、无用 feature、无用 Kconfig 选项移出主目录。
- 重新收敛 README、architecture 文档和测试入口，避免文档滞后。

## P4 检查与拆分清单

### `firmware-p4/main/mavlink_bridge.cpp`

- 当前职责：同时承担桥接主流程、UART 初始化与读任务、网络回调、MAVLink 解析、统计和状态管理。
- 目标职责拆分：
  - `bridge/service/bridge_runtime.cpp`
  - `bridge/service/bridge_forward_policy.cpp`
  - `transport/adapter/uart_port.cpp`
  - `bridge/adapter/mavlink_parser.cpp`
  - `bridge/service/bridge_stats.cpp`
- 计划新文件组：
  - `bridge/service/bridge_runtime.cpp`
  - `bridge/service/bridge_runtime.h`
  - `bridge/service/bridge_forward_policy.cpp`
  - `bridge/adapter/mavlink_parser.cpp`
  - `transport/adapter/uart_port.cpp`
  - `bridge/model/bridge_stats.h`
- 依赖迁移顺序：
  1. 先抽 `bridge_types` 和 `bridge_stats`，减少 `.cpp` 对大头文件的反向依赖。
  2. 把 UART 初始化、读任务和发送封装到 `uart_port`。
  3. 把 MAVLink 解析、CRC、帧边界处理抽到 `mavlink_parser`。
  4. 让 `bridge_runtime` 只负责装配回调、状态机和转发调度。
  5. 把统计更新收敛到 `bridge_stats`，禁止分散自增。
- 风险点：
  - UART 任务与网络回调之间的缓冲区所有权
  - 统计字段被多线程并发更新
  - 启停顺序改变后导致 UART 或网络句柄悬挂
- 静态检查项：
  - `bridge_runtime.cpp`、`uart_port.cpp`、`mavlink_parser.cpp`、`bridge_forward_policy.cpp` 全部 `<= 300` 行
  - `main.c` 不再直接 include UART 相关头
  - `bridge_cmd.c` 只能 include `bridge/service` 或 `bridge/model` 对外头
- 单元测试设计项：
  - 非法帧 / 半包 / 粘包解析行为
  - 不同网络模式下的转发策略选择
  - UART 配置参数校验
  - 统计更新是否按成功 / 失败路径分别计数
- 集成测试设计项：
  - 网络输入到 UART 输出的完整链路
  - UART 输入回传到网络的完整链路
  - 多次 `start/stop` 后资源是否可重复释放与重建
- 完成判定：
  - 旧文件不再承担 3 类以上职责
  - 任何桥接策略修改都不需要进入 `uart_port.cpp`
  - 对端到端桥接路径已有独立测试设计项可落地

### `firmware-p4/main/mavlink_bridge.h`

- 当前职责：同时堆放桥接配置、状态结构、统计、UART 参数、对外 API。
- 目标职责拆分：
  - `bridge/model/bridge_types.h`
  - `bridge/model/bridge_config.h`
  - `bridge/model/bridge_stats.h`
  - `bridge/service/bridge_runtime.h`
- 计划新文件组：
  - `bridge/model/bridge_types.h`
  - `bridge/model/bridge_config.h`
  - `bridge/model/bridge_stats.h`
  - `bridge/service/bridge_runtime.h`
- 依赖迁移顺序：
  1. 先抽纯类型和枚举。
  2. 再抽配置与默认值。
  3. 最后保留最薄的 `bridge_runtime.h` 作为服务接口。
- 风险点：
  - 头文件拆分后循环 include
  - 把私有结构误暴露成公共接口
- 静态检查项：
  - 任意新头文件 `<= 300` 行
  - `bridge_runtime.h` 不暴露 UART 私有字段
  - 不出现跨域 include 环回
- 单元测试设计项：
  - 配置对象默认值构造
  - 状态对象序列化 / 打印所需的只读接口
- 集成测试设计项：
  - 桥接服务头文件最小 include 集合能被 `main.c`、`bridge_cmd.c`、`network` 正常使用
- 完成判定：
  - 头文件职责清晰且无循环 include
  - 不再存在新的“桥接万能头”

### `firmware-p4/main/network_app.cpp`

- 当前职责：同时承担网络配置存储、运行时生命周期、UDP 回调、TCP server/client 任务和发送逻辑。
- 目标职责拆分：
  - `network/service/network_runtime.cpp`
  - `network/service/network_session_udp.cpp`
  - `network/service/network_session_tcp.cpp`
  - `network/service/network_config_store.cpp`
  - `network/adapter/socket_adapter.cpp`
- 计划新文件组：
  - `network/service/network_runtime.cpp`
  - `network/service/network_session_udp.cpp`
  - `network/service/network_session_tcp.cpp`
  - `network/service/network_config_store.cpp`
  - `network/model/network_config.h`
  - `network/model/network_status.h`
- 依赖迁移顺序：
  1. 先抽配置和状态模型。
  2. 再抽 UDP / TCP 会话实现。
  3. 最后让 `network_runtime` 只负责状态切换和对外 API。
- 风险点：
  - TCP client 与 server 两种模式共存时状态互斥不清
  - UDP 回调依然直连 bridge 细节
  - 配置变更与运行中重启的竞态
- 静态检查项：
  - `network_command.c` 只能调用 `network_runtime` 的稳定接口
  - UDP / TCP 文件中不出现命令解析逻辑
  - 网络模型头文件不含 socket 句柄
- 单元测试设计项：
  - 网络模式切换状态机
  - 配置对象合法性校验
  - TCP / UDP 发送失败后的状态恢复
- 集成测试设计项：
  - `net_start` 后建立 UDP / TCP 会话
  - 切换目标地址与端口后重新连通
  - 网络断开后是否能恢复
- 完成判定：
  - 改网络模式时无需修改命令层
  - 改 socket 细节时无需修改桥接策略

### `firmware-p4/main/network_cmd.c`

- 当前职责：负责 `net_type`、`net_mode`、`net_target`、`net_port`、`net_start`、`net_stop`、`net_send`、`net_status`、`net_localip`、`net_help` 命令解析和注册。
- 目标职责拆分：
  - `network/command/network_command.c`
  - `network/command/network_command_args.c`
  - `console/command/command_registry.c`
- 计划新文件组：
  - `network/command/network_command.c`
  - `network/command/network_command_args.c`
  - `network/command/network_command_help.c`
- 依赖迁移顺序：
  1. 先把参数解析和帮助文本抽离。
  2. 再把命令执行统一改成调用 `network_runtime`。
  3. 最后把命令注册表收敛回 `command_registry.c`。
- 风险点：
  - 命令层继续偷偷依赖网络内部状态结构
  - 解析逻辑与执行逻辑拆开后帮助文本遗漏
- 静态检查项：
  - 命令层不 include `socket`、`lwIP`、UART 头
  - 命令处理文件 `<= 300` 行
- 单元测试设计项：
  - 参数个数错误
  - IP / 端口格式错误
  - 模式枚举解析和帮助输出
- 集成测试设计项：
  - 控制台执行 `net_*` 命令后驱动网络服务状态变化
  - 错误命令输入不会破坏当前已建立会话
- 完成判定：
  - 命令层只保留文本输入到服务调用的职责

## C5 检查与拆分清单

### `firmware-c5/main/esp_hosted_coprocessor.c`

- 当前职责：入口 `app_main`、能力声明、收发任务、私有包处理、host reset、netif 组织、默认 Wi-Fi 配置。
- 目标职责拆分：
  - `app/service/coprocessor_boot.c`
  - `app/service/coprocessor_runtime.c`
  - `app/model/coprocessor_context.h`
  - `app/model/coprocessor_caps.h`
  - `app/infra/app_defaults.c`
- 计划新文件组：
  - `app/service/coprocessor_boot.c`
  - `app/service/coprocessor_runtime.c`
  - `app/model/coprocessor_context.h`
  - `app/model/coprocessor_caps.h`
  - `app/infra/app_defaults.c`
- 依赖迁移顺序：
  1. 先把能力声明和上下文结构抽走。
  2. 再把 RX / TX 任务与数据包处理收敛到 `coprocessor_runtime.c`。
  3. `app_main` 最后缩到只剩装配和生命周期调用。
- 风险点：
  - 入口拆分后初始化顺序漂移
  - host reset 与 transport 初始化的依赖顺序被打乱
- 静态检查项：
  - `app_main` 不直接出现 Wi-Fi RPC 细节
  - `coprocessor_boot.c` 不直接持有 transport 队列实现
- 单元测试设计项：
  - 能力矩阵构建
  - 启动参数与默认配置装配
  - RX / TX 路径错误码映射
- 集成测试设计项：
  - C5 启动后完成能力上报
  - 主 transport 初始化失败时的退出路径
- 完成判定：
  - 入口层只剩装配
  - 任何控制策略改动都不需要改 `app_main`

### `firmware-c5/main/slave_control.c`

- 当前职责：控制请求分发、OTA、心跳、内存监控、自定义 RPC、事件编码。
- 目标职责拆分：
  - `control/service/control_dispatch.c`
  - `control/service/control_heartbeat.c`
  - `control/service/control_ota.c`
  - `control/service/control_monitor.c`
  - `control/adapter/control_event_encoder.c`
- 计划新文件组：
  - `control/service/control_dispatch.c`
  - `control/service/control_heartbeat.c`
  - `control/service/control_ota.c`
  - `control/service/control_monitor.c`
  - `control/adapter/control_event_encoder.c`
- 依赖迁移顺序：
  1. 先抽事件编码。
  2. 再抽 OTA / 心跳 / 监控三个独立服务。
  3. 最后让 `control_dispatch.c` 只保留请求路由。
- 风险点：
  - dispatcher 拆分后请求编号和处理函数错位
  - 心跳、监控、OTA 共用状态对象时锁粒度不清
- 静态检查项：
  - `dispatch` 文件只做分发，不直接拼 protobuf 事件
  - 控制服务不 include transport 私有头
- 单元测试设计项：
  - 请求号到处理器的映射
  - 心跳事件生成
  - OTA 分支输入校验
  - 内存监控快照转换
- 集成测试设计项：
  - 通过 RPC 触发控制请求
  - 控制事件能回传到 host
- 完成判定：
  - 控制请求新增时只改 dispatch + 对应服务
  - 事件编码不再分散在多个函数里

### `firmware-c5/main/slave_wifi_std.c`

- 当前职责：Wi-Fi 初始化、事件注册、配置处理、扫描、状态查询、大量 `req_wifi_*` RPC 处理、DPP 与 DHCP / DNS 相关处理。
- 目标职责拆分：
  - `wifi/service/wifi_runtime.c`
  - `wifi/service/wifi_config_apply.c`
  - `wifi/service/wifi_scan_service.c`
  - `wifi/service/wifi_status_service.c`
  - `wifi/adapter/wifi_event_forwarder.c`
  - `wifi/adapter/wifi_scan_adapter.c`
- 计划新文件组：
  - `wifi/service/wifi_runtime.c`
  - `wifi/service/wifi_config_apply.c`
  - `wifi/service/wifi_scan_service.c`
  - `wifi/service/wifi_status_service.c`
  - `wifi/adapter/wifi_event_forwarder.c`
  - `wifi/adapter/wifi_scan_adapter.c`
  - `wifi/model/wifi_config_model.h`
  - `wifi/model/wifi_status_model.h`
- 依赖迁移顺序：
  1. 先抽配置模型、扫描模型、状态模型。
  2. 再按“配置应用 / 扫描 / 状态查询 / 事件转发”拆服务。
  3. `wifi_runtime.c` 最后只保留初始化和顶层编排。
- 风险点：
  - RPC handler 与实际 Wi-Fi 动作之间的映射被拆坏
  - 事件回传路径与 control / rpc 层边界不清
  - DPP、DHCP、DNS 等特性被不小心丢失
- 静态检查项：
  - `wifi_runtime.c` 不再出现大批 `req_wifi_*` 处理函数
  - 配置模型头不出现 `esp_wifi_*` 细节
  - 事件回传集中到 `wifi_event_forwarder.c`
- 单元测试设计项：
  - Wi-Fi 配置对象转换
  - 扫描结果映射
  - 状态查询返回值
  - 非法配置输入处理
- 集成测试设计项：
  - Wi-Fi 初始化并完成事件注册
  - 扫描请求、配置请求、状态请求走通
  - 配置变更后事件通知与状态读取保持一致
- 完成判定：
  - Wi-Fi 改动可局限在 `wifi/` 域
  - control / rpc 不再承载 Wi-Fi 具体策略

### `firmware-c5/main/Kconfig.projbuild`

- 当前职责：暴露大量 feature、transport、example、board 与 hosted 配置项，已成为配置层的大一统入口。
- 目标职责拆分：
  - `config/Kconfig.core`
  - `config/Kconfig.transport`
  - `config/Kconfig.wifi`
  - `config/Kconfig.power`
  - `config/Kconfig.examples`
  - 顶层 `Kconfig.projbuild` 只做 source / menu 组装
- 计划新文件组：
  - `firmware-c5/main/config/Kconfig.core`
  - `firmware-c5/main/config/Kconfig.transport`
  - `firmware-c5/main/config/Kconfig.wifi`
  - `firmware-c5/main/config/Kconfig.power`
  - `firmware-c5/main/config/Kconfig.examples`
- 依赖迁移顺序：
  1. 先按主线与非主线 transport 拆菜单。
  2. 再按 Wi-Fi / power / examples 拆。
  3. 顶层保留最薄入口。
- 风险点：
  - 迁移时 `sdkconfig` 兼容名漂移
  - 删除非 SDIO transport 选项时误伤上游宏
- 静态检查项：
  - 顶层 `Kconfig.projbuild` `<= 300` 行
  - transport 菜单默认主路径明确为 `SDIO`
  - examples 菜单与主线功能隔离
- 单元测试设计项：
  - 本项不写传统单测，转为“配置解析检查项”
  - 检查关键默认项是否仍能生成当前产品配置
- 集成测试设计项：
  - 用目标 `sdkconfig` 完成一次 menuconfig / build 验证
  - 关闭非 SDIO transport 后主构建仍通过
- 完成判定：
  - 配置项组织与目录边界一致
  - 非主线配置不再主导主目录结构

### `firmware-c5/main/sdio_slave_api.c`

- 当前职责：当前主线 SDIO transport 的缓冲、收发、事件、初始化与释放。
- 目标职责拆分：
  - `transport/adapter/sdio_transport.c`
  - `transport/service/transport_buffer_runtime.c`
  - `transport/model/transport_packet.h`
  - `transport/model/transport_state.h`
- 计划新文件组：
  - `transport/adapter/sdio_transport.c`
  - `transport/service/transport_buffer_runtime.c`
  - `transport/model/transport_packet.h`
  - `transport/model/transport_state.h`
- 依赖迁移顺序：
  1. 先抽公共缓冲和状态。
  2. 再让 SDIO 适配层只保留硬件 / driver 交互。
  3. 把通用 send / recv 生命周期迁到 `transport_buffer_runtime.c`。
- 风险点：
  - 缓冲区复用和中断回调之间的时序
  - 事件上报从 transport 抽走后丢失通知
- 静态检查项：
  - `sdio_transport.c` 不出现 Wi-Fi / control 逻辑
  - buffer 层不 include board 或 CLI 头
- 单元测试设计项：
  - 包描述结构构建
  - 缓冲状态切换
  - 初始化失败路径回滚
- 集成测试设计项：
  - C5 启动后 SDIO 主链路可工作
  - TX / RX 压力下缓冲回收正常
- 完成判定：
  - SDIO 成为唯一的一等 transport 实现

### `firmware-c5/main/spi_slave_api.c`

- 当前职责：SPI transport 历史实现，当前不是主线。
- 目标职责拆分：
  - 不做深度重构
  - 仅迁入 `transport/legacy/spi_transport_legacy.c`
  - 只保留最薄兼容边界，等待删除
- 计划新文件组：
  - `transport/legacy/spi_transport_legacy.c`
  - `transport/legacy/spi_transport_legacy.h`
- 依赖迁移顺序：
  1. 标记为 `legacy`。
  2. 把共用缓冲逻辑剥离到 transport 公共层。
  3. Phase 5 评估后直接删除。
- 风险点：
  - 共用宏和缓冲接口仍然拖住主线代码
- 静态检查项：
  - `legacy` 文件不允许被 `app/`、`wifi/`、`control/` 直接 include
- 单元测试设计项：
  - 无新增单测设计，只保留最小可编译检查
- 集成测试设计项：
  - 若当前产品不启用 SPI，本项只要求构建矩阵不破坏主线
- 完成判定：
  - SPI 路径不再影响主线目录设计

### `firmware-c5/main/spi_hd_slave_api.c`

- 当前职责：SPI HD transport 历史实现，当前不是主线。
- 目标职责拆分：
  - 不做深度重构
  - 仅迁入 `transport/legacy/spi_hd_transport_legacy.c`
  - 等待 Phase 5 删除
- 计划新文件组：
  - `transport/legacy/spi_hd_transport_legacy.c`
  - `transport/legacy/spi_hd_transport_legacy.h`
- 依赖迁移顺序：
  1. 标记为 `legacy`。
  2. 复用公共缓冲层。
  3. 与顶层 Kconfig 一起裁剪。
- 风险点：
  - 与 SPI / SDIO 共享宏导致删除边界不清
- 静态检查项：
  - 不得从主线 service 层直接 include
- 单元测试设计项：
  - 无新增单测设计，只保留编译检查
- 集成测试设计项：
  - 不影响当前 SDIO 主线的 build 与启动
- 完成判定：
  - SPI HD 从主线结构中降级为迁移期兼容层

### `firmware-c5/main/uart_slave_api.c`

- 当前职责：UART transport 历史实现，当前不是主线。
- 目标职责拆分：
  - 不做深度重构
  - 仅迁入 `transport/legacy/uart_transport_legacy.c`
  - 保留最薄兼容接口，随后删除
- 计划新文件组：
  - `transport/legacy/uart_transport_legacy.c`
  - `transport/legacy/uart_transport_legacy.h`
- 依赖迁移顺序：
  1. 划入 `legacy`。
  2. 把缓冲与事件公共逻辑上收。
  3. Phase 5 删除无产品依赖的旧 transport。
- 风险点：
  - 若仍有头文件被 BT 或 control 侧误依赖，删除时会连带失败
- 静态检查项：
  - UART legacy 不可被主线 Wi-Fi / control 直接依赖
- 单元测试设计项：
  - 无新增单测设计，只保留最小编译检查
- 集成测试设计项：
  - 不影响主线 SDIO bring-up
- 完成判定：
  - UART transport 不再是结构设计的中心对象

### `firmware-c5/main/nw_split_router.c`

- 当前职责：端口过滤、静态转发规则、路由决策。
- 目标职责拆分：
  - `wifi/service/network_route_policy.c`
  - `wifi/model/network_route_rules.h`
  - `wifi/adapter/network_route_parser.c`
- 计划新文件组：
  - `wifi/service/network_route_policy.c`
  - `wifi/model/network_route_rules.h`
  - `wifi/adapter/network_route_parser.c`
- 依赖迁移顺序：
  1. 先抽规则模型。
  2. 再抽字符串解析。
  3. 最后保留策略判断函数。
- 风险点：
  - 端口规则解析与策略执行耦合过深
- 静态检查项：
  - 路由策略不直连 transport 私有实现
- 单元测试设计项：
  - 端口范围解析
  - TCP / UDP 白名单匹配
- 集成测试设计项：
  - network split 打开时静态转发表生效
- 完成判定：
  - 路由规则能独立演进，不继续长在单文件里

### `firmware-c5/main/host_power_save.c`

- 当前职责：唤醒 GPIO、host 唤醒策略、电源状态管理。
- 目标职责拆分：
  - `power/service/host_power_policy.c`
  - `power/service/host_wakeup_runtime.c`
  - `power/adapter/wakeup_gpio_adapter.c`
- 计划新文件组：
  - `power/service/host_power_policy.c`
  - `power/service/host_wakeup_runtime.c`
  - `power/adapter/wakeup_gpio_adapter.c`
- 依赖迁移顺序：
  1. 抽 GPIO 适配。
  2. 再抽唤醒策略。
  3. 最后收敛状态管理。
- 风险点：
  - 上电 / 休眠时序变化导致 host 无法唤醒
- 静态检查项：
  - 电源策略不 include Wi-Fi 业务头
- 单元测试设计项：
  - 唤醒策略状态迁移
  - GPIO 参数校验
- 集成测试设计项：
  - 低功耗与唤醒链路验证
- 完成判定：
  - power 层与 transport / wifi 只通过稳定接口交互

### `firmware-c5/main/slave_wifi_enterprise.c`

- 当前职责：Enterprise Wi-Fi 配置与接入逻辑。
- 目标职责拆分：
  - `wifi/adapter/wifi_enterprise_adapter.c`
  - `wifi/model/wifi_enterprise_config.h`
- 计划新文件组：
  - `wifi/adapter/wifi_enterprise_adapter.c`
  - `wifi/model/wifi_enterprise_config.h`
- 依赖迁移顺序：
  1. 抽配置模型。
  2. 再让 enterprise 适配层只保留与 ESP-IDF 的接缝。
- 风险点：
  - 与 `slave_wifi_std.c` 共用配置路径，拆分后边界混乱
- 静态检查项：
  - enterprise 逻辑只存在于 `wifi/adapter/`
- 单元测试设计项：
  - Enterprise 配置映射
- 集成测试设计项：
  - 打开 enterprise 支持时仍能完成配置下发
- 完成判定：
  - enterprise 作为可选适配层存在，不污染标准 Wi-Fi 主流程

### `firmware-c5/main/stats.c`

- 当前职责：运行状态采样、吞吐统计、调试任务。
- 目标职责拆分：
  - `stats/service/stats_runtime.c`
  - `stats/service/stats_reporter.c`
  - `stats/model/stats_snapshot.h`
- 计划新文件组：
  - `stats/service/stats_runtime.c`
  - `stats/service/stats_reporter.c`
  - `stats/model/stats_snapshot.h`
- 依赖迁移顺序：
  1. 先抽快照模型。
  2. 再拆采样与报告。
- 风险点：
  - 多模块共用统计宏导致接口扩散
- 静态检查项：
  - 业务模块只能依赖 snapshot 或 report 接口
- 单元测试设计项：
  - 快照累计逻辑
  - 速率计算
- 集成测试设计项：
  - 压测时统计输出与实际传输趋势一致
- 完成判定：
  - `stats` 不再成为隐性全局状态桶

### `firmware-c5/main/slave_network_split.c`

- 当前职责：network split 的控制面逻辑。
- 目标职责拆分：
  - `wifi/service/network_split_control.c`
  - `wifi/model/network_split_config.h`
- 计划新文件组：
  - `wifi/service/network_split_control.c`
  - `wifi/model/network_split_config.h`
- 依赖迁移顺序：
  1. 先抽配置模型。
  2. 再把控制流程收进服务层。
- 风险点：
  - 若配置项已经不是主线，容易出现“代码保留但无人验证”
- 静态检查项：
  - network split 只作为可选能力，不影响主路径
- 单元测试设计项：
  - 配置启停切换
- 集成测试设计项：
  - 仅在功能打开时验证
- 完成判定：
  - network split 被隔离成明确可选能力

### `firmware-c5/main/slave_bt.c`

- 当前职责：BT 协处理与 VHCI 相关处理。
- 目标职责拆分：
  - `board/adapter/bt_uart_adapter.c`
  - `control/service/bt_control_runtime.c`
  - `control/model/bt_state.h`
- 计划新文件组：
  - `board/adapter/bt_uart_adapter.c`
  - `control/service/bt_control_runtime.c`
  - `control/model/bt_state.h`
- 依赖迁移顺序：
  1. 先把板级 UART 适配抽出去。
  2. 再把 BT 状态与控制流程拆开。
- 风险点：
  - BT 和 legacy UART transport 头文件互相引用
- 静态检查项：
  - BT 适配只存在于 `board/adapter/`
- 单元测试设计项：
  - BT 状态迁移
- 集成测试设计项：
  - 若 BT 是当前产品启用项，验证启动与发送可用
- 完成判定：
  - BT 能力边界明确，不污染主 transport 设计

### `firmware-c5/main/mempool_ll.c`

- 当前职责：底层内存池管理与队列支撑。
- 目标职责拆分：
  - `buffer/service/mempool_runtime.c`
  - `buffer/adapter/mempool_ll_adapter.c`
  - `buffer/model/mempool_types.h`
- 计划新文件组：
  - `buffer/service/mempool_runtime.c`
  - `buffer/adapter/mempool_ll_adapter.c`
  - `buffer/model/mempool_types.h`
- 依赖迁移顺序：
  1. 先抽类型。
  2. 再区分“公共生命周期”与“底层实现”。
- 风险点：
  - 多 transport 共用内存池时释放顺序不清
- 静态检查项：
  - transport 侧不直接 include LL 私有实现头
- 单元测试设计项：
  - 分配 / 回收
  - 边界容量处理
- 集成测试设计项：
  - 高负载下无明显泄漏或重复释放
- 完成判定：
  - mempool 成为独立 buffer 域，而不是 transport 附属物

### `firmware-c5/main/mempool_ll.h`

- 当前职责：混合放置底层内存池类型、宏、接口声明。
- 目标职责拆分：
  - `buffer/model/mempool_types.h`
  - `buffer/service/mempool_runtime.h`
  - `buffer/adapter/mempool_ll_adapter.h`
- 计划新文件组：
  - `buffer/model/mempool_types.h`
  - `buffer/service/mempool_runtime.h`
  - `buffer/adapter/mempool_ll_adapter.h`
- 依赖迁移顺序：
  1. 类型先拆。
  2. 对外 API 再拆。
  3. 私有宏最后下沉。
- 风险点：
  - 头文件拆分后现有宏失效
- 静态检查项：
  - 公共类型头不暴露底层私有宏
- 单元测试设计项：
  - 类型与接口边界检查
- 集成测试设计项：
  - transport 与 buffer 层头文件最小集可编译
- 完成判定：
  - `mempool_ll.h` 不再继续增长

### `firmware-c5/main/protocomm_pserial.c`

- 当前职责：TLV / protocomm 编解码，请求转发，事件回传，任务循环。
- 目标职责拆分：
  - `rpc/service/protocomm_session.c`
  - `rpc/service/rpc_event_sender.c`
  - `rpc/adapter/protocomm_tlv_codec.c`
  - `rpc/model/rpc_packet_model.h`
- 计划新文件组：
  - `rpc/service/protocomm_session.c`
  - `rpc/service/rpc_event_sender.c`
  - `rpc/adapter/protocomm_tlv_codec.c`
  - `rpc/model/rpc_packet_model.h`
- 依赖迁移顺序：
  1. 先抽编解码。
  2. 再抽事件发送。
  3. 最后让 session 层只做收发编排。
- 风险点：
  - 编解码与请求分发之间的边界定义不清
- 静态检查项：
  - RPC 层不直接 include Wi-Fi 业务头
- 单元测试设计项：
  - TLV 编解码
  - 请求包封装与解包
- 集成测试设计项：
  - 控制请求通过 protocomm 送达 control dispatch
- 完成判定：
  - RPC 层成为独立边界，不再和控制逻辑混写

## 接口重构守则

- 允许改名，只要新名字能清楚反映层级和职责。
- 允许拆分，只要拆分后调用方向更单向、更稳定。
- 禁止跨层直连，例如 `command -> adapter`、`app -> transport legacy`、`wifi -> board`。
- 禁止新的“大一统头文件”继续增长。
- 禁止把条件编译当成模块边界替代品。
- 所有新增接口都必须标注归属层级：`app / service / adapter / command / model / infra`。
- `firmware-p4` 的 console、bridge、network 接口可重构，不为兼容旧命令处理内部结构而妥协。
- `firmware-c5` 的接口必须区分：
  - 对 P4 协作面：稳定、可测试、集中暴露
  - 仅内部实现面：允许频繁演进，不得泄漏到主入口之外

## 静态检查清单

### 必做检查

- `main` 目录已完成按域拆分，不再全部平铺。
- 所有 `*.c`、`*.cpp`、`*.h`、`Kconfig*` 文件 `<= 300` 行。
- 启动层只做装配，不再承载业务策略。
- 命令层不再直连底层实现。
- 不再存在 `command -> adapter`、`service -> command`、`model -> adapter` 的反向依赖。
- C5 主路径只保留 `SDIO` 为一等 transport。

### 建议命令

```bash
python3 /Users/groove/.codex/skills/check-maxline/scripts/check_maxline.py --root firmware-p4/main --max-lines 300 --ext c --ext h --ext cpp
python3 /Users/groove/.codex/skills/check-maxline/scripts/check_maxline.py --root firmware-c5/main --max-lines 300 --ext c --ext h --ext cpp
wc -l firmware-c5/main/Kconfig.projbuild
rg -n '#include "' firmware-p4/main firmware-c5/main
```

### 静态检查通过判定

- `check-maxline` 返回 0。
- `Kconfig.projbuild` 或其拆分后的顶层入口不超线。
- include 关系能映射回目标层级，没有明显逆向依赖。
- 旧 legacy transport 不再被主线 `app / control / wifi` 直接 include。

## 单元测试设计清单

### P4 必须覆盖

- 桥接调度：网络输入、UART 输入、启停状态、失败回退。
- 网络配置状态转换：模式切换、目标地址、端口、启动前置条件。
- 命令参数解析：`net_*`、`uart_*` 的参数合法性与错误输入处理。
- 桥接统计：成功计数、失败计数、状态查询。

### C5 必须覆盖

- Wi-Fi 配置处理：标准配置、扫描结果映射、状态读取、非法输入。
- RPC / control 分发：请求号路由、事件编码、错误码返回。
- Transport 选择：`SDIO` 主路径初始化、失败回退、legacy 隔离。
- 状态 / 统计更新：吞吐统计、监控快照、电源状态。
- Buffer 生命周期：分配、回收、边界容量。

### 单测设计要求

- 每个被拆分的大文件都必须在新模块层面有对应测试责任面。
- 单测优先覆盖“状态转换”“参数校验”“策略判断”“错误路径”。
- 单测不直接依赖 CLI 文本输入，命令解析可独立测。
- 单测不直接依赖真实 UART / SDIO / Wi-Fi 设备，优先使用接口替身。

## 集成测试设计清单

### P4 集成测试

- P4 启动并装配 Wi-Fi、bridge、console。
- P4 网络流量到 UART 的转发链路。
- UART 到网络的回传链路。
- 修改网络配置后重新连通。
- 控制台命令驱动服务状态变化且不破坏现有会话。

### C5 集成测试

- C5 启动并完成协处理器主流程。
- `SDIO` 主链路初始化、收发、异常恢复。
- 控制请求通过 RPC 到达控制分发层。
- Wi-Fi 初始化、配置、扫描、事件回传。
- host power save / wakeup 主流程。

### P4 / C5 联调测试

- P4 / C5 协作主链路能建立。
- P4 通过 C5 获得无线能力后完成网络桥接。
- 回归验证 console / config / transport 切换等关键流程。
- 非主线能力关闭后不影响主线链路。

## 通过判定 Checklist

- [ ] 结构通过：`firmware-p4/main`、`firmware-c5/main` 均完成按域拆分
- [ ] `maxline` 通过：所有受治理文件 `<= 300` 行
- [ ] 启动层通过：启动层只保留装配逻辑
- [ ] 命令层通过：命令层不再直连底层实现
- [ ] 静态依赖通过：无明显跨层反向依赖
- [ ] P4 通过：桥接与网络已完成职责分拆
- [ ] C5 通过：`SDIO` 主线明确，legacy transport 已隔离
- [ ] 配置治理通过：`Kconfig.projbuild` 已拆分并与主线一致
- [ ] 单元测试设计完整：每个大文件都有对应测试责任面
- [ ] 集成测试设计完整：P4、C5、双端联调都有可执行测试项
- [ ] 迁移顺序完整：可按阶段推进，不需要补充重大决策
- [ ] 可交付第三方：外包工程师或 agent 可直接按本文执行
