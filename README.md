# PowerHub

> 桌面电源监测站 — 5 通道可编程直流电源，内置 UPS，带 LCD 仪表盘和 WiFi 远程控制。

> ⚠️ **项目处于早期开发阶段**，功能持续迭代中，接口和架构可能变化。

## 硬件特性

| 特性 | 规格 |
|---|---|
| 主控 | ESP32-S3 (双核 Xtensa LX7 @ 240MHz) |
| 监测通道 | 5 路独立 DC 输出 |
| 采样芯片 | TI INA226 × 5（I2C, 16 位 ADC） |
| 采样电阻 | 3mΩ, 最大 19A 每通道 |
| 输出控制 | TI TCA9535 16 位 I/O 扩展器（P00–P04 独立开关） |
| UPS 核心 | MPS MP4201 双向升降压控制器（PMBus） |
| DAC 配置 | MCP4725 × 2（12 位, 控制 MP4201 频率和模式） |
| 显示屏 | ST7701S RGB LCD (376×960, RGB565, PSRAM 帧缓冲) |
| 按键 | 3 键（通道切换 / 输出开关 / 长按进入配置模式） |
| 无线 | WiFi 2.4GHz — 自动连接 + AP 配置门户 |

## 软件架构

```
main.cpp (app_main)
├── StatusKey         — GPIO 按键扫描 (20ms 轮询, 短按/长按 2s)
├── DeviceInit        — I2C 总线初始化 + 注册全部外设
│   └── I2CBusManager — 单例, I2C_NUM_0 (SCL=17, SDA=16, 100kHz)
│       ├── INA226×5  — 电压/电流/功率监测 (0x40/41/42/44/45)
│       ├── TCA9535   — 通道输出开关控制 (0x20)
│       ├── MP4201    — 双向升降压 PMBus 控制器 (0x3F)
│       └── MCP4725×2 — DAC 配置 MP4201 频率/模式 (0x60/0x61)
├── PowerMonitor      — 每 70ms 轮询 INA226, 发布 Event
├── ApWifi            — WiFi 自动连接 + AP 配置门户
│   ├── WifiManager   — STA/APSTA 模式, NVS 凭据存储 (最多 5 组)
│   └── WsServer      — HTTP 服务器 + WebSocket (/ws)
├── PerfMonitor       — CPU 使用率 + 内存监测
├── SceneManager      — LCD 仪表盘, 脏矩形渲染
│   ├── LcdRgb        — ST7701S 初始化 + RGB 面板 + DMA 刷新
│   └── LcdDriver     — 软件帧缓冲 (RGB565, PSRAM, 376×960)
└── Web UI            — 复古游戏机风格 AP 配置页面 (SPIFFS)
```

### 线程模型

| 任务 | 核心 | 优先级 | 职能 |
|---|---|---|---|
| UIManagerTask | 0 | 5 | LCD 脏矩形渲染 |
| MonitorListenerTask | 0 | 5 | 接收 PowerMonitor 事件 |
| MonitorTask | 1 | 2 | 70ms 周期轮询 INA226 |
| GetKeyTask | 1 | 2 | 20ms 周期按键扫描 |
| KeyMonitorTask | 1 | 5 | 按键事件 → 通道选择/切换 |
| AutoConnectTask | 1 | 5 | WiFi 扫描 + 自动连接 |
| PerfTask | 1 | 1 | CPU 使用率监测 |

### 通信模式

所有组件间通信基于 **FreeRTOS 队列发布/订阅模式**：

```
生产者: xQueueOverwrite(listener, &data_ptr)  // 覆盖最旧条目, 非阻塞
消费者: xQueueReceive(queue, &data_ptr, timeout)  // 阻塞等待
```

## 快速开始

### 依赖

- ESP-IDF v5.5.3
- ESP32-S3 开发板
- LVGL v9.5.0（已链接，当前 UI 使用自研渲染器）
- cJSON

### 构建

```bash
# 首次构建
idf.py set-target esp32s3
idf.py build

# 烧录 + 串口监视
idf.py -p /dev/ttyUSB0 flash monitor

# 清理重编
idf.py fullclean build
```

### 分区表

| 分区 | 大小 | 用途 |
|---|---|---|
| factory | 4MB | 固件 |
| html (SPIFFS) | 128KB | Web 配置页面 |
| nvs | 24KB | WiFi 凭据存储 |
| phy_init | 4KB | WiFi 校准数据 |

## 使用说明

### 本地控制

| 按键 | GPIO | 短按 | 长按 (2s) |
|---|---|---|---|
| KEY 1 | GPIO 6 | 下一个通道 | — |
| KEY 2 | GPIO 5 | 切换当前通道 开/关 | — |
| KEY 3 | GPIO 4 | 上一个通道 | KEY 1+KEY 3 同时长按 → 进入 AP 配置模式 |

选中通道由 LCD 顶部方框高亮指示。

### LCD 仪表盘

```
┌────────────────────────────┐
│  [CH1 ● ON] [CH2 ● OFF] ...│  ← 顶部状态栏, 方框 = 选中通道
│  ┌──────┐ ┌──────┐         │
│  │CH-1 >>│ │CH-2 >>│  ...  │  ← 5 列卡片
│  │       │ │       │         │
│  │12.34V │ │       │         │  ← 电压 (大字体)
│  │▆▆▆▆▆▆▆│ │       │         │  ← 分段进度条
│  │1.234A │ │OFFLINE│         │  ← 电流 / 离线
│  │150.0W │ │       │         │  ← 功率
│  └──────┘ └──────┘         │
└────────────────────────────┘
```

CRT 荧光绿主题，黑色背景，扫描线装饰效果。

### WiFi 配置

上电后自动尝试连接已保存的 WiFi（最多 5 轮扫描匹配）。连接失败后自动进入 AP 模式。

**手动进入 AP 模式：** 同时长按 KEY 1 + KEY 3 2 秒。

AP 网络：
- **SSID:** `DesktopPowerStation`
- **密码:** `12345678`
- **IP:** `192.168.100.1`

手机连接后访问 `http://192.168.100.1/`，打开复古游戏机风格配置页面：
- 扫描并选择 WiFi 网络
- 输入密码连接
- 凭据自动保存到 NVS

### UPS (开发中)

MP4201 双向升降压控制器支持：
- **充电模式:** VIN → 电池 (CC/CV)
- **放电模式:** 电池 → VOUT (备用电源)
- **MPPT:** 输入电压调节（太阳能 panel 输入）

当前驱动层已完成，应用层集成开发中。

## I2C 外设一览

| 地址 | 芯片 | 功能 | 备注 |
|---|---|---|---|
| 0x20 | TCA9535 | I/O 扩展 | A1/A2/A3 接地, P00–P04 控制 5 通道开关 |
| 0x3F | MP4201 | 双向升降压 | PMBus, 100V/25A |
| 0x40 | INA226 | CH1 功率监测 | 3mΩ, 19A |
| 0x41 | INA226 | CH2 功率监测 | 3mΩ, 19A |
| 0x42 | INA226 | CH5 功率监测 | 3mΩ, 19A |
| 0x44 | INA226 | CH3 功率监测 | 3mΩ, 19A |
| 0x45 | INA226 | CH4 功率监测 | 3mΩ, 19A |
| 0x60 | MCP4725 | MP4201 FREQ 控制 | A0=GND |
| 0x61 | MCP4725 | MP4201 MODE 控制 | A0=VDD |

## GPIO 引脚分配

### I2C 总线
| 引脚 | 功能 |
|---|---|
| GPIO 16 | SDA |
| GPIO 17 | SCL |

### LCD RGB565 数据 (16 位)
| 引脚 | 信号 | 引脚 | 信号 |
|---|---|---|---|
| GPIO 41 | B0 | GPIO 11 | R0 |
| GPIO 40 | B1 | GPIO 10 | R1 |
| GPIO 38 | B2 | GPIO 9 | R2 |
| GPIO 39 | B3 | GPIO 46 | R3 |
| GPIO 45 | B4 | GPIO 3 | R4 |
| GPIO 48 | G0 | | |
| GPIO 47 | G1 | | |
| GPIO 21 | G2 | | |
| GPIO 14 | G3 | | |
| GPIO 13 | G4 | | |
| GPIO 12 | G5 | | |

### LCD 控制
| 引脚 | 信号 | 引脚 | 信号 |
|---|---|---|---|
| GPIO 1 | SPI CS | GPIO 7 | 背光 |
| GPIO 2 | SPI SCL | GPIO 8 | HSYNC |
| GPIO 42 | SPI SDA | GPIO 18 | VSYNC |
| | | GPIO 19 | PCLK (20MHz) |
| | | GPIO 20 | DE |

### 按键
| 引脚 | 按键 | 功能 |
|---|---|---|
| GPIO 4 | KEY 3 | 上一个通道 |
| GPIO 5 | KEY 2 | 开关切换 |
| GPIO 6 | KEY 1 | 下一个通道 |

## 项目结构

```
PowerHub/
├── main/
│   └── main.cpp              # 入口, 初始化顺序
├── components/
│   ├── device/
│   │   ├── device_init.cpp    # 外设注册 (I2C 探测 + 初始化)
│   │   └── i2c_device/
│   │       ├── i2c_bus.cpp    # I2C 总线管理器 (单例)
│   │       ├── i2c_device.cpp # 设备基类 (Probe/Init/Write/Read)
│   │       ├── ina226/        # INA226 驱动 (电压/电流/功率)
│   │       ├── tca9535/       # TCA9535 驱动 (I/O 扩展)
│   │       ├── mp4201/        # MP4201 驱动 (PMBus 升降压)
│   │       └── mcp4725/       # MCP4725 驱动 (12 位 DAC)
│   ├── power_monitor/         # 功率监测任务 + 通道控制
│   ├── wifi_manager/          # WiFi 栈 (STA + AP + WebSocket)
│   ├── html/
│   │   └── apcfg.html         # Web 配置页面 (游戏机主题)
│   ├── lcd/
│   │   ├── lcd_rgb.cpp        # ST7701S 初始化序列
│   │   ├── lcd_driver.cpp     # 软件帧缓冲 + 绘图基元
│   │   ├── scene_manager.cpp  # 仪表盘布局 + 脏矩形渲染
│   │   └── font/              # 像素字体
│   ├── key/                   # GPIO 按键驱动 (短按/长按)
│   └── perf_monitor/          # CPU 使用率监测
├── shouce/                    # 芯片数据手册 (PDF)
├── partitions_webserver.csv   # 分区表 (4M factory + 128K SPIFFS)
├── sdkconfig                  # ESP-IDF 项目配置
├── CMakeLists.txt             # 项目级 CMake
└── README.md
```
