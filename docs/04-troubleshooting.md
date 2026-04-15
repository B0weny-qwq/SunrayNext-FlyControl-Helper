# 04 常见问题排查

适合谁看：
- 编译、烧录、启动或联调过程中卡住的人
- 想知道出现问题时先查哪里的人

读完会得到什么：
- 知道常见失败点是什么
- 知道推荐的排查顺序
- 知道问题更可能在 P4、C5 还是联动边界

## 排查时最重要的一条规则

一次只确认一个层级。不要在“环境没准备好”的时候去怀疑桥接逻辑，也不要在“控制台没起来”的时候去追网络包。

## 最常见的四类问题

### 1. 构建工具不可用

现象：

- `idf.py` 找不到
- 目标芯片没有正确设置

先查：

- ESP-IDF 环境是否已经加载
- 当前终端是否继承了工具链环境

### 2. 设备能烧录，但启动不正常

现象：

- 没有进入预期日志
- P4 没有控制台
- C5 没有进入协处理器固件启动逻辑
- C5 日志停在 `waiting for download`

先查：

- 是否烧到了正确的目标板
- 是否用了错误的 `sdkconfig`
- 是否在 C5 端破坏了 no-PSRAM 或 SDIO 相关配置
- C5 的 `BOOT` 线在复位瞬间是否仍然把芯片带回下载模式

如果你在 C5 上看到下面这段日志：

```text
boot:0x28 (DOWNLOAD(UART0/USB))
waiting for download
```

说明问题还停留在“启动模式选择”这一层，还没进入应用。对当前这块板，已经验证可用的做法是：

1. 先让 `BOOT` 进入下载模式，开始执行烧录。
2. 烧录进入写 Flash 阶段后，把 `BOOT` 改接到 `3.3V`。
3. 等待烧录结束后的自动复位。
4. 这次复位会重新采样 `BOOT`，然后进入 `SPI_FAST_FLASH_BOOT`。

详细步骤见 [C5 烧录与启动模式说明](./guides/c5-flashing-boot-mode.md)。

### 3. P4 控制台可用，但网络不工作

现象：

- `wifi_set` 失败
- P4 没拿到 IP
- PC 发到 `UDP 8888` 或连到 `TCP 8889` 没有进入主桥接链路
- `net_start` 失败（如果你正在单独验证 `net_*` 调试通道）

先查：

- Wi-Fi 是否连上
- P4 当前本地 IP 是多少
- 外部流量是否真的发到了主桥接默认端口 `8888/udp` 或 `8889/tcp`
- 如果你查的是 `net_*` 路径，再确认 `net_type`、`net_mode`、`net_target` / `net_port` 是否已配置

### 4. 网络正常，但飞控链路不通

现象：

- 网络收到了数据
- 但是 UART 侧没有看到转发结果

先查：

- `uart_en` 是否已打开
- 波特率是否匹配
- 实际使用的是哪一路 UART
- 发送的数据是否真的是预期帧格式

## 推荐排查顺序

先看文字说明，再看下面这张流程图。

![Bring-up 排查流程图](./diagrams/rendered/bringup-checklist-flow.svg)

推荐顺序：

1. 工具链是否可用
2. 能否编译
3. 能否烧录
4. P4 是否启动控制台
5. C5 是否启动协处理器逻辑
6. Wi-Fi 是否连通
7. 网络是否启动
8. UART 是否打开
9. 端到端消息是否通过

## 快速定位建议

| 现象 | 优先看哪里 |
| --- | --- |
| 控制台命令不存在 | `firmware-p4/main/console_app.c`、`firmware-p4/main/network/command/network_command.c`、`firmware-p4/main/bridge_cmd.c` |
| `net_*` 命令配置无效 | `firmware-p4/main/network/service/network_runtime.cpp`、`firmware-p4/main/network/command/network_command*.c` |
| 主桥接收不到网络数据 | `firmware-p4/main/app/service/app_boot.c`、`firmware-p4/main/bridge/service/bridge_network_runtime.cpp` |
| UART 无转发 | `firmware-p4/main/bridge/service/bridge_runtime.cpp`、`firmware-p4/main/transport/adapter/uart_port.cpp`、`firmware-p4/main/bridge_cmd.c` |
| C5 启动配置异常 | `firmware-c5/main/upstream/esp_hosted_coprocessor.c`、`firmware-c5/main/app/service/coprocessor_entry.c`、`sdkconfig` |

## 什么时候该回头补文档

如果你排查时发现自己必须读很多源码才能知道下一步查什么，这通常说明文档还不够。把你这次排查依赖的关键线索补回文档，能让下一个人少走很多弯路。
