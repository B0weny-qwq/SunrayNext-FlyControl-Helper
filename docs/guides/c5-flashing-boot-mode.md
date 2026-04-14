# C5 烧录与启动模式说明

适合谁看：
- 正在给 `firmware-c5` 烧录固件的人
- 烧录成功后却看到 `waiting for download` 的人
- 需要理解这块 C5 板为什么“能烧进去，但不自动跑应用”的人

读完会得到什么：
- 知道这块 C5 板的最小接线方式
- 知道一套已经验证可用的烧录与进入 monitor 步骤
- 知道 `BOOT` 线为什么会影响“烧录”和“正常启动”两个阶段

## 先看现象

这块 C5 板最容易让人困惑的现象是：烧录已经成功，但复位后没有进入应用，而是停在下面这段 ROM 日志里：

```text
ESP-ROM:esp32c5-eco2-20250121
rst:0x1 (POWERON),boot:0x28 (DOWNLOAD(UART0/USB))
waiting for download
```

这说明芯片在复位瞬间被判定为“进入下载模式”，而不是“从 Flash 启动应用”。

当它正常从 Flash 启动时，日志会变成这样：

```text
rst:0x1 (POWERON),boot:0x38 (SPI_FAST_FLASH_BOOT)
```

所以这里最重要的不是应用逻辑，而是复位瞬间 `BOOT` 引脚的电平状态。

## 当前验证可用的最小接线

这套接线假设你使用外部 USB 下载器，板子本体没有独立的 `EN` 按钮，复位动作由下载器侧完成。

```text
MacBook
  |
  | USB-C
  v
USB 下载器
  |
  | TX   --------------------> C5 RX
  | RX   <-------------------- C5 TX
  | GND  --------------------> C5 GND
  | 5V   --------------------> C5 5V / VIN
  | EN   --------------------> C5 EN / RST
  |
  | BOOT --------------------> C5 BOOT
```

运行阶段的 `P4 <-> C5` 互联线保持项目原有 SDIO 链路，不在这里重复展开。这里的重点只放在“如何让 C5 烧录后从 Flash 正常启动”。

## 已验证可用的操作顺序

如果你的板子在烧录完成后总是回到 `waiting for download`，按下面这组步骤做：

1. 开始烧录前，让 `BOOT` 处于“下载模式”接法。
2. 执行 `idf.py flash monitor`。
3. 当烧录已经进入写 Flash 阶段后，把 `BOOT` 从下载模式接法切换到 `3.3V`。
4. 保持 `BOOT` 接在 `3.3V`，等待烧录完成。
5. 烧录工具自动 `hard reset` 后，芯片会按新的 `BOOT` 电平重新采样。
6. 这时它会从 `SPI_FAST_FLASH_BOOT` 进入应用，`monitor` 可以直接接到应用日志。

## 为什么这套步骤有效

烧录和正常启动，实际上是复位后两种不同的启动路径：

- 烧录前，需要让芯片进入 ROM 下载模式，这样 `esptool` 才能接管写 Flash。
- 烧录完成后，需要让芯片在下一次复位时改为“从 Flash 启动应用”。

关键点在于：芯片只会在复位瞬间采样 `BOOT` 状态。

这就意味着：

- 在“已经进入下载模式并开始烧录”之后，`BOOT` 线可以被改接。
- 真正决定“烧录后进应用还是回下载模式”的，是烧录结束时那次自动复位之前的 `BOOT` 电平。

从现象看，这块板子的 `BOOT` 脚如果悬空或保持当前默认接法，复位后很容易再次进入下载模式。把它切到 `3.3V`，就能稳定进入 `SPI_FAST_FLASH_BOOT`。

## 推荐命令

```bash
cd firmware-c5
idf.py set-target esp32c5
idf.py build flash monitor
```

如果你使用的是当前仓库默认工作流，也可以在加载 IDF 环境后直接执行：

```bash
python "$IDF_PATH/tools/idf.py" -p /dev/<your-c5-port> flash monitor
```

## 如何快速判断现在处于哪种状态

如果日志里出现下面这段，说明还在下载模式：

```text
boot:0x28 (DOWNLOAD(UART0/USB))
waiting for download
```

如果日志里出现下面这段，说明已经进入正常启动路径：

```text
boot:0x38 (SPI_FAST_FLASH_BOOT)
```

## 建议固化成操作习惯

- 烧录前先确认 `BOOT` 能把芯片送进下载模式。
- 烧录过程中尽早把 `BOOT` 改接到 `3.3V`。
- 不要等烧录完成、看到 `waiting for download` 以后才处理，这会多一次手动复位。
- 如果换了下载器、线序或供电方式，优先先看启动日志中的 `boot:0x..`，再判断是软件问题还是启动模式问题。
