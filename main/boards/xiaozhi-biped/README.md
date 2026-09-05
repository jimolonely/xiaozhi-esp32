# XiaoZhi 双足机器人 (xiaozhi-biped)

基于 ESP32-S3-DevKitC-1 的双足语音机器人。继承小智的语音助手能力，添加了：

- 2 条机械腿（每条 2 个舵机：髋关节 + 膝关节），共 4 个 MG90 舵机
- 通过 PCA9685 16 路 PWM 驱动板控制舵机（I2C 接口）
- HC-SR04 超声波传感器检测前方障碍
- 随机游走 + 本地记忆 + 自主语音评论

## 硬件清单

| 模块 | 型号 | 接口 | GPIO |
|------|------|------|------|
| 主控 | ESP32-S3-DevKitC-1 | - | - |
| 麦克风 | INMP441 | I2S | WS=4, SCK=5, DIN=6 |
| 功放 | MAX98357A | I2S | DOUT=7, BCLK=15, LRC=16 |
| 显示屏 | OLED 0.96" (SSD1306) | I2C | SDA=41, SCL=42 |
| 舵机驱动板 | PCA9685 | I2C | SDA=1, SCL=2, ADDR=0x70 |
| 超声传感器 | HC-SR04 | GPIO | TRIG=8, ECHO=9 |
| 舵机 | MG90 × 4 | PWM | CH0-3 (PCA9685) |

## 电源

- 2S LiPo 7.4V → Buck 降压 → 5V
- 5V 直接给 PCA9685 V+ 端口（舵机电源独立）
- 5V 同样给 DevKitC-1 5V 引脚（ESP32 电源）

PCA9685 与 ESP32 DevKitC-1 **必须共地**。

## 舵机通道分配 (PCA9685)

| 通道 | 舵机 |
|------|------|
| CH0 | 左腿髋关节 |
| CH1 | 左腿膝关节 |
| CH2 | 右腿髋关节 |
| CH3 | 右腿膝关节 |

## 行为模式

### 自主随机游走

启用后，机器人每隔 5-30 秒随机：
1. 等待 → 原地小动作（环顾四周）
2. 决定下一步（70% 本地随机 / 30% 云端 AI）
3. 行走 / 转向 / 摇头 / 鞠躬
4. 同步语音评论（如"换个方向看看"）
5. 写入本地记忆

### 障碍避让

HC-SR04 检测到前方 <20cm 障碍时，立即转向避开并发出"哎呀差点撞到"等评论。

### 直接控制

可由 MCP 工具直接触发走 / 转 / 站 / 坐 / 鞠躬等动作。

## MCP 工具集

| 工具 | 描述 |
|------|------|
| `self.biped.set_autonomous` | 启用/禁用自主行为 |
| `self.biped.do_action` | 立即触发一次决策 |
| `self.biped.walk` | 向某方向走几步 |
| `self.biped.turn` | 原地转向 |
| `self.biped.gesture` | 简单动作（站/坐/鞠躬/摇头） |
| `self.biped.get_distance` | 查询前方距离 |
| `self.biped.add_memory` | 添加记忆 |
| `self.biped.get_memories` | 获取最近 N 条记忆 |
| `self.biped.clear_memories` | 清空记忆 |
| `self.biped.get_status` | 查询当前状态 |

## 按钮

- **短按 BOOT 按钮**：进入/退出对话状态
- **双击 BOOT 按钮**：切换自主行为开关

## 构建

```sh
python3 scripts/build.py xiaozhi-biped --name xiaozhi-biped
```

## 目录结构

```
main/boards/xiaozhi-biped/
├── config.h              # GPIO + 硬件常量
├── config.json           # 构建变体配置
├── pca9685.cc/.h         # PCA9685 驱动
├── ultrasonic_sensor.cc/.h  # HC-SR04 驱动
├── biped_controller.cc/.h   # 双足步态控制器
├── biped_memory.cc/.h       # NVS 记忆存储
├── biped_behavior.cc/.h     # 随机游走状态机
├── biped_main_controller.cc/.h  # MCP 工具注册
└── xiaozhi_biped.cc      # 主板类
```