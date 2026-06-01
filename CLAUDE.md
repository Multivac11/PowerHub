# CLAUDE.md

本文件为 Claude Code (claude.ai/code) 在此仓库中工作时提供指导。

## 构建系统

ESP-IDF v5.5.3 项目，目标芯片为 **ESP32-S3**，使用 C++17。

```bash
# 构建
idf.py build

# 烧录到设备（请根据实际情况替换 PORT）
idf.py -p /dev/ttyUSB0 flash

# 构建 + 烧录 + 串口监视
idf.py -p /dev/ttyUSB0 flash monitor

# 完整清理构建
idf.py fullclean build

# 配置（menuconfig）
idf.py menuconfig
```

这是一个 VS Code ESP-IDF 项目；`.vscode/` 目录下的设置假定已安装 ESP-IDF 扩展。

## 架构

**桌面电源监测站（Desktop Power Station）** — 5 通道功率监测，带 LCD 仪表盘和 WiFi 配置门户。

### 硬件（GPIO 引脚分配至关重要）

| 外设 | 引脚 | 备注 |
|---|---|---|
| **I2C 总线** (I2C_NUM_0) | SCL=17, SDA=16 | 100kHz |
| **INA226 #1–#5** | 0x40, 0x41, 0x42, 0x44, 0x45 | 3mΩ 采样电阻, 最大 19A, 配置=0x056F |
| **LCD SPI 命令** | CS=1, SCL=2, SDA=42 | SPI2_HOST, 3 线 9 位 |
| **LCD RGB565** | 16 个引脚（详见 `lcd_rgb.cpp`） | HSYNC=8, VSYNC=18, PCLK=19, DE=20, BL=7 |
| **按键** | GPIO 4, 5, 6 | 上拉, 短按/长按（2 秒） |

### 组件层级

```
main.cpp (app_main)
├── StatusKey          — 3 按键输入 → FreeRTOS 队列
├── DeviceInit         — I2C 总线 + 注册 5 个 INA226
│   └── I2CBusManager  — 单例, 管理 I2C_NUM_0
│       └── INA226     — 每个芯片的驱动（探测、校准、读取 V/A/W）
├── ApWifi             — 自动连接 + AP 配置门户
│   ├── WifiManager    — STA/APSTA + NVS 凭据存储（最多 5 个）
│   └── WsServer       — HTTP 服务器 + /ws WebSocket
├── PowerMonitor       — 每 ~70ms 轮询所有 5 个 INA226 → 队列
└── SceneManager       — LCD 仪表盘, 脏矩形更新
    ├── LcdRgb         — ST7701S 初始化 + LVGL 端口 + DMA 刷新
    └── LcdDriver      — 软件帧缓冲（RGB565, PSRAM）
```

### 设计模式

- **Meyers 单例模式** — 所有主要组件（`I2CBusManager`, `PowerMonitor`, `WifiManager`, `ApWifi`, `WsServer`, `StatusKey`, `LcdRgb`, `SceneManager`）。
- **基于 FreeRTOS 队列的发布/监听模式** — `PowerMonitor` 发布读数，`StatusKey` 发布按键事件，`WifiManager` 发布状态变化。监听者注册队列；队列满时发布者覆盖最旧条目。`QueueHandle_t` 数组上限为 10 个监听者。
- **脏矩形渲染** — `SceneManager` 在刷新到 LCD 之前只重绘 PSRAM 帧缓冲中发生变化的区域。
- **WebSocket JSON 协议** — AP 配置页面发送 `{type, ssid, password}`；服务器回复扫描结果和连接状态。

### 线程模型

| 任务 | 核心 | 优先级 | 栈大小 |
|---|---|---|---|
| PowerMonitor | 1 | 2 | 8192 |
| StatusKey（轮询） | 1 | 2 | 4096 |
| UIManagerTask（渲染） | 0 | 5 | 16384 |
| MonitorListenerTask | 0 | 5 | 4096 |

### 关键文件

- [main/main.cpp](main/main.cpp) — 入口点，初始化顺序
- [components/device/i2c_device/ina226/ina226.h](components/device/i2c_device/ina226/ina226.h) — INA226 寄存器映射和驱动 API
- [components/lcd/lcd_rgb.cpp](components/lcd/lcd_rgb.cpp) — ST7701S 初始化序列（请勿重排寄存器写入顺序）
- [components/lcd/scene_manager.cpp](components/lcd/scene_manager.cpp) — 仪表盘布局和渲染
- [components/wifi_manager/ap_wifi.cpp](components/wifi_manager/ap_wifi.cpp) — 自动连接逻辑和 WebSocket 消息处理
- [components/html/apcfg.html](components/html/apcfg.html) — WiFi 配置网页（游戏机主题）
- [partitions_webserver.csv](partitions_webserver.csv) — 分区表（factory=4M, html SPIFFS=128K）
- `shouce/tca9535.pdf` — TCA9535 I/O 扩展器数据手册（固件中尚未使用）

### 依赖

通过 `main/idf_component.yml` 管理：`lvgl/lvgl` ^9.3.0, `espressif/esp_lvgl_port` ^2.6.2。
实际解析版本：LVGL v9.5.0, esp_lvgl_port v2.8.0。

所需的 ESP-IDF 组件：`driver`, `esp_timer`, `esp_driver_i2c`, `esp_driver_spi`, `esp_event`, `esp_wifi`, `esp_http_server`, `esp_lcd`, `spiffs`, `nvs_flash`, `json`, `esp_lvgl_port`, `esp_driver_ppa`。

### 添加新的 I2C 设备

1. 创建继承自 `I2CDevice` 的驱动类（参见 `ina226.h/cpp`）。
2. 在 `DeviceInit::device_init()` 中注册 — 先调用 `Probe()` 再调用 `Init()`。
3. 如果设备发布数据，遵循 `PowerMonitor` 的队列模式。
