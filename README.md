# SunrayNext-FlyControl-Helper 🚁

这是一个基于 ESP32-P4 和 ESP32-C5 的高性能无线飞控/远程控制助手项目。

## 📁 项目结构

本仓库采用多端工程管理模式，包含以下两个核心部分：

- **[P4端工程](./P4端工程/)**: 运行在 **ESP32-P4** 上的主控程序。负责核心控制逻辑、视频处理或高级通信协议。
- **[C5端工程](./C5端工程/)**: 运行在 **ESP32-C5** 上的固件。主要负责 Wi-Fi 6 / 蓝牙连接及底层通信链路（Slave 模式）。

---

## 🚀 快速开始

### 1. 环境准备
确保你已经安装了 [ESP-IDF v5.3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/get-started/index.html) 或更高版本（支持 P4 和 C5 系列）。

### 2. 获取代码
```bash
git clone [https://github.com/B0weny-qwq/Fly_Contral-Helper.git](https://github.com/B0weny-qwq/Fly_Contral-Helper.git)
