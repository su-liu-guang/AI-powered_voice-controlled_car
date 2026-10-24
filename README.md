# AI-powered_voice-controlled_car

智能语音小车项目，包含 ESP32-S3 与 STM32F103 两端源码和可烧录固件。

| 目录 | 用途 |
| --- | --- |
| [linx-esp32](linx-esp32/) | 麦克风上传、平台激活、AI JSON 严格校验、连续监听及 UART 动作下发 |
| [stm32_motion](stm32_motion/) | USART1 接收 AA55 状态帧、TIM4 PWM 与差速运动控制 |
| [发布包](releases/2026-10-24-realtime/README.md) | 固件、烧录地址、接线、验证状态及 SHA-256 校验清单 |

## 当前发布：2026-10-24-realtime

完整 JSON 校验通过立即执行，同轮只执行一次；自动小车控制采用平台 realtime 持续收音，恢复监听不依赖 tts.stop。旧结束事件按轮次顺序处理，不清空下一句的 JSON 半包。

当前编译配置 UART1 TX=GPIO17、RX=GPIO21，分别连接 STM32 PA10、PA9，共地。麦克风 BCLK=GPIO5、WS=GPIO4、SD=GPIO6，左声道。实际接线必须与配置一致，具体见发布说明。

ESP32 的 24 项电脑端测试全部通过，两个工程构建成功，STM32 ARM 模拟逻辑测试通过。新连续监听版本仍需实机验证平台响应及前进后立即停止的实际延迟。

## 编译与使用

分别在对应子目录使用 PlatformIO 构建；固件与下载地址见发布包说明。

- [小车控制与连续监听](linx-esp32/docs/car-integration.md)
- [配网与激活码](linx-esp32/docs/wifi-activation.md)
- [STM32 接线与运动协议](stm32_motion/README.md)
- [文件校验清单](releases/2026-10-24-realtime/SHA256SUMS.txt)

## 来源与许可

源码来自本地两个独立工程。每个源文件快照的 SHA-256、源仓库基线提交和发布元数据见发布目录 manifest.json 与 source-snapshots.json。原有版权与许可保留，ESP32 原工程见 [LICENSE](linx-esp32/LICENSE)，STM32 库版权见对应源码。
