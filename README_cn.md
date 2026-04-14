# SunrayNext-FlyControl-Helper

SunrayNext-FlyControl-Helper 是一个基于 ESP32-P4 与 ESP32-C5 的双芯片无线飞控辅助平台。仓库同时包含 P4 侧的 MAVLink 桥接主控固件，以及 C5 侧经过定制的无线协处理器固件。

英文文档请见 [README.md](./README.md)。
结构化开发文档请见 [docs/README.md](./docs/README.md)。

## 项目概览

项目由两个相互配合的固件工程组成：

| 组件 | 目标芯片 | 作用 |
| --- | --- | --- |
| [`firmware-p4`](./firmware-p4/) | ESP32-P4 | 主控程序，负责 Wi-Fi / UART 桥接、控制台命令、MAVLink 转发 |
| [`firmware-c5`](./firmware-c5/) | ESP32-C5 | 基于 `esp-hosted` 定制的 SDIO Slave 固件，为 P4 提供无线链路 |

仓库中也包含硬件设计与硬件资料：

| 目录 | 说明 |
| --- | --- |
| [`pcb-design`](./pcb-design/) | PCB 设计文件、BOM、贴片坐标和制造输出 |
| [`hardware-assets`](./hardware-assets/) | 板卡照片、渲染图和结构导出文件 |

## 架构说明

P4 固件负责接收来自上位机、仿真器或地面站的网络数据，并通过 UART 转发给飞控。C5 固件负责无线网络能力，并通过 SDIO 作为 P4 的无线协处理器。

典型数据流如下：

```text
上位机 / 仿真器 / 地面站
          |
       UDP / TCP
          |
    ESP32-P4 桥接程序
          |
         UART
          |
         飞控

ESP32-C5 通过 SDIO 为 P4 提供无线链路。
```

## 仓库结构

```text
.
├── firmware-p4/
├── firmware-c5/
├── pcb-design/
├── hardware-assets/
├── README.md
└── README_cn.md
```

## 环境要求

- ESP-IDF v5.3 或更新版本
- 支持 ESP32-P4 与 ESP32-C5 的交叉编译环境
- 可用于两块板卡的串口烧录环境
- 用于联调的 ESP32-C5 模组与 ESP32-P4 开发板

## 快速开始

### 1. 获取代码

```bash
git clone <repo-url>
cd SunrayNext-FlyControl-Helper
```

### 2. 编译 P4 固件

```bash
cd firmware-p4
idf.py build
```

### 3. 编译 C5 固件

```bash
cd ../firmware-c5
idf.py set-target esp32c5
idf.py build
```

## P4 固件工作流

`firmware-p4` 是系统中面向业务逻辑的一侧，负责初始化 Wi-Fi、创建 MAVLink 桥接器，并通过 ESP-IDF Console 暴露运行期配置能力。

主要能力：

- Wi-Fi STA 配网
- UDP / TCP 网络传输
- 多串口 UART 转发
- 控制台命令动态配置网络与串口行为

常用流程：

```bash
cd firmware-p4
idf.py flash monitor
```

常见控制台命令：

```text
wifi_set <ssid> <password>
net_type udp
net_target <pc_ip> 8888
uart_en 1 1
```

详细说明见 [`firmware-p4/README.md`](./firmware-p4/README.md)。

## C5 固件工作流

`firmware-c5` 是基于 `esp-hosted` 定制的 SDIO Slave 固件，面向 WTP4C5-S1 一类模块，并针对无 PSRAM 环境做了适配与裁剪。

关键注意事项：

- 以仓库内现有 `sdkconfig` 为基线
- 保持 no-PSRAM 相关配置
- 保持与 P4 侧匹配的 SDIO 传输配置

编译与烧录：

```bash
cd firmware-c5
idf.py set-target esp32c5
idf.py build flash monitor
```

如果某些 C5 板在烧录完成后总是停在 `waiting for download`，不要先把它当成固件问题。对当前验证过的这块板，正确做法是先让 `BOOT` 进入下载模式开始烧录，等写 Flash 阶段开始后再把 `BOOT` 改接到 `3.3V`，这样烧录结束后的自动复位就会进入 `SPI_FAST_FLASH_BOOT`。完整说明见 [docs/guides/c5-flashing-boot-mode.md](./docs/guides/c5-flashing-boot-mode.md)。

详细说明见 [`firmware-c5/README.md`](./firmware-c5/README.md)。

## 硬件资料

硬件相关内容已与固件目录分离：

- [`pcb-design`](./pcb-design/) 保存生产与装配文件
- [`hardware-assets`](./hardware-assets/) 保存板卡图片与导出素材

这些目录内部仍可能保留中文文件名；本次整理仅统一了仓库顶层目录名称。

## 说明

- `firmware-c5` 中包含大量托管组件，默认应视为上游依赖，不建议无目的地改动。
- 不要随意重置或重新生成 C5 侧 `sdkconfig`，当前配置包含板级内存与传输约束。
- 顶层文档采用英文主 README，加一份中文镜像 [`README_cn.md`](./README_cn.md)。
