# 文档导航

这套文档服务两个目标。第一个目标是帮助第一次接触项目的人尽快把工程跑起来。第二个目标是帮助准备改代码的人看清 P4、C5、网络链路和控制台命令之间的关系。

适合谁看：
- 第一次接触本项目的固件开发者
- 需要定位 P4 / C5 代码入口的联调开发者
- 需要扩展桥接逻辑或排查 bring-up 问题的维护者

读完会得到什么：
- 知道项目为什么分成 P4 和 C5 两个固件
- 知道第一次编译、烧录、联调应该按什么顺序做
- 知道要看哪些源码文件来理解系统
- 知道图纸和文档分别放在哪里

## 建议阅读顺序

### 快速上手

1. [01 项目介绍](./01-project-intro.md)
2. [02 快速开始](./02-quick-start.md)
3. [03 开发工作流](./03-development-workflow.md)
4. [04 常见问题排查](./04-troubleshooting.md)

### 深入开发

1. [系统总览](./architecture/system-overview.md)
2. [P4 固件架构](./architecture/p4-firmware-architecture.md)
3. [C5 固件架构](./architecture/c5-firmware-architecture.md)
4. [P4 与 C5 联动](./architecture/p4-c5-integration.md)
5. [控制台命令](./interfaces/console-commands.md)
6. [数据流](./interfaces/data-flow.md)
7. [如何新增一个功能](./guides/adding-a-feature.md)
8. [P4 / C5 Main 解耦检查清单](./checktodolist.md)

## 文档分层

| 路径 | 作用 |
| --- | --- |
| `docs/0*.md` | 新人先读的入门文档 |
| `docs/architecture/` | 理解系统结构、模块边界和协作方式 |
| `docs/interfaces/` | 理解命令、输入输出和数据流 |
| `docs/guides/` | 面向开发动作的实操指南 |
| `docs/diagrams/src/` | PlantUML 源文件 |
| `docs/diagrams/rendered/` | 由 PlantUML 导出的 SVG 图 |

## 图纸索引

| 图名 | 说明 |
| --- | --- |
| [系统关系图](./diagrams/rendered/system-context.svg) | 展示 PC、P4、C5、飞控之间的关系 |
| [P4 启动流程图](./diagrams/rendered/startup-flow.svg) | 展示 `app_main` 启动后初始化顺序 |
| [网络到 UART 时序图](./diagrams/rendered/network-to-uart-sequence.svg) | 展示网络数据如何进入飞控链路 |
| [UART 到网络时序图](./diagrams/rendered/uart-to-network-sequence.svg) | 展示飞控回传如何离开设备 |
| [P4 模块依赖图](./diagrams/rendered/p4-module-dependency.svg) | 展示 P4 主要模块之间的依赖关系 |
| [C5 分层图](./diagrams/rendered/c5-hosted-layering.svg) | 展示 `esp-hosted` 定制层与传输层的关系 |
| [Bring-up 排查流程图](./diagrams/rendered/bringup-checklist-flow.svg) | 展示首次联调时推荐的检查顺序 |

## 常见入口

- 仓库入口说明见 [README.md](../README.md)
- 中文仓库说明见 [README_cn.md](../README_cn.md)
- P4 子工程说明见 [firmware-p4/README.md](../firmware-p4/README.md)
- C5 子工程说明见 [firmware-c5/README.md](../firmware-c5/README.md)
- Main 解耦执行清单见 [checktodolist.md](./checktodolist.md)

## 如何维护这套文档

- 先改正文，再改图。不要让图先引入正文里没有解释过的术语。
- 新增一张 PlantUML 图时，把源文件放到 `docs/diagrams/src/`，把导出的 SVG 放到 `docs/diagrams/rendered/`。
- 统一使用 `docs/diagrams/render.sh` 生成 SVG。默认透明背景，传入 `white` 参数可导出白色背景版本。
- 如果只是补充某个模块细节，优先改对应的 `architecture/` 或 `interfaces/` 文档，不要把所有内容堆回顶层 README。
