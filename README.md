# STM32-loT-TempHumi

基于 STM32F103C8 + ESP8266 + DHT11 的物联网温湿度监控系统，通过 MQTT 协议对接中国移动 OneNET 云平台，支持数据采集、远程监控、阈值报警和离线数据缓存补发。

## 硬件方案

| 模块 | 型号 | 说明 |
|------|------|------|
| MCU | STM32F103C8 | Cortex-M3，72MHz |
| 传感器 | DHT11 | 数字温湿度传感器 |
| Wi-Fi | ESP8266 | AT 指令模式，UART2 通信 |
| 显示 | OLED | 0.96 寸 I2C OLED |
| 报警 | 蜂鸣器 | 阈值超限声光报警 |
| 输入 | 按键 | 用户输入 |
| 指示 | LED | 状态指示 |

## 功能特性

- **温湿度采集**：每秒读取 DHT11 数据，含校验和容错
- **OLED 实时显示**：温度、湿度、WiFi 状态、MQTT 状态一目了然
- **MQTT 数据上传**：传感器数据实时推送至 OneNET 云平台
- **离线缓存补发**：断网时数据暂存环形缓冲区，恢复后自动补传
- **远程阈值下发**：可通过云平台下发温度/湿度报警阈值，实时生效
- **蜂鸣器报警**：温度或湿度超限自动触发声光报警
- **掉线自动重连**：WiFi/MQTT 断线检测、去抖容错、自动恢复
- **AT 桥接调试模式**：`UART_BRIDGE_MODE` 宏一键切换，串口直通 ESP8266 调试

## 引脚连接

| 外设 | GPIO |
|------|------|
| DHT11 | PA1 |
| OLED | I2C (PB6 SCL, PB7 SDA) |
| ESP8266 UART | PA2 (TX), PA3 (RX) |
| 蜂鸣器 | PB0 |
| LED1 | PC13 |
| 按键 | PA0 |

## 软件架构

```
├── Start/          # STM32 启动文件、系统时钟、CMSIS
├── Library/        # STM32 标准外设库
├── System/         # 系统延时
├── Hardware/       # 硬件驱动层
│   ├── DHT11       # 温湿度传感器驱动
│   ├── ESP8266     # Wi-Fi 模块 AT 驱动
│   ├── MQTT        # MQTT 协议 + OneNET 对接
│   ├── OLED        # OLED 显示驱动
│   ├── USART       # 串口驱动 (USART1 调试 / USART2 ESP8266)
│   ├── RingBuffer  # 离线数据环形缓冲区
│   ├── Buzzer      # 蜂鸣器驱动
│   ├── LED         # LED 驱动
│   └── Key         # 按键驱动
└── User/           # main.c、中断服务、工程配置
```

## 云平台配置

本项目对接 **中国移动 OneNET** 物联网平台（MQTT 旧版协议），需在 `Hardware/mqtt.c` 中配置：

```c
#define PRODUCT_ID   "your_product_id"
#define DEVICE_NAME  "your_device_name"
#define DEVICE_KEY   "your_device_key"
#define MQTT_HOST    "mqtts.heclouds.com"
#define MQTT_PORT    1883
```

WiFi 配置在 `Hardware/esp8266.c`：

```c
#define WIFI_SSID  "your_ssid"
#define WIFI_PWD   "your_password"
```

## 开发环境

- **IDE**: Keil MDK (ARM Compiler 5/6)
- **MCU 型号**: STM32F103C8
- **标准库**: STM32F10x Standard Peripheral Library

### 编译与烧录

1. 使用 Keil MDK 打开 `Project.uvprojx`
2. 选择目标 `STM32F103C8`
3. 编译后通过 ST-Link / J-Link 烧录

### AT 桥接调试模式

将 `main.c` 中 `UART_BRIDGE_MODE` 设为 `1`，编译烧录后，串口 1 与 ESP8266 直通，可直接发送 AT 指令调试模块。

## 数据流

```
DHT11 ──→ MCU ──→ OLED 显示
              └──→ RingBuffer ──→ ESP8266 ──→ OneNET (MQTT)
                                              │
                      阈值下发 ←──────────────┘
                      蜂鸣器 ←── 阈值比较
```
