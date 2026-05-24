# LineWalker v5.0

基于 STM32F103C8T6 的八路循迹小车，PID 实时纠偏 + 蓝牙实时调参 + Flutter App 控制。

## 硬件

| 模块 | 型号 | 引脚 |
|---|---|---|
| MCU | STM32F103C8T6 | — |
| 巡线 | 8路 I2C 红外 | PB6 SCL / PB7 SDA |
| 电机驱动 | TB6612 | PA1 左 PWM / PA2 右 PWM |
| 方向控制 | — | PB14 IN1 / PB15 IN2 / PA6 IN3 / PA7 IN4 |
| 避障 | 2路红外 | PC14 右前 / PC15 左前 |
| 蓝牙 | HC-05 | PA9 TX / PA10 RX (9600 8N1) |
| 编码器 | 2路红外 | PA4 左 / PA5 右 |
| OLED | SSD1306 I2C | PB8 SCL / PB9 SDA |
| 蜂鸣器 | 有源 | PA8 |

## 目录结构

```
├── Hardware/          # 外设驱动
│   ├── LineWalking.c  # 循迹核心 (PID + 状态机 + 锁)
│   ├── Bluetooth.c    # 蓝牙模式切换 + OLED 显示
│   ├── BluetoothCmd.c # 文本命令解析 (PID 调参 / 保存 / 预设)
│   ├── EightWayTrack.c # 8路 I2C 传感器读取
│   ├── Encoder.c      # 编码器测速
│   ├── FlashStore.c   # Flash 参数持久化
│   ├── Motor.c        # TB6612 电机 PWM
│   ├── OLED.c         # SSD1306 驱动
│   ├── PID.c          # PID 控制器
│   └── ...
├── User/              # 入口 + 中断
├── Library/           # STM32 标准外设库
├── Start/             # 启动文件
├── System/            # 延时
├── App/flutter/       # Flutter 控制 App
└── Project.uvprojx    # Keil MDK 工程
```

## 编译

### 固件 (STM32)

用 Keil MDK-ARM V5 打开 `Project.uvprojx` → Build。

产物：`Objects/Project.hex`

### App (Android)

```bash
cd App/flutter
flutter pub get
flutter build apk --debug
```

产物：`App/flutter/build/app/outputs/flutter-apk/app-debug.apk`

## 循迹核心策略

1. **锐角 (P1)** — 两段黑线中间有白缝，进入状态机：直冲 → 原地旋转找另一边
2. **直角 (P2)** — 线在最边、对面边白，锁方向记忆，不直接控制车轮
3. **质心法 (P4)** — 黑探头平均位置 − 3.5 = 偏差，死区 ±0.2，PID 差速驱动

## 速度换算

```
速度 = 轮子转速(cm/s) × 10

例: SP=55 → 实际约 5.5 cm/s
    app 编码器页显示的 cm/s = 固件值 / 10
```

## 蓝牙通信协议

9600 8N1，`\r\n` 结尾。App 连接后自动下发 `SET?` 同步参数。

### PID 调参
```
KP=18     KI=0.30    KD=12     SP=45
RSP=45    TL=7       TIN=25    TOUT=50
```

### 预设
```
PR1  保守档  KP=18 KI=0.30 KD=12 SP=45 RSP=45 TL=7
PR2  默认档  KP=25 KI=0.15 KD=30 SP=55 RSP=55 TL=6
PR3  激进档  KP=32 KI=0.20 KD=14 SP=60 RSP=60 TL=5
```

### 其他
```
SAVE   保存当前全部参数到 Flash
SET?   查询全部配置
O=0/1  关闭/开启避障
```

## App 功能

- **传感器页** — 8路巡线 + 2路避障可视化 + 编码器速度/里程 + 状态中文显示
- **调试页** — PID/KI/KD/SP/转弯速度/锁存轮数 滚轮调参 + 三档预设 + 保存
- **控制页** — 循迹按钮 + 方向控制 + 原地旋转
- **日志页** — 实时状态日志 + 复制/清空
- **设置页** — 主题/颜色/震动/引脚对照

## License

MIT
