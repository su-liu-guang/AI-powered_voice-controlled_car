# AI-powered_voice-controlled_car

智能语音小车项目。不同固件工程分别保存在独立目录中。

## 工程目录

| 目录 | 说明 |
| --- | --- |
| [linx-esp32](linx-esp32/) | 基于 linx-esp32 的 ESP32 固件，包含小车动作 JSON 校验、串口文字 AI 测试与二进制帧转换 |

当前先上传 `linx-esp32/`，另一套工程完成后将以同级目录加入。

## linx-esp32 当前测试方式

电脑串口助手发送一行 UTF-8 文字，设备上传到 AI 平台，汇总完整回复后校验格式：

- 格式正确：显示状态字节及 `AA 55 STATE CRC8` 帧的二进制和十六进制表示。
- 格式错误：只报告错误，不转换、不更新动作状态、不执行硬件指令。
- 当前启用串口测试模式，暂停麦克风上传与小车动作输出。

在 `linx-esp32/` 目录使用 PlatformIO 构建：

```powershell
pio run -e esp32s3
```

电脑端测试：

```powershell
pio test -d test-native -e car_tests -e serial_tests
```

当前软件版本已通过 17 组电脑端测试及固件编译；实际接线、平台交互和硬件动作仍需按设备情况验证。

## 使用说明

- [串口文字 AI 测试](linx-esp32/docs/serial-text-chat.md)
- [小车业务与动作协议](linx-esp32/docs/car-integration.md)
- [PlatformIO 构建](linx-esp32/docs/platformio.md)
- [配网与激活码](linx-esp32/docs/wifi-activation.md)

## 来源与许可

`linx-esp32/` 本次导入自本地 `feat/car-integration` 分支的提交 `4cbd058`。
原工程来源、说明与许可保留在子目录内，见 [原工程 README](linx-esp32/README.md) 和 [LICENSE](linx-esp32/LICENSE)。
