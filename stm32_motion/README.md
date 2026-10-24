# STM32 小车移动控制

独立的 PlatformIO C 工程，参考同级 `stm32_car` 的 STM32F103C8 电机驱动。
只处理 ESP32 发来的移动状态，没有循迹、避障、按键启动、阻塞延时或运动时长计时。

## 接线

| ESP32-S3 | STM32F103 | 用途 |
| --- | --- | --- |
| GPIO17 / UART1 TX | PA10 / USART1 RX | ESP32 发送指令 |
| GPIO21 / UART1 RX | PA9 / USART1 TX | 预留回传通路 |
| GND | GND | 共地 |

串口为 115200、8N1、3.3V 电平。TX/RX 交叉连接。PA9 已启用发送功能，但回传协议尚未定义，固件不主动发送日志或 ACK。
ESP32 侧需要在它自己的工程配置中启用 GPIO17/GPIO21；本项目没有修改 ESP32 工程。

原板 CH340 也连接 PA9/PA10。串口下载和板间通信共用这些线路，需按实际电路隔离发送端，避免 CH340 TX 与 ESP32 TX 同时驱动 PA10。
下载时先断开 ESP32 对 PA10 的驱动；运行时确认 CH340 TX 已隔离。未验证本板能否仅靠软件关闭电脑串口实现电气隔离。

## 电机输出

| 引脚 | TIM4 通道 | 沿用原工程的含义 |
| --- | --- | --- |
| PB6 | CH1 | 左侧前进 |
| PB7 | CH2 | 左侧后退 |
| PB8 | CH3 | 右侧前进 |
| PB9 | CH4 | 右侧后退 |

PWM 的 ARR=99、PSC=35，72MHz 定时器时钟下为 20kHz。沿用原工程的 8MHz 外部晶振和 72MHz 系统时钟配置，晶振、驱动板和电机实际方向仍需实物核对。
`include/car_config.h` 的 `CAR_SPEED_PERCENT` 暂沿用参考程序的 70，占空比不等于实际速度，需实测校准。

上电初始化四路为零。有效指令更新后由硬件 PWM 持续输出，直到下一条有效指令；`0x00` 停止。没有接收超时自动停车，因此断线后会保持上一次动作。

## 通信协议

固定四字节：`AA 55 STATE CRC8`。CRC 覆盖前三字节，poly=`07`、init=`00`、refin/refout=false、xorout=`00`，与现有 ESP32 实现一致。

| 状态位 | 含义 |
| --- | --- |
| bit0 / 01 | 前进 |
| bit1 / 02 | 后退 |
| bit2 / 04 | 左转 |
| bit3 / 08 | 右转 |
| bit4 / 10 | ESP32 本地发射 LED；STM32 忽略 |
| bit5～7 | 保留，必须为零 |

已知测试帧：

- 停止：`AA 55 00 82`
- 前进、左转、LED：`AA 55 15 E9`
- 全部开启：`AA 55 1F DF`

CRC 错误、保留位非零或不完整帧不更新电机状态，不等同于有效停止指令。
接收缓冲溢出或硬件接收错误时丢弃积压数据并重置解析器，等待后续完整帧；不会把错误帧拼成动作。
串口中断仅收数据，主循环解析并驱动电机，不在中断中延时。

左转、右转均表示前进中的差速转弯，两侧都向前输出，内侧降低占空比。
`CAR_TURN_INNER_PERCENT` 暂设为 50，表示内侧占空比为外侧的 50%，是可修改的初始调试值，并非用户指定或实测的速度比例。
电机能否在较低占空比下持续转动需要实测，不能仅由非零 PWM 保证。

以外侧 70%、内侧 35% 为例：

| 指令 | 左侧 PWM | 右侧 PWM |
| --- | --- | --- |
| 前进 | +70% | +70% |
| 后退 | -70% | -70% |
| 左转 / 前进+左转 | +35% | +70% |
| 右转 / 前进+右转 | +70% | +35% |
| 后退+左转 | -70% | -35% |
| 后退+右转 | -35% | -70% |
| 停止 | 0 | 0 |

倒车组合交换内外侧，以车头朝向定义左/右转；前后同时开启或左右同时开启时停止。
占空比精度为 1%，因此外侧仅为 1% 时，内侧保留 1% 输出，无法继续细分。常规占空比下内侧小于外侧。

## 工程结构

```text
platformio.ini        构建、芯片和串口下载配置
include/              配置和公共接口
src/main.c            接收、解析、更新电机的主循环
src/car_uart.c        USART1 中断及接收队列
src/car_protocol.c    CRC8 和流式帧解析
src/car_motion.c      状态位转为左右侧占空比
src/motor.c           TIM4 PWM 和无延时运动函数
src/startup_*.S       GCC 启动文件和中断向量
lib/STM32F1/          从参考工程复制的 CMSIS 与所需标准外设库
scripts/              链接脚本和构建设置
test/                 实际 C 逻辑的 ARM 模拟测试
docs/                 设计与实施记录
```

`Motor_Set(left, right)` 支持 -100～100，正数前进、负数后退、零停止。
同时提供 `Motor_Forward`、`Motor_Backward`、`Motor_Left`、`Motor_Right`、`Motor_Stop` 等无延时接口。
当前不提供原地旋转指令。

## 编译与测试

在此文件夹打开 PlatformIO 项目，执行 Build，或运行：

```powershell
& "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe" run
```

产物：`.pio/build/stm32_motion/firmware.bin`、`firmware.elf`、`firmware.map`。
工程自带所需库与启动代码，不依赖同级旧工程才能编译。

逻辑测试使用 PlatformIO 的 ARM 编译器和 Unicorn ARM 指令模拟器，在电脑上执行实际 C 代码，不连接硬件：

```powershell
& "$env:USERPROFILE/.platformio/penv/Scripts/python.exe" -m pip install --target .pio/test-tools -r test/requirements.txt
& "$env:USERPROFILE/.platformio/penv/Scripts/python.exe" test/run_logic_tests.py
```

覆盖 CRC 固定样例、全部 32 种状态、32 个单比特错误、保留位、噪声/半帧恢复和动作映射，以及通过模拟寄存器输入检查真实接收中断代码的队列循环、溢出和错误恢复。
不模拟真实串口电气时序、电机驱动电路或机械动作。

## 下载

未固定 COM 端口。确认当前端口和接线后再手动上传，例如：

```powershell
& "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe" run -t upload --upload-port COM6
```

串口下载速度 256000、DTR/RTS 时序沿用参考项目，依赖原板自动下载电路和 BOOT0 接线；不是所有 STM32 核心板的通用设置。
当前未执行下载、擦除、复位或实车测试。第一次测试应将车轮悬空，核对前进/后退、左右方向、停止帧和持续运动行为。

## 当前验证结果

2026-10-24：PlatformIO release 构建成功，Flash 3316 / 65536 字节，RAM（含堆栈预留）1696 / 20480 字节。
ARM 模拟逻辑测试通过。复制的 14 个 ST 库与 CMSIS 文件 SHA-256 与参考工程一致。
转向已按用户确认改为两侧均前进、内侧减速，测试包含全部状态和低占空比边界；实际轮速、转弯半径仍待上板验证。

## 来源

`lib/STM32F1` 来自同级参考工程的 `Library`、`Start`，保留 ST 原始版权和许可声明；只复制所需文件。
GCC 启动代码、链接脚本和构建脚本来自参考工程的 `platformio`。
业务代码为本项目重新编写；旧工程未修改。
