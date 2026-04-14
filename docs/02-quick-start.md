# 02 快速开始

适合谁看：
- 想先把项目编译出来、烧进去、看到基本链路的人
- 还不准备立刻改代码，但想知道项目怎么运行的人

读完会得到什么：
- 知道第一次准备环境需要做什么
- 知道 P4 和 C5 的构建命令
- 知道第一次联调时建议的顺序

## 先理解这次上手要做什么

第一次上手不要把目标定得太大。最稳妥的目标不是“一次做完整个飞控联调”，而是按顺序确认三件事：

1. 工程能编译
2. 固件能烧录
3. P4 能启动控制台，C5 能启动无线协处理器逻辑

等这三件事通过以后，再去验证网络和 UART 的完整链路。

## 环境准备

你至少需要这些条件：

- 已安装 ESP-IDF v5.3 或更新版本
- 已准备好 ESP32-P4 和 ESP32-C5 对应工具链
- 有串口烧录环境
- 知道两块板各自对应的下载口

如果当前终端里 `idf.py` 还不可用，先加载 ESP-IDF 环境，再继续后面的步骤。

## 仓库结构

第一次上手只需要先关注下面两个目录：

- `firmware-p4/`
- `firmware-c5/`

P4 是主逻辑入口。C5 是无线协处理器入口。先各自编译通过，再做双端联调。

## 第一次编译

### 编译 P4

```bash
cd firmware-p4
idf.py build
```

### 编译 C5

```bash
cd ../firmware-c5
idf.py set-target esp32c5
idf.py build
```

这里的 `set-target` 只对 C5 子工程强调一次，因为它的目标芯片要明确落到 ESP32-C5。

## 第一次烧录

### 烧录 P4

```bash
cd firmware-p4
idf.py flash monitor
```

### 烧录 C5

```bash
cd ../firmware-c5
idf.py set-target esp32c5
idf.py flash monitor
```

如果你的 C5 板在烧录完成后总是停在 `waiting for download`，先不要立刻怀疑固件。先看 [C5 烧录与启动模式说明](./guides/c5-flashing-boot-mode.md)。对这块板来说，`BOOT` 在复位瞬间的电平会决定它是继续停在下载模式，还是从 Flash 启动应用。

## 第一次联通验证

第一次联通验证建议只看“设备是否起来”，不要一开始就追求完整飞控业务。

### 先看 P4

P4 启动后应该完成三件事：

- 初始化 Wi-Fi
- 初始化桥接器
- 启动控制台

如果一切正常，你应该能在串口里看到控制台可用，并能输入命令。P4 的启动顺序可以参考下面这张图。

![P4 启动流程图](./diagrams/rendered/startup-flow.svg)

### 再看 C5

C5 启动时，重点不是立即发业务数据，而是确认它以 `esp-hosted` 协处理器固件的方式启动，并且使用的是预期的 SDIO 传输配置。

如果日志里出现 `boot:0x28 (DOWNLOAD(UART0/USB))`，说明它这次没有进入应用。先回到 [04 常见问题排查](./04-troubleshooting.md) 的 C5 启动部分，再结合 [C5 烧录与启动模式说明](./guides/c5-flashing-boot-mode.md) 调整 `BOOT` 线在复位前的状态。

## 第一次推荐输入的命令

P4 端先从最简单的控制台操作开始：

```text
wifi_set <ssid> <password>
net_type udp
net_target <pc_ip> 8888
uart_en 1 1
```

这里不要一次改很多参数。先让最基础的默认链路走通，再考虑改串口波特率、协议模式或客户端/服务端模式。

## 下一步看什么

- 想知道日常改代码时的动作顺序，看 [03 开发工作流](./03-development-workflow.md)
- 想知道启动失败或联调失败怎么查，看 [04 常见问题排查](./04-troubleshooting.md)
