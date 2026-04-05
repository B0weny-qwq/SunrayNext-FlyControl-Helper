# P4 WiFi Remote - MAVLink WiFi 桥接器

## 📝 项目概述
**P4 WiFi Remote** 是一个基于 **ESP32-P4** 的高性能无线控制与 MAVLink 协议桥接系统。

系统通过 WiFi 接收外部数据（如 PC 仿真器、QGC 地面站），通过高性能 UART 转发给飞控；同时支持将飞控的回传数据通过 TCP/UDP 广播至网络端，实现远程透明传输。特别针对 VRPN 仿真及视觉位置估计（`VISION_POSITION_ESTIMATE`）进行了透传优化。

---

## 🏗️ 系统架构
    ┌──────────────┐       UDP/TCP        ┌─────────────────┐       UART       ┌─────────────┐
    │    PC 端      │ ──────────────────── │    ESP32-P4     │ ────────────────── │    飞控     │
    │ vrpn-sim     │   127.0.0.1:8888     │  P4 WiFi Remote  │   TELEM1/TELEM2  │   Flight    │
    │ mavlink      │   VISION_POSITION    │  MAVLink Bridge  │   MAVLink 帧      │  Controller │
    │ 仿真器        │                      │                 │                  │             │
    └──────────────┘                      └─────────────────┘                  └─────────────┘

---

## 📂 目录结构
* **main/**: 核心源码目录
    * `main.c`: 系统初始化与入口。
    * `wifi_app.c/h`: WiFi STA 连接管理逻辑。
    * `network_app.cpp/h`: 基于 C++ 封装的 UDP/TCP 通信。
    * `network_cmd.c`: 网络交互命令 (`net_type`, `net_target` 等)。
    * `mavlink_bridge.cpp`: MAVLink 协议解析与桥接核心实现。
    * `bridge_cmd.c/h`: UART 桥接控制命令 (`uart_baud`, `uart_en` 等)。
    * `console_app.c/h`: 基于 ESP-IDF REPL 的控制台交互。
* **sdkconfig**: 针对 P4 优化的编译配置。
* **CMakeLists.txt**: 项目构建脚本。

---

## ⚡ 硬件配置 (ESP32-P4)

### 1. UART 端口分配
| 端口名称 | 功能 | TX 引脚 | RX 引脚 | 默认波特率 |
| :--- | :--- | :--- | :--- | :--- |
| **TELEM1** | 飞控主端口 | GPIO20 | GPIO21 | 115200bps |
| **TELEM2** | 飞控辅助口 | GPIO10 | GPIO11 | 115200bps |
| **DEBUG** | 调试串口 | GPIO22 | GPIO23 | 115200bps |

### 2. 默认网络参数
* **UDP Port**: `8888` (用于接收来自仿真器的数据包)
* **TCP Port**: `8889` (用于向网络客户端广播回传数据)
* **Max Clients**: `5` (支持的最大 TCP 客户端同时连接数)

---

## 💻 Console 命令交互详解
在终端串口输入以下命令进行实时配置：

### 1. WiFi 管理
* `wifi_set [SSID] [Password]`: 连接到指定的 WiFi 热点。
* `echo [Text]`: 简单的串口回显测试。

### 2. 网络通信 (`net_` 系列)
* `net_type [udp/tcp]`: 设置当前的通信协议类型。
* `net_mode [client/server]`: 设置通信的主从模式。
* `net_target [IP] [Port]`: 设置远程目标地址（仅在客户端模式下生效）。
* `net_port [Port]`: 设置本地监听端口（仅在服务器模式下生效）。
* `net_start`: 手动启动网络服务。
* `net_stop`: 手动停止网络服务。
* `net_status`: 查看当前网络状态、连接信息及流量统计。
* `net_localip`: 查询当前 ESP32 的局域网 IP 地址。

### 3. UART 桥接 (`uart_` 系列)
* `uart_baud [id] [baud]`: 设置串口波特率 (1:TELEM1, 2:TELEM2)。
* `uart_en [id] [0/1]`: 开启(1)或关闭(0)指定串口的 MAVLink 转发功能。
* `uart_status`: 查看所有 UART 端口的当前运行状态。

---

## 📡 MAVLink V2 核心消息解析
系统针对 **`VISION_POSITION_ESTIMATE (#102)`** 消息进行了透传优化：
* **起始符**: `0xFD` (MAVLink v2 标志)。
* **消息 ID**: `102`。
* **负载长度**: 32 Bytes (包含位置与姿态的浮点数数据)。
* **安全机制**: 具备自动序列号校验与 CRC16 完整性校验，确保飞控接收的数据帧 100% 可靠。

---

## 🚀 快速开始

### 环境准备
确保已配置好 **ESP-IDF v5.3** 或更高版本的开发环境。

### 编译烧录
    idf.py build flash monitor

### 初始化配置
1.  输入 `wifi_set SSID PWD` 完成配网。
2.  输入 `net_target PC_IP 8888` 指定数据来源。
3.  输入 `uart_en 1 1` 开启飞控转发逻辑。

---

**Maintained by B0weny-qwq**
