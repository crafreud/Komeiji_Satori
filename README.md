# Komeiji Satori - ESP32-S3 智能眼球项目

> 基于 ESP32-S3 芯片的智能眼球系统，实现人脸识别、手指跟踪、光流运动检测及舵机自动跟踪控制。

## 项目概述

本项目使用 ESP32-S3 开发板，通过摄像头采集图像，结合多种视觉算法（人脸检测、肤色识别、光流跟踪、运动检测），驱动 4 路舵机实现眼球的智能跟踪运动。系统支持 WiFi 热点模式，可通过网页实时查看摄像头画面。

## 主要功能

### 核心能力

| 功能 | 说明 |
|------|------|
| 人脸检测与跟踪 | 基于 ESP-DL 的人脸检测模型（MSR01 + MNP01），PID 控制平滑跟踪 |
| 手指跟踪 | 肤色阈值检测，支持面积/尺寸过滤，比例控制舵机跟随 |
| 光流运动检测 | Lucas-Kanade 稀疏光流算法，Harris 角点检测，运动估计与补偿 |
| 运动检测 | 帧差法检测运动物体，支持运动区域白色替换 |
| 随机动作系统 | 24 种预设动作组合（波浪、圆形扫描、星形轨迹、呼吸等），空闲时自动触发 |
| 目标仲裁 | 优先级：人脸 > 运动物体 > 空闲随机动作 |
| WiFi 视频流 | AP 模式 + MJPEG 实时视频流，内置 Web 界面 |

### 硬件配置

| 组件 | 型号/参数 |
|------|-----------|
| 主控芯片 | ESP32-S3 |
| 摄像头 | QVGA 320×240，RGB565 格式 |
| 舵机 | 4 路 PWM 舵机（50Hz，500~2500μs 脉宽） |
| 舵机引脚 | GPIO 1, 2, 4, 10 |
| WiFi | AP 模式，SSID: `ESP32_Camera`，密码: `12345678` |
| 访问地址 | http://192.168.4.1 |

## 项目结构

```
Komeiji_Satori/
├── 01_camera_for_trac/          # 摄像头跟踪系统（肤色检测 + 光流 + 舵机控制）
│   ├── main/
│   │   ├── 01_camera_for_trac.c       # 主程序：色块跟踪、光流跟踪、任务调度
│   │   ├── camera_color_detection.c/h # 摄像头初始化、颜色检测、色块识别
│   │   ├── optical_flow.c/h           # 稀疏光流算法（Lucas-Kanade）
│   │   ├── servo.c/h                  # 4路舵机PWM控制
│   │   └── wifi_server.c/h            # WiFi热点 + HTTP服务器 + MJPEG流
│   ├── README_finger_tracking.md      # 手指跟踪功能详细说明
│   └── README_optical_flow.md         # 光流算法集成说明
│
├── 02_camera_opencv_test/       # OpenCV 集成测试
│   └── main/
│       └── 02_camera_opencv_test.c    # 基于 OpenCV 的运动检测测试
│
├── 03_face_test/                # 硬件驱动测试（I2C、传感器、摄像头）
│   └── main/
│       ├── esp32_s3_szp.c/h           # I2C、PCA9557 IO扩展、QMI8658 IMU、摄像头初始化
│       └── ...
│
├── 04_final_test/               # 最终集成版本（人脸检测 + 舵机跟踪 + 随机动作）
│   └── main/
│       ├── 04_final_test.cpp          # 主程序：人脸/运动检测、PID控制、24种随机动作
│       ├── who_human_face_detection.* # ESP-DL 人脸检测封装
│       └── who_ai_utils.*             # AI 工具函数
│
└── examples/                    # OpenCV 示例程序
    ├── features2d/              # 特征点检测
    ├── motion_detection/        # 运动检测
    ├── object_tracking/         # 目标跟踪
    ├── people_detection/        # 行人检测
    └── text_area_detection/     # 文本区域检测
```

## 快速开始

### 环境要求

- ESP-IDF v5.x 或更高版本
- ESP32-S3 开发板（带摄像头）
- 4 路舵机（可选）

### 编译与烧录

选择一个子项目进行编译：

```bash
# 以 04_final_test（最终版本）为例
cd 04_final_test
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

### 运行模式

| 子项目 | 功能 | 编译说明 |
|--------|------|----------|
| `01_camera_for_trac` | 肤色跟踪 + 光流 + WiFi 视频流 | 定义 `TEST_MODE` 可进入测试模式 |
| `02_camera_opencv_test` | OpenCV 运动检测测试 | 需要 OpenCV 组件 |
| `03_face_test` | 硬件驱动测试 | 基础摄像头和外设测试 |
| `04_final_test` | 人脸检测 + 舵机跟踪 + 随机动作 | 需要 ESP-DL 组件 |

## 技术细节

### 舵机控制

- **PWM 频率**：50Hz（周期 20ms）
- **脉宽范围**：500μs（0°）~ 2500μs（180°）
- **分辨率**：14 位
- **通道分配**：
  - 通道 1（GPIO 1）：辅助舵机
  - 通道 2（GPIO 2）：垂直方向
  - 通道 3（GPIO 4）：水平方向
  - 通道 4（GPIO 10）：备用

### 人脸检测（04_final_test）

- 使用 ESP-DL 的两级检测器：`HumanFaceDetectMSR01`（粗检测）+ `HumanFaceDetectMNP01`（精检测）
- PID 控制器参数：Kp=0.8, Ki=0.0, Kd=0.1
- 空闲 30 秒或每 2 分钟强制触发随机动作
- 垂直舵机接近边界时触发辅助舵机联动

### 光流算法（01_camera_for_trac）

- **算法**：Lucas-Kanade 稀疏光流
- **特征检测**：Harris 角点检测器
- **最大特征点**：20 个
- **金字塔层数**：2 层
- **窗口大小**：5×5 像素
- **收敛阈值**：0.1

### 随机动作系统（04_final_test）

系统内置 24 种随机动作组合，包括：

| 动作 | 描述 |
|------|------|
| 水平/垂直摆动 | 基础摆动动作 |
| 波浪动作 | 多舵机协调波浪运动 |
| 圆形扫描 | 三轴协调圆形轨迹 |
| 震动模式 | 快速小幅震动后减速 |
| 聚焦动作 | 从外围逐渐收缩到中心 |
| 星形轨迹 | 五角星运动路径 |
| 搜索模式 | 模拟雷达扫描 |
| 螺旋动作 | 从中心向外螺旋运动 |
| Z字形/8字形 | 几何轨迹运动 |
| 钟摆/呼吸 | 自然模拟运动 |

## WiFi 访问

连接 ESP32 热点后访问 http://192.168.4.1：

- **`/`** — Web 控制页面，显示实时视频流
- **`/camera`** — 单帧 JPEG 图像
- **`/stream`** — MJPEG 实时视频流

## 许可证

本项目仅供学习和个人使用。