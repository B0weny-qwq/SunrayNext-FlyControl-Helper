# 控制台命令

适合谁看：
- 需要通过串口控制台配置系统的人
- 准备修改命令或新增命令的人

读完会得到什么：
- 知道现有命令分成哪几类
- 知道这些命令推荐按什么顺序使用
- 知道命令层和业务层的边界在哪里

## 先理解控制台在系统里的位置

控制台不是系统的核心业务模块。它更像是给开发者和调试者提供的运行时入口。P4 先把 Wi-Fi 和桥接器初始化起来，然后再由控制台暴露配置命令。

所以当一个命令表现异常时，你既要看命令解析本身，也要看它最终调用的业务模块是否已经处于可工作状态。

## 命令分组

### 基础命令

| 命令 | 作用 |
| --- | --- |
| `echo <text>` | 回显输入，主要用于确认控制台是否工作 |
| `wifi_set <ssid> <password>` | 连接 Wi-Fi |

### 网络命令

| 命令 | 作用 |
| --- | --- |
| `net_type <udp|tcp>` | 选择网络传输类型 |
| `net_mode <client|server>` | 设置客户端或服务端模式 |
| `net_target <ip> <port>` | 设置客户端模式下的远端目标 |
| `net_port <port>` | 设置服务端模式下的本地监听端口 |
| `net_start` | 启动网络服务 |
| `net_stop` | 停止网络服务 |
| `net_send <message>` | 发送测试消息 |
| `net_status` | 查看网络状态与统计 |
| `net_localip` | 查看当前本地 IP |

### UART 与桥接命令

| 命令 | 作用 |
| --- | --- |
| `uart_baud [id] [baud]` | 查看或设置串口波特率 |
| `uart_en <id> <0|1>` | 启用或禁用指定串口 |
| `uart_status` | 查看串口状态与统计 |
| `uart_help` | 查看 UART 帮助 |

## 推荐使用顺序

第一次联调时，不建议随意跳着用命令。更稳的顺序是：

1. `wifi_set`
2. `net_type`
3. `net_mode`
4. `net_target` 或 `net_port`
5. `net_start`
6. `uart_en`
7. `uart_status` / `net_status`

这个顺序的核心思想很简单。先把网络侧准备好，再打开串口侧，不要反过来。

## 典型组合

### 外部设备向飞控发 UDP

```text
wifi_set <ssid> <password>
net_type udp
net_mode client
net_target <pc_ip> 8888
net_start
uart_en 1 1
```

### 本地先看网络状态

```text
net_status
net_localip
uart_status
```

## 命令层和业务层的边界

`console_app` 负责把命令注册到控制台里。`network_cmd` 和 `bridge_cmd` 负责参数解析与调用。真正的业务行为主要仍然落在 `network_app` 和 `mavlink_bridge`。

所以新增命令时，最好保持这种边界，不要把复杂业务直接塞进命令处理函数里。
