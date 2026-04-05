# P4 WiFi Remote - MAVLink WiFi 桥接器

## 项目概述

P4 WiFi Remote 是一个基于 ESP32-P4 的 WiFi 远程控制与 MAVLink 协议桥接系统。系统通过 WiFi 接收外部数据（来自 PC 仿真器或其他 MAVLink 源），通过 UART 转发给飞控；同时支持将飞控的 UART 数据回传至网络。

---

## 系统架构

```
┌──────────────┐       UDP/TCP        ┌─────────────────┐       UART       ┌─────────────┐
│   PC 端      │ ──────────────────── │   ESP32-P4      │ ────────────────── │   飞控      │
│ vrpn-sim     │   127.0.0.1:8888     │  P4 WiFi Remote  │   TELEM1/TELEM2  │  Flight    │
│ mavlink     │   VISION_POSITION     │  MAVLink Bridge  │   MAVLink 帧     │  Controller │
│ 仿真器      │                       │                  │                   │             │
└──────────────┘                      └──────────────────┘                  └─────────────┘
```

---

## 目录结构

```
P4_WIFI_REMOTEv1.0/
├── main/
│   ├── main.c              # 入口程序
│   ├── wifi_app.c/h        # WiFi STA 连接管理
│   ├── network_app.cpp/h    # UDP/TCP 通信封装 (C++)
│   ├── network_cmd.c       # 网络命令 (net_type, net_target 等)
│   ├── mavlink_bridge.cpp  # MAVLink 桥接核心实现
│   ├── mavlink_bridge.h    # MAVLink 配置与 API
│   ├── bridge_cmd.c/h      # UART 桥接命令 (uart_baud, uart_en 等)
│   └── console_app.c/h     # Console REPL 交互
├── build/                   # 编译输出
└── sdkconfig               # ESP-IDF 配置
```

---

## 硬件配置

### ESP32-P4 引脚分配

| UART      | 功能     | TX 引脚 | RX 引脚 | 默认波特率 |
|-----------|----------|---------|---------|-----------|
| TELEM1    | 飞控主端口 | GPIO20  | GPIO21  | 115200    |
| TELEM2    | 飞控从端口 | GPIO10  | GPIO11  | 115200    |
| DEBUG     | 调试串口  | GPIO22  | GPIO23  | 115200    |

### 网络配置

| 参数       | 默认值  |
|------------|---------|
| UDP 端口   | 8888    |
| TCP 端口   | 8889    |
| 最大 TCP 客户端 | 5   |

---

## 快速开始

### 1. 编译烧录

```bash
# 编译
idf.py build

# 烧录
idf.py flash monitor
```

### 2. 连接 WiFi

```
esp32p4> wifi_set LingLONG_5G 12341234
```

### 3. 配置目标地址（客户端模式）

```
esp32p4> net_type udp
esp32p4> net_target 192.168.110.166 8888
```

### 4. 启用 UART 并启动

```
esp32p4> uart_en 1 1      # 启用 TELEM1
```

---

## Console 命令

### WiFi 命令

| 命令                  | 说明                    |
|-----------------------|------------------------|
| `wifi_set <ssid> <pwd>` | 连接指定 WiFi         |
| `echo <text>`         | 回显测试               |

### 网络命令

| 命令                           | 说明                      |
|--------------------------------|--------------------------|
| `net_type <udp\|tcp>`          | 设置通信类型              |
| `net_mode <client\|server>`    | 设置通信模式              |
| `net_target <IP> <PORT>`       | 设置目标地址（客户端）    |
| `net_port <PORT>`              | 设置本地端口（服务器）    |
| `net_start`                    | 启动通信                  |
| `net_stop`                     | 停止通信                  |
| `net_send <MESSAGE>`           | 发送字符串                |
| `net_status`                   | 显示网络状态              |
| `net_localip`                  | 显示本地 IP               |
| `net_help`                     | 显示帮助                  |

### UART 桥接命令

| 命令                   | 说明                    |
|------------------------|------------------------|
| `uart_baud [id] [baud]`| 查看/设置波特率         |
| `uart_en <id> <0\|1>`  | 启用/禁用 UART          |
| `uart_status`          | 显示所有 UART 状态      |
| `uart_help`            | 显示帮助                |

---

## MAVLink 配置

配置文件：`main/mavlink_bridge.h` 顶部

```c
// UART 配置
#define MAVLINK_TELEM1_ENABLED       1
#define MAVLINK_TELEM1_BAUD          115200
#define MAVLINK_TELEM1_TX_PIN        20
#define MAVLINK_TELEM1_RX_PIN        21

#define MAVLINK_TELEM2_ENABLED       1
#define MAVLINK_TELEM2_BAUD          115200
#define MAVLINK_TELEM2_TX_PIN        10
#define MAVLINK_TELEM2_RX_PIN        11

#define MAVLINK_DEBUG_ENABLED        1
#define MAVLINK_DEBUG_BAUD           115200
#define MAVLINK_DEBUG_TX_PIN         22
#define MAVLINK_DEBUG_RX_PIN         23

// 网络配置
#define MAVLINK_UDP_PORT             8888
#define MAVLINK_TCP_PORT             8889

// 队列配置
#define MAVLINK_QUEUE_SIZE           32
#define MAVLINK_SEQ_WINDOW           10
```

---

## MAVLink v2 消息格式

### VISION_POSITION_ESTIMATE (#102)

PC 端发送 VISION_POSITION_ESTIMATE 消息到 ESP32 的 UDP 8888 端口。

**帧格式（MAVLink v2）：**

| 字节偏移 | 长度 | 字段        | 说明                    |
|---------|------|-------------|------------------------|
| 0       | 1    | STX (0xFD)  | MAVLink v2 帧起始      |
| 1       | 1    | payload_len | 负载长度 (32)          |
| 2       | 1    | incompat    | 不兼容标志              |
| 3       | 1    | compat      | 兼容标志                |
| 4       | 1    | seq         | 序列号                  |
| 5       | 1    | sysid       | 系统 ID                 |
| 6       | 1    | compid      | 组件 ID                 |
| 7-9     | 3    | msgid       | 消息 ID (102)          |
| 10-17   | 8    | usec        | 时间戳（微秒）          |
| 18-21   | 4    | x           | X 位置（米）           |
| 22-25   | 4    | y           | Y 位置（米）           |
| 26-29   | 4    | z           | Z 位置（米）           |
| 30-33   | 4    | roll        | 横滚角（弧度）         |
| 34-37   | 4    | pitch       | 俯仰角（弧度）         |
| 38-41   | 4    | yaw         | 偏航角（弧度）         |
| 42-125  | 84   | covariance  | 协方差矩阵 [21]        |
| 126-127 | 2    | CRC         | CRC16 校验             |

**示例数据：**

```
usec = 1712365400000
x = 1.2, y = 0.5, z = 0.8
roll = 0, pitch = 0, yaw = 1.570796 (90°)
```

Hex 数据：
```
FD200000010101660000704AC5529F019A9999993F0000003FCDCC4C3F0000000000000000DB0FC93F0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000B4B5
```

---

## 典型使用场景

### 场景 1：PC 仿真器 -> ESP32 -> 飞控

PC 端运行 `vrpn-sim-mavlink` 发送视觉定位数据，经 ESP32 转发到飞控。

```bash
# PC 端
./build/fake_vrpn_uav_server --bind :3883 --num-trackers 1 --rate 50

# ESP32 Console
wifi_set LingLONG_5G 12341234
net_target 192.168.110.166 8888
uart_baud 1 460800
uart_en 1 1
```

### 场景 2：QGroundControl 直连

QGC 可以直接通过 UDP 连接到 ESP32。

```bash
# ESP32 Console
wifi_set <ssid> <password>
uart_en 1 1

# QGC 设置
通讯簿 -> 添加：协议 UDP，端口 14550
```

---

## 数据流

### 网络 -> 飞控

```
UDP 数据包到达 (8888)
    ↓
mavlink_bridge 解析
    ↓
有序队列（可选）
    ↓
mavlink_bridge_net_to_all_uarts()
    ↓
UART TX (TELEM1/TELEM2)
    ↓
飞控
```

### 飞控 -> 网络

```
飞控 UART 数据
    ↓
uart_rx_task() 接收
    ↓
callback 通知
    ↓
mavlink_bridge_uart_to_net()
    ↓
TCP 广播到所有客户端
```

---

## 模块说明

### wifi_app

WiFi STA 模式管理模块，负责：
- NVS 初始化
- WiFi 驱动初始化
- 连接事件处理
- 自动重连（最多 5 次，间隔 3 秒）
- IP 地址获取

### network_app

UDP/TCP 通信封装（C++）：
- `espp::UdpSocket` - UDP 收发
- `espp::TcpSocket` - TCP 监听/连接
- 支持客户端/服务器模式
- 自动数据打印（ASCII 或 Hex）

### mavlink_bridge

MAVLink 核心桥接：
- UDP 服务器监听（端口 8888）
- TCP 服务器支持（端口 8889）
- 多 UART 管理（TELEM1, TELEM2, DEBUG）
- 有序队列模式（可选）
- 序列号容错窗口

### console_app

ESP-IDF Console REPL：
- UART 交互界面
- 命令注册与管理
- 自动补全支持

---

## 调试

### 查看日志

```bash
idf.py monitor
```

### 常用日志标签

| 标签            | 说明              |
|-----------------|------------------|
| wifi_app        | WiFi 连接状态     |
| network_app     | 网络通信         |
| mavlink_bridge  | MAVLink 桥接     |
| net_cmd         | 网络命令解析     |
| bridge_cmd      | UART 命令解析    |
| console_app     | Console 交互     |

### 启用 HEX dump

```c
// main/mavlink_bridge.h
#define MAVLINK_LOG_HEXDUMP          1
```

---

## 常见问题

### Q: WiFi 连接失败
- 检查 SSID 和密码
- 确认路由器可连接
- 检查是否超出重连次数限制

### Q: UART 通信异常
- 检查波特率设置：`uart_baud`
- 确认 UART 已启用：`uart_en`
- 检查接线是否正确

### Q: MAVLink 消息不被识别
- 确认使用 MAVLink v2 (STX = 0xFD)
- 检查 CRC16 校验
- 确认消息 ID 为 102 (VISION_POSITION_ESTIMATE)

---

## 开发指南

### 添加新 UART

1. 在 `mavlink_bridge.h` 添加宏定义
2. 在 `mavlink_bridge.cpp` 的 `mavlink_bridge_init()` 添加配置
3. 更新 `UART_ID_MAX` 枚举

### 添加新 Console 命令

1. 在对应的 `_cmd.c` 文件添加命令处理函数
2. 在命令数组中添加定义
3. 在 `console_app_init()` 注册

### 添加新 MAVLink 消息类型

在 `mavlink_bridge.cpp` 修改消息解析逻辑：
```cpp
if (data[0] == MAVLINK_STX_V2) {
    switch (msg_id) {
        case 102:  // VISION_POSITION_ESTIMATE
            // 处理逻辑
            break;
    }
}
```

---

## 版本信息

- 项目版本：v1.0
- 创建日期：2026-03-25
- 最后更新：2026-04-06
- 目标平台：ESP32-P4 + ESP-IDF v5.x
