# Codex Island for Waveshare ESP32-S3-Touch-AMOLED-1.75C

> 面向 Codex 的可直接执行开发任务书  
> 目标硬件：Waveshare ESP32-S3-Touch-AMOLED-1.75C 带电池版  
> 开发主机：macOS，硬件已通过 USB-C 连接  
> UI 视觉基准：`assets/ui-reference.png`

---

## 0. Codex 执行要求

请把本文档视为完整的实现规格，不要只生成脚手架、伪代码或设计说明。需要依次完成：

1. 检测开发板串口并确认 ESP32-S3 可通信；
2. 备份或确认可恢复出厂固件；
3. 建立独立仓库；
4. 基于微雪官方 ESP-IDF + LVGL 9 示例完成原生 466×466 固件；
5. 在真实硬件上编译、烧录并运行静态 UI；
6. 基于 `cc-island` 改造 macOS Bridge，只保留 Codex；
7. 完成 BLE NUS 通信、Codex 用量采集和 CodexRadar 数据采集；
8. 完成三页触摸 UI、电池/连接状态、缓存、断线重连和防烧屏；
9. 增加单元测试、README、安装脚本和 macOS LaunchAgent；
10. 最终输出实际执行过的构建、测试和烧录结果，不能仅声称“理论上可行”。

除非被 USB 权限、macOS 蓝牙权限或必须人工观察屏幕的步骤阻塞，否则不要中途询问用户。

---

# 1. 项目目标

开发一个独立项目，暂定仓库名：

```text
codex-island-waveshare
```

设备作为 Codex 桌面状态屏，显示三个横向滑动页面：

1. **CODEX Usage**：5 小时额度、7 天额度、重置倒计时、近 7 天本地活动柱状图、今日 token 和估算费用；
2. **CODEX RADAR**：从 CodexRadar 获取当前各模型 IQ，首屏显示最高的 5 项，其余项目可纵向翻页/滚动；
3. **STATUS**：最后同步时间、数据是否过期、BLE 状态、电池状态和 IQ 趋势线。

视觉效果必须贴近：

```text
assets/ui-reference.png
```

不是简单复刻原 `cc-island` 的双行界面，而是重新使用 LVGL 9 构建原生 466×466 圆屏 UI。

---

# 2. 明确范围

## 2.1 本期必须实现

- ESP32-S3-Touch-AMOLED-1.75C 原生 466×466 AMOLED 显示；
- CST92xx/CST9217 触摸；
- AXP2101 电源、电池电量、充电状态；
- BLE 5 LE，Nordic UART Service；
- macOS Bridge 自动发现并连接设备；
- Codex 5 小时/7 天额度；
- Codex 本地 session 日志的今日 token、今日估算费用；
- 最近 7 个自然日 token 活动柱状图；
- CodexRadar 当前多模型 IQ；
- Radar 数据本地缓存和 stale 状态；
- 三页 UI、触摸滑动、页面指示点；
- USB 供电与电池供电下不同的休眠策略；
- 自动重连、手动刷新、启动时显示最后缓存；
- macOS 登录后自动启动 Bridge；
- 单元测试和可复现构建文档。

## 2.2 本期不实现

- Claude/Anthropic 的任何页面或数据；
- GPS、麦克风、语音、扬声器功能；
- ESP32 直接保存 Codex 凭据；
- ESP32 直接访问 `~/.codex`；
- ESP32 直接解析 CodexRadar HTML；
- 云端中转服务器；
- OTA；
- 复杂设置 App；
- 将 `claude-desktop-buddy` 的 184×224 逻辑画布放大到屏幕。

---

# 3. 技术路线定稿

## 3.1 固件主基线

使用微雪官方仓库：

```text
https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C
```

主基线目录：

```text
examples/esp-idf/02_lvgl_demo_v9
```

电源和电池参考：

```text
examples/esp-idf/01_AXP2101
```

选择理由：

- 官方示例原生支持 466×466 CO5300 QSPI AMOLED；
- 官方 BSP 已完成显示和触摸初始化；
- 示例使用 LVGL 9；
- 官方 `sdkconfig.defaults` 已配置 OPI PSRAM、32MB Flash 和性能参数；
- 不需要自行重写 CO5300、CST92xx 和 AXP2101 底层驱动。

固件固定采用：

```text
ESP-IDF 5.5.4
LVGL 9.5.x
C/C++
NimBLE（ESP-IDF 自带）
```

不要以 Arduino/PlatformIO 作为最终主工程。PlatformIO 参考工程只用于核对按键、触摸、电源和休眠行为。

## 3.2 数据和 BLE 基线

使用：

```text
https://github.com/alexjc-tech/cc-island
```

重点参考：

```text
bridge/codexisland_bridge.py
firmware/app_codex/ble/ble_nus.h
firmware/app_codex/ble/ble_nus.cpp
```

复用内容：

- `~/.codex/auth.json` 的读取逻辑；
- Codex usage 请求逻辑；
- `~/.codex/sessions/**/rollout-*.jsonl` 解析逻辑；
- 本地 token 和费用统计逻辑；
- Bleak 连接和 Nordic UART Service；
- 换行分隔 JSON 的通信方式；
- 手动刷新请求思路。

不要复用：

- M5Stack Mooncake App 框架；
- M5Stack HAL；
- Claude 数据路径；
- 双行 Claude/Codex UI；
- 原固件的 256 字节单行限制；
- M5Stack 振动马达逻辑。

## 3.3 微雪板级行为参考

使用：

```text
https://github.com/vthinkxie/claude-desktop-buddy-esp32
```

仅参考：

```text
src/boards/
src/hw/
platformio.ini
```

重点核对：

- 1.75C 的 CO5300 初始化差异；
- CST92xx 触摸行为；
- AXP2101 电源键；
- BOOT/PWR 按键语义；
- USB 供电与电池供电下的休眠/唤醒；
- 屏幕关闭后触摸或按键唤醒。

**不得采用该仓库的 184×224 canvas → 276×336 → 居中放大方案。** 本项目必须直接在 466×466 LVGL 坐标系绘制。

---

# 4. 所有需要参考的 GitHub 仓库

Codex 开始工作后，将以下仓库浅克隆到项目的 `reference/`，并在 `reference/LOCKFILE.md` 记录实际 commit hash。`reference/` 默认加入 `.gitignore`，不把第三方完整源码提交进主仓库。

## 4.1 必须参考

| 用途 | 仓库 |
|---|---|
| 微雪官方硬件、BSP、LVGL、电源示例 | https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C |
| 原始 Codex usage、日志统计和 BLE Bridge | https://github.com/alexjc-tech/cc-island |
| 微雪 1.75C 板级适配、按键、电源、触摸参考 | https://github.com/vthinkxie/claude-desktop-buddy-esp32 |
| ESP-IDF 5.5.4 工具链 | https://github.com/espressif/esp-idf |
| LVGL 9 API 参考 | https://github.com/lvgl/lvgl |

## 4.2 可选参考，不作为代码基线

| 用途 | 仓库 |
|---|---|
| 原 `cc-island` 依赖的 M5Stack 出厂工程，用于理解原 UI 生命周期 | https://github.com/m5stack/M5StopWatch-UserDemo |
| 官方 Claude Desktop Buddy 的原始协议背景，仅作架构参考 | https://github.com/anthropics/claude-desktop-buddy |

## 4.3 非 GitHub 数据源

```text
https://codexradar.com/
https://codexradar.com/current.json
```

注意：公开 `/current.json` 主要用于站点当前状态/重置事件，不应假定其中一定包含多模型 IQ。多模型 IQ Provider 必须设计成可替换实现，优先支持站方授权 API；个人原型可以启用低频 HTML 解析 fallback。

---

# 5. 推荐仓库结构

```text
codex-island-waveshare/
├── AGENTS.md
├── README.md
├── LICENSES.md
├── .gitignore
├── assets/
│   └── ui-reference.png
│
├── firmware/
│   ├── CMakeLists.txt
│   ├── partitions.csv
│   ├── sdkconfig.defaults
│   └── main/
│       ├── CMakeLists.txt
│       ├── idf_component.yml
│       ├── app_main.cpp
│       │
│       ├── model/
│       │   ├── app_state.hpp
│       │   └── app_state.cpp
│       ├── protocol/
│       │   ├── protocol.hpp
│       │   ├── protocol.cpp
│       │   ├── radar_assembly.hpp
│       │   └── radar_assembly.cpp
│       ├── transport/
│       │   ├── ble_nus.hpp
│       │   └── ble_nus.cpp
│       ├── ui/
│       │   ├── ui_app.hpp
│       │   ├── ui_app.cpp
│       │   ├── ui_theme.hpp
│       │   ├── ui_theme.cpp
│       │   ├── widgets/
│       │   │   ├── gradient_arc.hpp
│       │   │   ├── gradient_arc.cpp
│       │   │   ├── status_icons.hpp
│       │   │   └── status_icons.cpp
│       │   ├── page_usage.hpp
│       │   ├── page_usage.cpp
│       │   ├── page_radar.hpp
│       │   ├── page_radar.cpp
│       │   ├── page_status.hpp
│       │   └── page_status.cpp
│       ├── power/
│       │   ├── power_manager.hpp
│       │   └── power_manager.cpp
│       ├── storage/
│       │   ├── state_cache.hpp
│       │   └── state_cache.cpp
│       └── input/
│           ├── input_manager.hpp
│           └── input_manager.cpp
│
├── bridge/
│   ├── pyproject.toml
│   ├── src/codex_island_bridge/
│   │   ├── __init__.py
│   │   ├── cli.py
│   │   ├── config.py
│   │   ├── service.py
│   │   ├── protocol.py
│   │   ├── cache.py
│   │   ├── codex_usage.py
│   │   ├── codex_sessions.py
│   │   ├── ble_transport.py
│   │   ├── radar/
│   │   │   ├── base.py
│   │   │   ├── authorized_api.py
│   │   │   ├── html_provider.py
│   │   │   └── mock_provider.py
│   │   └── radar_history.py
│   └── tests/
│       ├── test_protocol.py
│       ├── test_codex_sessions.py
│       ├── test_radar_html.py
│       └── fixtures/
│           └── codexradar_minimal.html
│
├── scripts/
│   ├── bootstrap_macos.sh
│   ├── detect_port.sh
│   ├── backup_flash.sh
│   ├── build_firmware.sh
│   ├── flash_firmware.sh
│   ├── run_bridge.sh
│   └── install_launch_agent.sh
│
├── launchd/
│   └── com.local.codex-island-bridge.plist.template
│
└── reference/
    └── LOCKFILE.md
```

---

# 6. macOS 上的第一轮硬件操作

## 6.1 检测串口

先执行：

```bash
system_profiler SPUSBDataType
ls -1 /dev/cu.* | sort
```

优先寻找：

```text
/dev/cu.usbmodem*
/dev/cu.wchusbserial*
/dev/cu.SLAB_USBtoUART*
```

将检测到的端口保存在当前 shell：

```bash
export ESP_PORT=/dev/cu.usbmodemXXXX
```

不要在脚本中永久写死端口；`scripts/detect_port.sh` 应自动筛选新出现的 ESP32-S3 串口，并支持 `ESP_PORT` 环境变量覆盖。

## 6.2 检查芯片和 Flash

```bash
python -m esptool --chip esp32s3 -p "$ESP_PORT" chip_id
python -m esptool --chip esp32s3 -p "$ESP_PORT" flash_id
```

1.75C 官方 ESP-IDF 示例按 **32MB Flash** 配置，但仍必须以 `flash_id` 的真实输出和当前官方示例为准，不根据旧社区仓库中的 8MB/16MB 描述修改分区。

## 6.3 备份出厂 Flash

先从 `flash_id` 确认容量：

```bash
# 32 MB 示例
python -m esptool --chip esp32s3 -p "$ESP_PORT" read_flash \
  0x0 0x2000000 factory-backup-32mb.bin
```

若实际为 16MB，则长度用 `0x1000000`。同时保存：

```bash
sha256sum factory-backup-*.bin > factory-backup.sha256
```

如果完整备份失败，不要阻塞开发，但必须在 README 中记录微雪官方 `Firmware/` 恢复路径。

## 6.4 ESP-IDF 环境

优先使用 ESP-IDF 5.5.4：

```bash
mkdir -p ~/esp
cd ~/esp
git clone --recursive -b v5.5.4 https://github.com/espressif/esp-idf.git esp-idf-v5.5.4
cd esp-idf-v5.5.4
./install.sh esp32s3
. ./export.sh
idf.py --version
```

Apple Silicon 上保证使用 ARM64 的 Python、CMake 和 Ninja，不混入 `/usr/local` 下的 x86_64 Homebrew 工具。

---

# 7. 固件初始化

## 7.1 建立主工程

克隆微雪官方仓库后，将以下目录复制为 `firmware/`：

```text
reference/ESP32-S3-Touch-AMOLED-1.75C/
└── examples/esp-idf/02_lvgl_demo_v9
```

保留官方：

- `sdkconfig.defaults`；
- `partitions.csv`；
- `idf_component.yml` 中的 Waveshare BSP 和 LVGL 依赖；
- `bsp_display_start()`；
- `bsp_display_lock()` / `bsp_display_unlock()`。

删除：

- `lv_demos` 的源码 glob；
- `lv_demo_benchmark()`；
- 所有 LVGL demo 配置；
- 与本项目无关的音乐、benchmark、stress demo。

`idf_component.yml` 至少保留：

```yaml
dependencies:
  waveshare/esp32_s3_touch_amoled_1_75c:
    version: "^3.0.0"
  lvgl/lvgl:
    version: "9.5.0"
    public: true
```

## 7.2 首次官方示例闭环

在开始写 UI 前，先原样构建并烧录官方 `02_lvgl_demo_v9`：

```bash
cd firmware
idf.py set-target esp32s3
idf.py fullclean
idf.py build
idf.py -p "$ESP_PORT" flash monitor
```

确认：

- 屏幕正常点亮；
- 466×466 画面方向正确；
- 触摸坐标方向正确；
- 无持续重启；
- PSRAM 检测正常；
- Flash 分区无错误。

随后提交一个独立 commit：

```text
chore: verify Waveshare LVGL baseline on ESP32-S3-Touch-AMOLED-1.75C
```

---

# 8. 固件状态模型

UI 不得直接读取 cJSON 对象。通信数据先转换成稳定领域模型，再原子提交状态。

```cpp
#pragma once

#include <array>
#include <cstdint>

constexpr size_t kMaxRadarModels = 24;
constexpr size_t kDailyPoints = 7;
constexpr size_t kTrendPoints = 12;

struct UsageState {
    bool valid = false;
    uint8_t five_hour_percent = 0;
    uint8_t seven_day_percent = 0;
    uint32_t reset_seconds = 0;
    uint64_t today_tokens = 0;
    uint32_t today_cost_cents = 0;
    std::array<uint64_t, kDailyPoints> daily_tokens{};
    int64_t updated_at = 0;
};

struct RadarModel {
    char family[16]{};
    char effort[16]{};
    int16_t iq_x10 = 0;
    uint8_t passed = 0;
    uint8_t total = 0;
};

struct RadarState {
    bool valid = false;
    bool stale = true;
    int64_t updated_at = 0;
    uint8_t count = 0;
    std::array<RadarModel, kMaxRadarModels> models{};
    std::array<int16_t, kTrendPoints> trend_iq_x10{};
    uint8_t trend_count = 0;
};

struct LinkState {
    bool ble_connected = false;
    int64_t last_packet_at = 0;
};

struct PowerState {
    uint8_t battery_percent = 0;
    bool charging = false;
    bool usb_present = false;
};

struct AppState {
    UsageState usage;
    RadarState radar;
    LinkState link;
    PowerState power;
};
```

要求：

- 使用 mutex 或单写者任务保证状态一致性；
- BLE callback 不得直接操作 LVGL；
- LVGL 更新必须在显示锁内完成；
- 协议解析失败时保留上一份有效状态；
- 所有字符串写入固定数组时必须边界检查并保证 `\0` 结尾。

---

# 9. BLE NUS 设计

## 9.1 UUID

采用 Nordic UART Service：

```text
Service: 6e400001-b5a3-f393-e0a9-e50e24dcca9e
RX:      6e400002-b5a3-f393-e0a9-e50e24dcca9e
TX:      6e400003-b5a3-f393-e0a9-e50e24dcca9e
```

设备广播名：

```text
Codex Island-XXXX
```

`XXXX` 使用 BT MAC 后两字节，避免多设备重名。

## 9.2 传输规则

- UTF-8；
- 每行一个 JSON；
- 行尾 `\n`；
- BLE 分包由接收端重新组装；
- 单行最大长度从原项目的 256 提升为 **2048 字节**；
- 超长行丢弃，并在串口输出错误；
- JSON 必须包含协议版本 `v`、消息类型 `k`、序列号 `seq`；
- 重复 `seq` 可以幂等处理。

## 9.3 Bridge → ESP32：Usage

```json
{
  "v": 1,
  "k": "usage",
  "seq": 101,
  "ts": 1784100000,
  "p5": 42,
  "p7": 31,
  "reset_s": 4980,
  "tok": 486200,
  "cost_c": 231,
  "daily": [15200, 83000, 47000, 92000, 66000, 101000, 486200]
}
```

字段：

| 字段 | 含义 |
|---|---|
| `p5` | 5 小时窗口已用百分比 |
| `p7` | 7 天窗口已用百分比 |
| `reset_s` | 距离 5 小时窗口重置的秒数 |
| `tok` | 今日 token |
| `cost_c` | 今日估算费用，美分 |
| `daily` | 最近 7 个本地自然日 token 活动量 |

## 9.4 Bridge → ESP32：Radar

```json
{
  "v": 1,
  "k": "radar",
  "seq": 102,
  "ts": 1784100000,
  "stale": false,
  "models": [
    ["Sol", "max", 1200, 8, 10],
    ["Sol", "xhigh", 1500, 10, 10],
    ["Terra", "max", 1200, 8, 10],
    ["Luna", "max", 900, 6, 10]
  ],
  "trend": [900, 1050, 1050, 1200, 1050, 1200]
}
```

IQ 采用 `iq_x10`，避免浮点传输：

```text
120.0 → 1200
75.0  → 750
```

若未来模型数量导致消息超过 2048 字节，再实现 `radar_chunk`，MVP 不必提前复杂化。

## 9.5 ESP32 → Bridge

手动刷新：

```json
{"v":1,"k":"refresh","what":"all"}
```

设备就绪：

```json
{"v":1,"k":"hello","fw":"0.1.0"}
```

Bridge 收到 `hello` 后立即推送本地缓存，再异步刷新网络数据，避免设备连接后长时间空白。

---

# 10. FreeRTOS 任务划分

建议：

```text
app_main
├── BSP/display/touch 初始化
├── UI 创建
├── BLE NUS 初始化
├── protocol_task
├── ui_tick_task
├── power_task
└── input_task（若 BSP 未直接接入 LVGL）
```

## 10.1 BLE callback

只做：

```text
字节接收 → 按 \n 组行 → 放入 FreeRTOS Queue
```

不得：

- cJSON 深度解析；
- 更新 `AppState`；
- 调用 LVGL；
- 执行阻塞 I/O。

## 10.2 protocol_task

```text
Queue 取完整消息
→ cJSON 解析
→ 校验版本/类型/范围
→ 构造 next AppState
→ 原子 commit
→ 通知 UI 刷新
```

## 10.3 ui_tick_task

每 1 秒：

- 更新 reset 倒计时；
- 更新 `Last sync Xm ago`；
- 检查 stale；
- 更新电池图标；
- 处理自动换页和像素偏移。

不要每秒重建对象；只修改 label、chart series 和 style 值。

---

# 11. UI 规格：严格按参考图实现

屏幕：

```text
466 × 466
圆形 AMOLED
背景：纯黑 #000000
安全内容区域建议：x=42..424，y=28..438
```

整体要求：

- 原生 466×466；
- 不使用矩形卡片大底色；
- 深黑背景，少量细分割线；
- 文字以白色为主；
- 蓝、青、绿、紫、橙做 AMOLED 高饱和点缀；
- 动画克制，避免持续高帧率；
- 三个页面底部均显示 3 个 page dots；
- 横向滑动切换三页；
- 页面一与参考图主屏尽量像素级接近；
- 页面二、三与参考图右侧圆形预览一致。

## 11.1 主题色

```cpp
constexpr uint32_t kBlack       = 0x000000;
constexpr uint32_t kWhite       = 0xF7F7F7;
constexpr uint32_t kMuted       = 0x98A6B8;
constexpr uint32_t kDivider     = 0x18212B;
constexpr uint32_t kBlue        = 0x1684FF;
constexpr uint32_t kCyan        = 0x00C8D7;
constexpr uint32_t kGreen       = 0x79F14A;
constexpr uint32_t kLime        = 0xB9F35A;
constexpr uint32_t kPurple      = 0xA84EFF;
constexpr uint32_t kMagenta     = 0xE94BEA;
constexpr uint32_t kOrange      = 0xFFB000;
constexpr uint32_t kDarkRing    = 0x10231A;
```

最终颜色可依据真机 AMOLED 观感微调，但不能改成大面积亮色背景。

## 11.2 字体

优先使用 LVGL 内置 Montserrat，避免额外字体许可证和大体积资源。

启用至少：

```text
Montserrat 12 / 14 / 16 / 18 / 20 / 22 / 24 / 28 / 32 / 36 / 48
```

建议：

| 元素 | 字号 |
|---|---:|
| 页面标题 | 22–24 |
| 主百分比数字 | 48 |
| 主百分号 | 28–32 |
| 7 DAY | 24–28 |
| 普通正文 | 17–20 |
| 小标签/星期 | 12–14 |

如果 Montserrat 48 真机上仍偏小，再单独生成仅含 `0123456789%` 的数字字体，不引入整套大字体。

---

# 12. 页面一：CODEX Usage

## 12.1 布局

视觉结构：

```text
          [link]   CODEX   [battery]

              gradient usage arc
                    42 %
                 5h window
              ─────────────
              reset in 1h 23m

              ─────────────
                  7 DAY 31%
             ▁  ▃  ▂  ▅  ▄  ▆  █
             M  T  W  T  F  S  S
              ─────────────
           [token] Today    [$] $2.31
                   486K tok

                    ●  ○  ○
```

建议坐标，允许真机微调 ±8 px：

| 元素 | 中心/区域 |
|---|---|
| 顶部连接图标 | `(145, 54)` |
| `CODEX` | `(233, 55)` |
| 电池图标 | `(342, 54)` |
| 渐变环 | 中心约 `(233, 183)`，直径 `250–270` |
| `42` | 中心约 `(220, 174)` |
| `%` | 紧随数字右侧 |
| `5h window` | y≈225 |
| reset 分隔线 | y≈252，宽约 160 |
| reset 文本 | y≈274 |
| 7 DAY 分隔线 | y≈300，宽约 300 |
| `7 DAY 31%` | y≈322 |
| 7 根柱 | y≈350–394 |
| 星期字符 | y≈400 |
| 底部信息分隔线 | y≈412 |
| Token/Cost | y≈425 |
| page dots | y≈448 |

坐标以视觉参考为准，若圆形边缘裁切则优先保证文本和数值完整。

## 12.2 渐变进度环

普通 `lv_arc` 无法完全还原蓝→青→绿的圆周渐变。实现自定义 `gradient_arc`：

- 使用 LVGL draw event；
- 将完整有效弧分成 90–140 个小 arc segment；
- 根据 segment 比例在线性插值：蓝 → 青 → 绿；
- 进度末端绘制绿色圆点；
- 未使用部分绘制暗绿色半透明弧；
- 起止角和参考图一致，底部保留开口；
- 数据变化时可用 400ms ease-out 动画更新，不持续动画。

## 12.3 近 7 日柱状图

柱状图表达的是 **本地 Codex token 活动量**，不是周额度细分。

- 7 根圆角柱；
- 高度根据 7 日最大值归一化；
- 最低可见高度 8 px；
- 每根柱从蓝到紫/粉渐变；
- 标签固定为设备本地时区下的 `M T W T F S S`；
- `7 DAY 31%` 中 31% 仍是服务端周窗口使用率。

## 12.4 底部信息

左侧：

```text
Today
486K tok
```

右侧：

```text
$2.31
```

费用必须在 Bridge 和 README 中标注为 `estimated`，设备上为保持参考图简洁可以不显示该单词。

---

# 13. 页面二：CODEX RADAR

视觉结构：

```text
              [radar icon]
              CODEX RADAR

       ●  Sol max                 120
       ●  Sol xhigh               105
       ●  Terra max               105
       ●  Luna max                 90
       ●  Terra high               45

                    ○  ●  ○
```

要求：

- 标题居中；
- 首屏显示 IQ 最高的 5 项；
- 分数相同按站点原顺序；
- 每行左侧彩色圆点、模型名，右侧 IQ；
- 数值只在小数部分为 `.0` 时显示整数，否则保留一位小数；
- 颜色按当前可见排名固定使用：
  1. 绿色；
  2. 黄绿色；
  3. 蓝色；
  4. 紫色；
  5. 橙色；
- 上下滑动或自动轮播查看剩余模型；
- 子列表切换不改变底部三个顶层 page dots；
- Radar 无数据时显示 `Waiting for radar data`，不要显示假数值；
- Radar 数据过期时标题下方显示小号橙色 `STALE`。

Radar 数据按 `iq_x10` 排序，UI 不硬编码 Sol/Terra/Luna，必须支持未来新增 family 和 effort。

---

# 14. 页面三：STATUS

视觉结构：

```text
              [pulse icon]
                 STATUS

          [check]  Last sync
                   12m ago

          ─────────────────

          [shield] stale: no

              small green trend line

                    ○  ○  ●
```

另外在不破坏参考图的前提下，用小图标或很小的文字表达：

- BLE connected/disconnected；
- battery %；
- charging；
- USB present。

趋势线：

- 默认显示最近 12 次 `Sol max` IQ；
- 找不到 `Sol max` 时显示每次采样中的最高 IQ；
- 数据由 Bridge 维护和下发；
- 绿色折线，底部淡绿色透明填充；
- 只有 0–1 个点时显示水平点/短线；
- 自动计算纵轴 min/max，并留 10% padding；
- 不显示坐标轴文字，保持参考图简洁。

---

# 15. 触摸和按键

## 15.1 触摸

- 左/右滑：切换 Usage / Radar / Status；
- Radar 页面上/下滑：切换 Radar 列表子页；
- 点击底部 page dot：切换顶层页面；
- 单击屏幕：从熄屏唤醒；
- 不实现复杂手势冲突；
- 使用 LVGL gesture 或 tileview，避免手写不稳定的触摸坐标滤波。

## 15.2 实体键

以官方 BSP 和 `vthinkxie` 1.75C 适配核对实际按键，不自行猜引脚。

建议语义：

| 操作 | 行为 |
|---|---|
| BOOT 短按 | 下一页 |
| BOOT 双击 | 请求立即刷新 |
| PWR 短按 | 屏幕开/关 |
| PWR 长按 | 保留 AXP2101 原有硬关机行为 |

不要覆盖或破坏 AXP2101 的硬件长按关机。

---

# 16. 电源和 AMOLED 防烧屏

基于微雪官方：

```text
examples/esp-idf/01_AXP2101
```

实现：

- 电池电量；
- USB/VBUS 是否存在；
- 是否充电；
- 屏幕开关；
- 低电量状态。

策略：

## USB 供电

- 默认亮度 30–40%；
- 不强制熄屏；
- 每 15 秒自动轮换页面；
- 每 5 分钟整体 UI 随机偏移 1–2 px；
- 允许触摸后暂停自动轮换 60 秒。

## 电池供电

- 30 秒无操作降低亮度；
- 120 秒无操作关闭显示；
- 触摸或按键唤醒；
- 低于 10% 时显示橙色电池图标；
- 不因 BLE 断线持续高频扫描导致异常耗电。

如 BSP 没有直接提供亮度 API，先检查 CO5300/官方 BSP 可用接口；不要未经验证直接写面板寄存器。

---

# 17. Bridge 改造

## 17.1 Python 工程

使用 Python 3.11+，建立 `pyproject.toml`。依赖至少：

```text
bleak
requests
beautifulsoup4
certifi
platformdirs
```

测试依赖：

```text
pytest
pytest-cov
```

CLI：

```bash
codex-island-bridge print
codex-island-bridge run
codex-island-bridge once
codex-island-bridge radar-test
codex-island-bridge devices
```

## 17.2 Codex usage Provider

从 `cc-island` 移植，但封装为：

```python
class CodexUsageProvider:
    def fetch(self) -> UsageSnapshot:
        ...
```

要求：

- 读取 `~/.codex/auth.json`；
- 不打印 access token；
- 不写入 access token 到日志、缓存或 BLE；
- 请求失败返回结构化错误；
- 使用超时；
- 保留最近一次有效 usage；
- provider 的 HTTP 细节与 UI/协议解耦，便于接口变化时单独修复。

## 17.3 Session 统计

扫描：

```text
~/.codex/sessions/**/rollout-*.jsonl
```

在原 `cost_codex()` 基础上增加：

- 今日 token；
- 今日估算费用；
- 最近 7 个本地自然日 token；
- 文件增量扫描缓存，避免每 5 分钟全量重扫大型历史目录；
- 文件损坏或单行 JSON 错误时跳过该行并告警，不终止服务；
- 时区使用 macOS 本地时区；
- 价格表和估算规则集中在一个模块并带注释。

## 17.4 更新周期

```text
Usage：每 5 分钟
Radar：每 60 分钟
BLE 重连：指数退避，最大 60 秒
设备手动刷新：最短间隔 5 秒
```

设备连接后：

1. 立即发送缓存；
2. 再刷新 usage；
3. Radar 距离上次成功不足 30 分钟时不强制抓网页；
4. 最终发送新状态。

---

# 18. CodexRadar Provider

## 18.1 Provider 接口

```python
from typing import Protocol

class RadarProvider(Protocol):
    def fetch(self) -> "RadarSnapshot":
        ...
```

实现：

```text
AuthorizedApiRadarProvider
HtmlRadarProvider
MockRadarProvider
```

优先级：

1. 若配置 `CODEX_RADAR_API_URL`，使用授权/稳定 JSON API；
2. 若显式配置 `CODEX_RADAR_ALLOW_HTML=1`，允许 HTML Provider；
3. 开发和 UI 测试使用 Mock Provider；
4. 默认不静默使用 mock 冒充真实数据。

## 18.2 HTML Provider

目标网页：

```text
https://codexradar.com/
```

解析时不要依赖“第 N 个 div”或动态 CSS class。围绕语义解析：

1. 找到标题文本 `降智雷达`，解析更新时间；
2. 找到 `本次多模型指标`；
3. 读取以 `项目` 开头的模型列；
4. 读取 `通过数`；
5. 读取 `IQ`；
6. 按列配对；
7. 校验模型数、IQ 数、通过数长度完全一致；
8. IQ 合理范围暂设 `0 < IQ <= 300`；
9. 至少 3 个模型才认为成功；
10. 解析失败不得覆盖最后有效缓存。

当前页面示例结构大致为：

```text
项目 Sol max Sol xhigh ... Terra max ... Luna high
通过数 8/10 10/10 ...
IQ 120.0 150.0 ...
```

不要依赖示例具体数值，因为站点会持续更新。

## 18.3 访问频率和缓存

- 最短请求间隔 30 分钟；
- 默认 60 分钟；
- 保存 `ETag` 和 `Last-Modified`（若服务端提供）；
- 使用明确 User-Agent；
- 请求超时 15 秒；
- 失败时指数退避；
- 缓存路径使用 `platformdirs`：

```text
~/Library/Application Support/CodexIsland/
├── usage_cache.json
├── radar_cache.json
└── radar_history.json
```

`stale` 判定：

```text
当前时间 - radar.updated_at > 18 小时
```

站点通常一天更新两次，因此 18 小时既能容忍延迟，又能提示明显过期。

## 18.4 Radar 历史

每次 Radar IQ 内容发生变化时保存一条采样：

```json
{
  "ts": 1784100000,
  "models": {
    "Sol/max": 1200,
    "Sol/xhigh": 1500
  }
}
```

最多保留 90 天或 500 条。下发给设备的 `trend` 默认取最近 12 条 `Sol/max`。

---

# 19. 本地缓存

## Bridge 缓存

缓存：

- Usage 最后有效值；
- Radar 最后有效值；
- Radar 历史；
- Session 文件增量索引。

写缓存必须：

- 临时文件写入；
- `fsync`；
- 原子 rename；
- JSON 带 schema version。

## ESP32 缓存

使用 NVS 保存最后一次：

- Usage；
- Radar；
- Radar trend；
- 最后同步时间；
- 当前页面；
- 亮度设置。

避免每秒写 NVS；只在收到新有效数据且内容发生变化时写入。

启动顺序：

```text
加载 NVS 缓存 → 立即绘制 → 启动 BLE → 收到 hello/新状态后更新
```

---

# 20. 错误状态

必须区分：

| 状态 | UI 行为 |
|---|---|
| 从未收到 Usage | 页面一显示 `Waiting for Mac` |
| BLE 断开但有缓存 | 保留数值，顶部 link 图标变灰 |
| Usage 请求失败 | 保留旧数值，Status 显示同步时间 |
| Radar 请求失败 | 保留旧 Radar，可能进入 stale |
| Radar 从未成功 | Radar 页面显示 `Waiting for radar data` |
| JSON 解析失败 | 保留全部旧状态并记录串口错误 |
| 电池低 | 电池图标橙色 |

不得在网络错误时把百分比、token 或 IQ 清零，因为零会被误解为真实数据。

---

# 21. CMake 和 sdkconfig 改动

## 21.1 CMake

- 移除 LVGL demos 源码 glob；
- 使用 `file(GLOB_RECURSE APP_SOURCES ...)` 或显式列出 `main/` 下源文件；
- C++ 标准至少 C++17；
- 依赖：NVS、NimBLE、cJSON、LVGL、Waveshare BSP；
- 开启严格 warning，但第三方组件 warning 不作为本项目错误。

## 21.2 sdkconfig.defaults

从官方 1.75C LVGL demo 继承，并调整：

- 保留 `CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y`；
- 保留 OPI PSRAM；
- 启用 Bluetooth、NimBLE peripheral；
- 启用需要的 Montserrat 字号；
- 关闭全部 LVGL demo；
- 关闭 LVGL performance overlay；
- cJSON 使用 ESP-IDF 组件；
- 日志默认 INFO；
- 发布构建允许将部分底层日志调为 WARN。

不要从零手写 sdkconfig，避免丢失官方显示性能配置。

---

# 22. 测试要求

## 22.1 Bridge 单元测试

至少覆盖：

1. Usage JSON → 协议 JSON；
2. 费用美分转换；
3. 今日/近 7 日时区边界；
4. session 损坏行跳过；
5. Radar HTML 正常解析；
6. Radar 表头与 IQ 数量不一致时失败；
7. Radar 无更新时间时的 fallback；
8. Radar 失败不覆盖缓存；
9. `stale` 判定；
10. BLE 消息长度小于 2048；
11. 重复 seq 幂等；
12. Radar 排序和 top-5。

HTML fixture 只保存经过精简的相关表格，不保存完整网站页面。

## 22.2 固件测试

使用 ESP-IDF Unity 或纯 C++ 可测试模块覆盖：

- Usage 协议解析；
- Radar 数组解析；
- 过长 family/effort 截断；
- 超范围百分比拒绝；
- IQ 排序；
- reset 倒计时；
- stale 时间计算；
- 超长 BLE 行丢弃；
- JSON 错误不更新状态。

## 22.3 真机验收

必须在真实 1.75C 上验证：

- 冷启动正常；
- 画面方向正确；
- 无边缘文字裁切；
- 三页横滑正常；
- Radar 子页上下滑正常；
- BLE 在 macOS 可发现；
- Bridge 首次连接成功；
- 断开 USB 后电池运行；
- 电池百分比/充电状态合理；
- 屏幕休眠和唤醒正常；
- Bridge 重启后自动重连；
- Mac 睡眠唤醒后可恢复；
- 24 小时运行无重启和明显内存增长。

---

# 23. 分阶段实施与提交

## 阶段 A：硬件基线

交付：

- 官方 LVGL demo 构建/烧录成功；
- 串口、Flash、PSRAM 记录；
- 出厂备份或恢复说明。

提交：

```text
chore: establish Waveshare ESP-IDF baseline
```

## 阶段 B：静态 UI

使用固定 mock 数据完成三页，并烧录到真机：

```text
42%
7 DAY 31%
reset in 1h 23m
Today 486K tok
$2.31
```

Radar mock 与参考图一致。

提交：

```text
feat(ui): implement native 466x466 Codex dashboard
```

## 阶段 C：BLE NUS

完成：

- 广播；
- macOS 发现；
- JSON 收发；
- mock Bridge 推送；
- hello/refresh。

提交：

```text
feat(ble): add Nordic UART transport and protocol
```

## 阶段 D：真实 Codex Usage

完成：

- 只保留 Codex；
- 额度、reset、token、cost、7 日 activity；
- 缓存；
- UI 实时更新。

提交：

```text
feat(bridge): add Codex usage and session aggregation
```

## 阶段 E：CodexRadar

完成：

- Provider 架构；
- HTML/API；
- cache/stale/history；
- Radar 和 Status 页面。

提交：

```text
feat(radar): add model IQ collection and display
```

## 阶段 F：产品化

完成：

- AXP2101；
- 休眠、防烧屏；
- LaunchAgent；
- 测试；
- README；
- 24h 稳定性。

提交：

```text
feat: complete power management and macOS autostart
```

---

# 24. 验收标准

只有同时满足以下条件，才算项目完成：

## 固件

- [ ] 使用官方 Waveshare ESP-IDF BSP；
- [ ] 直接渲染 466×466，不是低分辨率放大；
- [ ] 三页 UI 与参考图整体结构、比例和配色一致；
- [ ] 触摸滑动无明显误触；
- [ ] BLE NUS 稳定；
- [ ] AXP2101 电池状态可用；
- [ ] 缓存、断线、stale 状态正确；
- [ ] 无 Claude 内容；
- [ ] 无 Codex 凭据落盘到设备。

## Bridge

- [ ] 可从 `~/.codex` 获取真实数据；
- [ ] 可输出 `print` JSON；
- [ ] 可通过 BLE 推送；
- [ ] 可解析 CodexRadar 当前多模型 IQ；
- [ ] 解析失败不破坏缓存；
- [ ] 可通过 LaunchAgent 登录自启；
- [ ] token 不出现在日志。

## 工程质量

- [ ] `pytest` 全部通过；
- [ ] `idf.py build` 通过；
- [ ] 已在真实硬件执行 flash；
- [ ] README 包含从零安装步骤；
- [ ] 记录第三方许可证；
- [ ] 记录实际 reference commit hash；
- [ ] 没有未说明的 TODO 阻塞核心功能。

---

# 25. 许可证和商标

- Waveshare 官方仓库：Apache-2.0；
- `cc-island`：MIT；
- `claude-desktop-buddy-esp32`：只作为参考，复制代码时保留对应许可证和版权；
- LVGL：遵循其仓库许可证；
- 不复制 Claude Logo；
- `CODEX` 文本仅作为服务识别；
- 如使用 OpenAI 标志图形，在 README 中加入非官方、非背书声明；MVP 建议只使用 `CODEX` 文本和自制通用图标。

新增：

```text
LICENSES.md
```

说明每一段第三方代码的来源、文件路径、许可证和修改内容。

---

# 26. 最终输出格式

Codex 完成后必须汇报：

1. 新建/修改的文件列表；
2. 架构说明；
3. 实际串口名称；
4. `chip_id` 和 `flash_id` 结果；
5. ESP-IDF 版本；
6. `idf.py build` 结果；
7. `pytest` 结果；
8. 烧录命令和结果；
9. BLE 连接日志；
10. Codex usage 实际数据是否成功；
11. CodexRadar 实际数据是否成功；
12. 仍需人工观察确认的 UI 项；
13. 未解决问题及对应日志，不得隐藏失败。

---

# 27. 可直接交给 Codex 的启动 Prompt

```text
请在当前工作目录完整执行 CODEX_IMPLEMENTATION_SPEC.md。

目标硬件是已通过 USB-C 连接到这台 Mac 的 Waveshare ESP32-S3-Touch-AMOLED-1.75C 带电池版。UI 参考图是 assets/ui-reference.png。

不要只给设计或生成脚手架。先检测串口、ESP32-S3 和 Flash，备份或确认恢复方案；然后建立独立工程，基于 Waveshare 官方 ESP-IDF `examples/esp-idf/02_lvgl_demo_v9`，使用原生 466×466 LVGL 9 实现三页 UI。数据与 BLE 参考 alexjc-tech/cc-island，只保留 Codex；硬件行为参考 vthinkxie/claude-desktop-buddy-esp32，但不得使用其低分辨率 canvas 放大方案。

按照文档阶段提交代码，并在每个阶段实际 build；静态 UI 完成后烧录真机，随后完成 BLE Bridge、真实 Codex usage、CodexRadar、缓存、AXP2101、电池管理和 LaunchAgent。可以自行安装缺少的开发依赖。除 USB/蓝牙权限或必须由我肉眼确认屏幕效果外，不要中途停下来询问。最终给出真实执行的 build、test、flash 和运行日志摘要。
```

