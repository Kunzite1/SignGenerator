# SignGenerator

基于 STM32F103RCT6 的课程设计多波形信号发生器。项目计划使用片内 12 位 DAC、定时器和 DMA 输出正弦波、方波、三角波及锯齿波，并通过 OLED 和旋转编码器完成显示与控制。

当前状态：方案和器件路线已确定，Linux 开发环境已核对；`stm32proj/` 中已经生成 STM32CubeMX+CMake 工程骨架，MCU 型号、SWD、外部晶振引脚和 PA8 板载 LED 已完成首轮检查，其余外设待配置。

## 功能目标

- 必做：四种基本波形、波形切换、频率调节、当前状态显示；
- 测试：计算频率上下限和最小步进，与示波器实测值比较；
- 扩展：白噪声、固定参数 AM/FM、自动扫频；
- 目标频率范围：四种基本波形暂定 1 Hz～10 kHz。

完整选型、参考项目和理论计算见[方案调研](./多波形信号发生器方案调研.md)，原始要求见[课程设计任务书](./二级项目任务书-报告模板-注意事项-模电实验室新位置.doc)。

## 硬件与采购记录

| 模块 | 选型 | 状态/说明 |
|---|---|---|
| 单片机最小系统板 | STM32F103RCT6 | [天猫购买页（商品 942093832769，SKU 5839119920269）](https://detail.tmall.com/item.htm?id=942093832769&skuId=5839119920269)；[双 Type-C 核心板原理图](./docs/双TypeCF103RCT6原理图.pdf) |
| 显示 | SSD1306 0.96 英寸 128×64 I²C OLED | 3.3 V 供电，通常地址为 `0x3C` |
| 输入 | EC11 旋转编码器模块、独立启停键 | 编码器负责调频和菜单选择 |
| 模拟输出 | PA4/DAC_OUT1、1 kΩ串联电阻、BNC/SMA 转接 | 首轮直接连接高阻示波器，不加运放 |
| 可选模拟输入 | B10K 线性电位器 | 接 PA0/ADC1_IN0，用于调幅或 ADC 演示 |
| 下载调试 | ST-Link V2 | 使用 SWDIO、SWCLK、GND、VTref 和 NRST |

购买链接记录于 2026-09-05；电商页面可能修改标题或 SKU，实际器件以订单和到货丝印为准。

## 预定引脚

| 功能 | MCU 引脚 | CubeMX 外设 |
|---|---|---|
| 波形输出 | PA4 | DAC_OUT1 |
| OLED SCL/SDA | PB6/PB7 | I2C1 |
| EC11 A/B | PA6/PA7 | TIM3_CH1/TIM3_CH2 Encoder Mode |
| EC11 按键/启停键 | PB0/PB1 | GPIO Input，Pull-up |
| 可选电位器 | PA0 | ADC1_IN0 |
| 可选串口 | PA9/PA10 | USART1_TX/RX |
| 下载调试 | PA13/PA14 | SYS Serial Wire |

## STM32CubeMX 首次配置

本机已具备 STM32CubeMX 6.18.1、STM32CubeF1 1.8.7、CMake 3.22、GNU Arm GCC、OpenOCD 和 GDB。CubeMX 自带 Java，无需修改系统 `JAVA_HOME`。可从应用菜单启动，也可运行：

```sh
~/STM32CubeMX/STM32CubeMX
```

CubeMX 当前配置步骤：

1. 选择 **New Project → MCU/MPU Selector**，搜索并精确选择 `STM32F103RCT6`，不要选成 `STM32F103C8T6`。
2. 在 **System Core → SYS** 中将 Debug 设为 **Serial Wire**；在 **RCC** 中根据实物配置 HSE。常见 8 MHz 无源晶振板选择 **Crystal/Ceramic Resonator**，到货后应先看原理图或晶振标识。
3. 在 **Clock Configuration** 中设置 HSE 8 MHz、PLL ×9、SYSCLK 72 MHz、APB1 36 MHz、APB2 72 MHz。APB1 分频为 2 时，TIM6 定时器时钟仍为 72 MHz。
4. 开启 **DAC Channel 1**，输出引脚为 PA4；Trigger 选 `TIM6 TRGO`，Output Buffer 开启。
5. 开启 **TIM6**，Clock Source 选 Internal Clock，Trigger Output 选 **Update Event**。初始 PSC 可设 0、ARR 设 280，之后由程序按目标频率重算。
6. 在 DAC 的 **DMA Settings** 中添加 Channel 1 请求：Mode=`Circular`、Memory Increment=`Enable`、Peripheral/Memory Data Width=`Half Word`，优先级可设 High。
7. 开启 I2C1（PB6/PB7）和 TIM3 Encoder Mode（PA6/PA7）；PB0、PB1 配成上拉输入。USART1 和 ADC1_IN0 可等基本输出成功后再启用。
8. 当前工程名和目录为 `stm32proj/`，语言 C、Toolchain/IDE=`CMake`。继续使用现有 `.ioc`，复制所需 HAL/CMSIS 文件到工程，并保持“每个外设生成独立 `.c/.h`”与保留 User Code。

生成后首先核对：设备宏是 `STM32F103xE`，启动文件是 `startup_stm32f103xe.s`，链接脚本描述 256 KiB Flash/48 KiB RAM，编译参数使用 Cortex-M3 且没有 M4F 硬浮点选项。应提交 `.ioc`、`Core/`、`Drivers/`、`cmake/`、启动文件和链接脚本。

## 构建与烧录

本机暂未安装 Ninja，因此首个工程使用 Unix Makefiles。CubeMX 生成代码后，从仓库根目录执行：

```sh
cmake -S stm32proj -B build/Debug -G "Unix Makefiles" \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/stm32proj/cmake/gcc-arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Debug --parallel
```

实板和 ST-Link 到货后，先确认接线及 ELF 名称，再烧录：

```sh
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "adapter speed 1000" \
  -c "program {build/Debug/stm32proj.elf} verify reset exit"
```

## 项目结构

```text
.
├── README.md                    # 项目入口和当前配置
├── stm32proj/                   # CubeMX/CMake 固件工程及 .ioc
├── include/                     # 后续手写模块的公共头文件
├── tests/                       # 可在主机运行的算法测试
├── hardware/                    # 接线图、原理图和转接板
├── docs/                        # 核心板原理图、报告与测试记录
└── assets/                      # 照片和示波器截图
```

CubeMX 再生成时，仅在 `USER CODE BEGIN/END` 块内直接修改生成文件；波形表、界面状态机等业务代码应放到独立模块，并从顶层 CMake 目标显式引入。

## 首轮验收

1. 空工程可以从全新 `build/Debug` 目录编译并生成 ELF/BIN/HEX；
2. ST-Link 能识别 STM32F103RC，完成烧录、校验和复位；
3. PA4 首先输出约 1 kHz 正弦波，再加入其余三种波形；
4. OLED、EC11 和频率分档逐项接入，每完成一项即记录示波器结果。
