# SignGenerator

基于 STM32F103RCT6 的课程设计多波形信号发生器。项目计划使用片内 12 位 DAC、定时器和 DMA 输出正弦波、方波、三角波、锯齿波及白噪声，并通过 OLED 显示和四个外接按键完成控制。

当前状态：第一版固件已通过 Debug/Release 交叉编译，并完成首轮硬件实测——PA4 的 1 kHz 输出正常，ST-Link/OpenOCD 烧录与校验可用，白噪声已在示波器上确认（噪声带宽随采样率变化）。固件的输入是四个外接按键，板载两键保留接线但固件不再使用；台架测试期间串口单键控制为打开状态，见[当前固件操作](#当前固件操作)。方波、三角波、锯齿波的输出质量，以及四个外接按键和 OLED 显示仍待逐项实测。

## 功能目标

- 必做：四种基本波形、波形切换、频率调节、当前状态显示；
- 测试：计算频率上下限和最小步进，与示波器实测值比较；
- 扩展：白噪声（已完成）、固定参数 AM/FM、自动扫频；
- 目标频率范围：四种基本波形暂定 1 Hz～10 kHz。

完整选型和理论计算见[方案调研](./多波形信号发生器方案调研.md)，第一版代码的来源取舍和排查索引见[开源项目参考](./docs/开源项目参考.md)。原始任务书只保留在本地，不纳入公开仓库。

## 硬件与采购记录

| 模块 | 选型 | 状态/说明 |
|---|---|---|
| 单片机最小系统板 | STM32F103RCT6 | [天猫购买页（商品 942093832769，SKU 5839119920269）](https://detail.tmall.com/item.htm?id=942093832769&skuId=5839119920269)；[双 Type-C 核心板原理图](./docs/双TypeCF103RCT6原理图.pdf) |
| 显示 | SSD1306 0.96 英寸 128×64 I²C OLED | 3.3 V 供电，通常地址为 `0x3C` |
| 输入 | 四个外接常开按键 | 固件只使用外接四键；板载 K3/K4 保留接线但不参与控制 |
| 模拟输出 | PA4/DAC_OUT1、1 kΩ串联电阻、BNC/SMA 转接 | 首轮直接连接高阻示波器，不加运放 |
| 可选模拟输入 | B10K 线性电位器 | 接 PA1/ADC1_IN1，避开板载 PA0/WK_UP 按键 |
| 下载调试 | ST-Link V2 | 使用 SWDIO、SWCLK、GND、VTref 和 NRST |

购买链接记录于 2026-09-05；电商页面可能修改标题或 SKU，实际器件以订单和到货丝印为准。

## 预定引脚

| 功能 | MCU 引脚 | CubeMX 外设 |
|---|---|---|
| 板载 LED2 | PA8 | GPIO Output，低电平点亮 |
| 波形输出 | PA4 | DAC_OUT1 |
| OLED SCL/SDA | PB6/PB7 | I2C1 |
| 板载 K3/K4（固件不使用） | PC8/PC9 | GPIO Input，Pull-up |
| 启停/切换波形 | PB12/PB13 | GPIO Input，Pull-up |
| 频率减/频率加 | PB14/PB15 | GPIO Input，Pull-up |
| 可选电位器 | PA1 | ADC1_IN1 |
| 板载 USB 转串口 | PA9/PA10 | USART1_TX/RX，经 CH340K 引出 |
| 下载调试 | PA13/PA14 | SYS Serial Wire |

## 当前固件操作

上电后默认以 1 kHz 输出正弦波。外接 PB12 控制启停，PB13 按正弦波、方波、三角波、锯齿波、白噪声循环切换，PB14/PB15 调节频率或采样率，长按后自动连发。板载 K3（PC8）/K4（PC9）仍被配置为上拉输入，但固件不再扫描这两个按键。四种周期波形的调节步长随频段在 1、10、100 和 1000 Hz 间变化，范围限制为 1 Hz～10 kHz。

白噪声模式下「频率」的语义变成**采样率**：按键改为倍频/减半，范围 20 kS/s～500 kS/s（对应带宽 10 kHz～250 kHz），默认 100 kS/s。噪声在每个半缓冲与整缓冲中断里用 xorshift32 重新生成，不是循环播放的周期伪噪声；因此把采样率降低，噪声带宽会同步变窄，可以直接用示波器验证。

PA8 板载 LED 作为独立的 1 Hz 心跳灯，以 50% 占空比持续闪烁，不再表示波形启停状态；主循环卡死时闪烁会停止。波形运行状态仍显示在 OLED 并通过 USART1 输出。

OLED 显示波形、设定频率（噪声模式下为采样率）、定时器实际值和运行状态；没有连接 OLED 时不会阻止波形输出。USART1 以 115200 8N1 在每次状态变化后输出同样的信息，可以只用来观察状态：周期波形是 `wave=SINE set=1000Hz actual=1000.889Hz points=256 state=RUN`，白噪声是 `wave=NOISE rate=100000SPS actual=100000SPS points=256 state=RUN`。

串口单键控制（q 启停、w 换波、e/r 加减频或采样率）的代码在 `app_serial.c` 中，由该文件顶部的 `APP_SERIAL_KEY_CONTROL` 开关控制。**台架测试期间为 1（打开）**：在外接四键还没接线时，它是唯一的输入手段；接好外接四键后应置 0，回到纯按键控制。关闭状态下接收路径仍会被排空，只是不再产生按键事件。配合 [`tools/serial_console.py`](./tools/serial_console.py) 使用（需要 pyserial）。

## 快速开始

需要 CMake 3.22 或更高版本、GNU Arm Embedded Toolchain、Make、OpenOCD 和 ST-Link。克隆后运行：

```sh
git clone https://github.com/Kunzite1/SignGenerator.git
cd SignGenerator
```

```sh
./stm32proj/32build.sh
```

脚本默认构建 Debug，并在 `stm32proj/build/Debug/` 生成 ELF、HEX 和 BIN；发布构建或全新重建可使用 `./stm32proj/32build.sh Release`、`./stm32proj/32build.sh --clean`。

连接目标板的 SWDIO、SWCLK、GND、VTref 和 NRST 后烧录：

```sh
./stm32proj/32flash.sh
```

烧录脚本会先重新构建，再通过 OpenOCD 完成烧录、校验和复位。仅检查命令而不访问硬件可运行 `./stm32proj/32flash.sh --no-build --dry-run`。

硬件配置的唯一源文件是 [`stm32proj/stm32proj.ioc`](./stm32proj/stm32proj.ioc)。修改引脚或外设后用 STM32CubeMX 重新生成，再从全新的构建目录验证。

## 项目结构

```text
.
├── docs/                        # 原理图、参考项目和后续测试记录
├── tools/                       # 主机端脚本（serial_console.py 串口控制台）
└── stm32proj/
    ├── 32build.sh               # Debug/Release 构建及产物转换
    ├── 32flash.sh               # ST-Link/OpenOCD 烧录
    ├── Core/Inc、Core/Src       # 生成代码和手写功能模块
    ├── Drivers/                 # STM32 HAL 与 CMSIS
    ├── cmake/                   # 交叉编译工具链
    └── stm32proj.ioc            # CubeMX 硬件配置源文件
```

CubeMX 再生成时，仅在 `USER CODE BEGIN/END` 块内直接修改生成文件；波形表、界面状态机等业务代码应放到独立模块，并从顶层 CMake 目标显式引入。

## 到板后验收

1. 从全新 `stm32proj/build/Debug` 目录编译并生成 ELF/BIN/HEX；
2. ST-Link 能识别 STM32F103RC，完成烧录、校验和复位；
3. PA4 能输出约 1 kHz 正弦波，并能切换其余四种波形；
4. PB12/PB13 验证启停与波形切换，PB14/PB15 验证频率调节（含长按连发）；PC8/PC9 不再参与控制；
5. PA8 LED 以 1 Hz 闪烁，OLED 和频率分档逐项验证并记录示波器结果；
6. 白噪声在慢时基下呈现模糊带、FFT 频谱平到采样率的一半，改变采样率时带宽随之变化。
