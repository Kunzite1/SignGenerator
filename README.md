# SignGenerator

基于 STM32F103RCT6 的课程设计多波形信号发生器。项目计划使用片内 12 位 DAC、定时器和 DMA 输出正弦波、方波、三角波及锯齿波，并通过 OLED 和四个外接按键完成显示与控制。

当前状态：第一版固件已经完成并通过 Debug/Release 交叉编译，包含四波形 DAC DMA 输出、四键控制、OLED 状态页和串口日志；开发板尚未到货，输出质量和烧录流程等待实测。

## 功能目标

- 必做：四种基本波形、波形切换、频率调节、当前状态显示；
- 测试：计算频率上下限和最小步进，与示波器实测值比较；
- 扩展：白噪声、固定参数 AM/FM、自动扫频；
- 目标频率范围：四种基本波形暂定 1 Hz～10 kHz。

完整选型和理论计算见[方案调研](./多波形信号发生器方案调研.md)，第一版代码的来源取舍和排查索引见[开源项目参考](./docs/开源项目参考.md)。原始任务书只保留在本地，不纳入公开仓库。

## 硬件与采购记录

| 模块 | 选型 | 状态/说明 |
|---|---|---|
| 单片机最小系统板 | STM32F103RCT6 | [天猫购买页（商品 942093832769，SKU 5839119920269）](https://detail.tmall.com/item.htm?id=942093832769&skuId=5839119920269)；[双 Type-C 核心板原理图](./docs/双TypeCF103RCT6原理图.pdf) |
| 显示 | SSD1306 0.96 英寸 128×64 I²C OLED | 3.3 V 供电，通常地址为 `0x3C` |
| 输入 | 四个常开按键 | 分别负责启停、切换波形、频率减和频率加 |
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
| 启停/切换波形 | PB12/PB13 | GPIO Input，Pull-up |
| 频率减/频率加 | PB14/PB15 | GPIO Input，Pull-up |
| 可选电位器 | PA1 | ADC1_IN1 |
| 板载 USB 转串口 | PA9/PA10 | USART1_TX/RX，经 CH340K 引出 |
| 下载调试 | PA13/PA14 | SYS Serial Wire |

## 当前固件操作

上电后默认以 1 kHz 输出正弦波，PA8 LED 低电平点亮表示正在运行。PB12 控制启停，PB13 按正弦波、方波、三角波、锯齿波循环切换，PB14/PB15 降低或提高频率；频率键长按后自动连发。调节步长随频段在 1、10、100 和 1000 Hz 间变化，范围限制为 1 Hz～10 kHz。

OLED 显示波形、设定频率、定时器实际频率和运行状态；没有连接 OLED 时不会阻止波形输出。USART1 以 115200 8N1 在每次状态变化后输出同样的信息。

## 快速开始

需要 CMake 3.22 或更高版本、GNU Arm Embedded Toolchain、Make、OpenOCD 和 ST-Link。克隆后运行：

```sh
git clone https://github.com/Kunzite1/SignGenerator.git
cd SignGenerator
```

```sh
./build.sh
```

脚本默认构建 Debug，并在 `build/Debug/` 生成 ELF、HEX 和 BIN；发布构建或全新重建可使用 `./build.sh Release`、`./build.sh --clean`。

连接目标板的 SWDIO、SWCLK、GND、VTref 和 NRST 后烧录：

```sh
./flash.sh
```

烧录脚本会先重新构建，再通过 OpenOCD 完成烧录、校验和复位。仅检查命令而不访问硬件可运行 `./flash.sh --no-build --dry-run`。

硬件配置的唯一源文件是 [`stm32proj/stm32proj.ioc`](./stm32proj/stm32proj.ioc)。修改引脚或外设后用 STM32CubeMX 重新生成，再从全新的构建目录验证。

## 项目结构

```text
.
├── build.sh                     # Debug/Release 构建及产物转换
├── flash.sh                     # ST-Link/OpenOCD 烧录
├── docs/                        # 原理图、参考项目和后续测试记录
└── stm32proj/
    ├── Core/Inc、Core/Src       # 生成代码和手写功能模块
    ├── Drivers/                 # STM32 HAL 与 CMSIS
    ├── cmake/                   # 交叉编译工具链
    └── stm32proj.ioc            # CubeMX 硬件配置源文件
```

CubeMX 再生成时，仅在 `USER CODE BEGIN/END` 块内直接修改生成文件；波形表、界面状态机等业务代码应放到独立模块，并从顶层 CMake 目标显式引入。

## 到板后验收

1. 从全新 `build/Debug` 目录编译并生成 ELF/BIN/HEX；
2. ST-Link 能识别 STM32F103RC，完成烧录、校验和复位；
3. PA4 能输出约 1 kHz 正弦波，并能切换其余三种波形；
4. OLED、四个按键和频率分档逐项接入，每完成一项即记录示波器结果。
