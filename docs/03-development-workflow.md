# 03 开发工作流

适合谁看：
- 已经完成第一次编译和烧录的人
- 准备开始改文档、改命令或改桥接逻辑的人

读完会得到什么：
- 知道日常开发应该按什么顺序推进
- 知道什么时候先看 P4，什么时候先看 C5
- 知道联调时建议的验证节奏

## 日常开发不要一上来就双端乱改

这个项目是双端协作，但日常开发最好先把改动归类。先判断你改的是哪一层，再决定从哪里开始。

- 如果你改的是控制台命令、网络桥接、UART 转发，先从 P4 开始
- 如果你改的是无线协处理器能力、`esp-hosted` 传输或内存约束，先从 C5 开始
- 如果你改的是两端交界处，先在文档里写清楚输入输出，再分别修改两端

## 推荐开发节奏

### 1. 先定位入口

P4 入口在 `firmware-p4/main/main.c`，真正的启动编排在 `firmware-p4/main/app/service/app_boot.c`。C5 的上游启动入口在 `firmware-c5/main/upstream/esp_hosted_coprocessor.c`，项目自定义约束入口在 `firmware-c5/main/app/service/coprocessor_entry.c`。

### 2. 再定位模块

P4 一般从这些模块里找：

- `wifi_app`
- `app/service/app_boot.c`
- `bridge/service/*`
- `bridge_cmd`
- `network/command/*`
- `network/service/*`
- `bridge_cmd`
- `console_app`

C5 一般先看：

- `upstream/esp_hosted_coprocessor.c`
- `app/service/coprocessor_entry.c`
- `upstream/slave_control.*`
- `upstream/sdio_slave_api.*`
- `upstream/slave_wifi_std.*`
- `config/model/product_feature_flags.h`
- `sdkconfig`

### 3. 再做单点验证

不要改完以后直接做完整联调。更稳的做法是：

- 命令改动先验证控制台行为
- 网络改动先验证网络链路
- UART 改动先验证串口收发
- 两端联动改动最后再做端到端验证

## 一次完整修改通常长这样

1. 读懂相关模块当前行为
2. 确认要动的是 P4、C5，还是两端接口
3. 改动一个最小行为
4. 在本端验证日志和状态
5. 再接入另一端做联调
6. 最后更新对应文档

## 常用命令

### P4

```bash
cd firmware-p4
idf.py build
idf.py flash monitor
```

### C5

```bash
cd firmware-c5
idf.py set-target esp32c5
idf.py build
idf.py flash monitor
```

## 联调顺序建议

第一次联调或大改后联调，建议按这个顺序来：

1. C5 单独启动正常
2. P4 单独启动正常
3. P4 控制台可用
4. Wi-Fi 配置正常
5. 网络配置正常
6. UART 转发打开
7. 端到端消息通过

如果中途失败，不要继续叠加步骤，直接回退到上一个已确认正常的节点。

补充一个当前代码里的重要事实：

- 主 MAVLink 桥接链路由 `app_boot -> mavlink_bridge_init()` 直接拉起，默认监听 `UDP 8888` 和 `TCP 8889`
- `net_*` 命令操作的是 `network/service/*` 里的独立网络运行时，不会直接接管主桥接链路

## 文档何时更新

这套项目很依赖“你是否知道该去哪里看代码”。所以只要你改了下面这些内容，就应该同步更新文档：

- 新增或改变控制台命令
- 改变网络和 UART 的默认关系
- 改变 P4 / C5 分工
- 改变 bring-up 或排查顺序

下一篇的排查文档，会把这些失败点按顺序整理出来，见 [04 常见问题排查](./04-troubleshooting.md)。
