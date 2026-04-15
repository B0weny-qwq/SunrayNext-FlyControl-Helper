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

当前代码里有一个很重要的区分：

- `uart_*` 命令操作的是主 MAVLink 桥接器
- `net_*` 命令操作的是 `network/service/*` 里的独立网络运行时
- 主桥接链路本身会在启动时自动拉起默认 `UDP 8888` / `TCP 8889` 监听，不需要先执行 `net_start`

## 命令分组

### 基础命令

| 命令 | 作用 |
| --- | --- |
| `echo <text>` | 回显输入，主要用于确认控制台是否工作 |
| `wifi_set <ssid> <password>` | 连接 Wi-Fi |

### 网络命令

| 命令 | 作用 |
| --- | --- |
| `net_type <udp|tcp>` | 选择独立调试通道的传输类型 |
| `net_mode <client|server>` | 设置独立调试通道的客户端或服务端模式 |
| `net_target <ip> <port>` | 设置独立调试通道在客户端模式下的远端目标 |
| `net_port <port>` | 设置独立调试通道在服务端模式下的本地监听端口 |
| `net_start` | 启动独立调试通道 |
| `net_stop` | 停止独立调试通道 |
| `net_send <message>` | 通过独立调试通道发送测试消息 |
| `net_status` | 查看独立调试通道状态与统计 |
| `net_localip` | 查看当前本地 IP |

### UART 与桥接命令

| 命令 | 作用 |
| --- | --- |
| `uart_baud [id] [baud]` | 查看或设置串口波特率 |
| `uart_en <id> <0|1>` | 启用或禁用指定串口 |
| `uart_status` | 查看串口状态与统计 |
| `uart_help` | 查看 UART 帮助 |

当前 `uart_*` 命令按数组下标工作：

- `0 = TELEM1`
- `1 = TELEM2`
- `2 = DEBUG`

## 推荐使用顺序

第一次联调时，不建议随意跳着用命令。对于主桥接链路，更稳的顺序是：

1. `wifi_set`
2. 确认 P4 已拿到 IP
3. 从 PC 直接向 `8888/udp` 或 `8889/tcp` 发流量
4. `uart_en`
5. `uart_status`

这个顺序的核心思想很简单。先把 Wi-Fi 和默认桥接监听准备好，再打开串口侧，不要把 `net_*` 调试通道和主链路混在一起。

## 典型组合

### 外部设备向飞控发 UDP

```text
wifi_set <ssid> <password>
uart_en 0 1
uart_status
```

这之后从 PC 直接向 P4 当前 IP 的 `8888/udp` 发送数据即可进入主桥接链路。

### 独立验证 `net_*` 调试通道

```text
wifi_set <ssid> <password>
net_type udp
net_mode client
net_target <pc_ip> 5000
net_start
net_status
net_localip
```

## 命令层和业务层的边界

`console_app` 负责把命令注册到控制台里。`network/command/*` 和 `bridge_cmd` 负责参数解析与调用。真正的业务行为分别落在 `network/service/*` 和 `bridge/service/*`。

所以新增命令时，最好保持这种边界，不要把复杂业务直接塞进命令处理函数里。
