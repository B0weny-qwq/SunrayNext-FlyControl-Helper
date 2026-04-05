# C5-Side Firmware (SDIO Slave)

本工程为适配 **WTP4C5-S1** 模组定制的 SDIO 虚拟网卡固件（Slave 模式），专为 ESP32-P4 提供 Wi-Fi 链路。

## ⚠️ 核心配置 (Menuconfig 定制)

本项目对官方 `esp-hosted` 历程进行了关键参数修改，**严禁随意重置 `sdkconfig`**：

- **硬件适配**: 针对 **WTP4C5-S1** 模组的 Flash 布局进行了分区表与频率适配。
- **内存优化**: 针对 **No PSRAM** 环境进行了裁剪。关闭了所有依赖外部 RAM 的缓存机制，优化了内部 SRAM 的堆栈分配，确保在 512KB SRAM 内稳定运行 SDIO 吞吐。
- **通信接口**: 锁定为 **SDIO 接口**。
- **功能裁剪**: 移除了非必要的调试日志和冗余组件，以腾出内存空间给 Wi-Fi 数据包缓冲。

## 🛠️ 编译说明

1. **目标芯片**: ESP32-C5
2. **注意**: 编译前请确保使用本项目自带的 `sdkconfig`。如果手动修改，务必保持 **No PSRAM** 相关的内存配置，否则会导致初始化失败（ESP_ERR_NO_MEM）。

```bash
idf.py set-target esp32c5
idf.py build flash monitor
