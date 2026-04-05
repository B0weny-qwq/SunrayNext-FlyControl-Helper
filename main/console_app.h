/**
 * @file console_app.h
 * @brief Console 应用层接口定义
 *
 * @details
 * 本模块提供命令行交互界面（REPL）的初始化接口。
 * 支持通过 UART 输入命令，实现 WiFi 连接等交互功能。
 *
 * @author P4 Team
 * @date 2026-03-25
 *
 * @defgroup console_app Console Application
 * @brief Console 命令行交互模块
 * @{
 */

#pragma once

/**
 * @brief 初始化 Console REPL 并启动
 *
 * @details
 * 配置并启动 ESP-IDF Console 交互式命令行：
 * - 配置 UART 硬件接口
 * - 注册可用命令（echo, wifi_set）
 * - 启动 REPL 进入命令等待状态
 *
 * @note
 * 此函数会阻塞调用线程，REPL 在独立任务中处理命令。
 *
 * @return 始终返回 0
 *
 * @see wifi_app_connect_with_creds()
 */
int console_app_init(void);

/** @} */ // end of console_app group
