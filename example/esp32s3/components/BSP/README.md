# BSP (Board Support Package)

## 目录结构

```
BSP/
├── audio/                  # 音频相关驱动
│   ├── codec/              # 音频编解码器
│   │   ├── es8388.c
│   │   └── es8388.h
│   └── i2s/                # I2S接口
│       ├── i2s.c
│       └── i2s.h
├── display/                # 显示相关驱动
│   ├── lcd/                # LCD驱动
│   │   ├── lcd.c
│   │   ├── lcd.h
│   │   └── lcdfont.h
│   └── touch/              # 触摸屏驱动
│       ├── touch.c
│       └── touch.h
├── storage/                # 存储相关驱动
│   └── sd/                 # SD卡驱动
│       ├── sd.c
│       └── sd.h
├── input/                  # 输入设备驱动
│   └── key/                # 按键驱动
│       ├── key.c
│       └── key.h
├── bus/                    # 总线驱动
│   ├── i2c/                # I2C总线
│   │   ├── i2c.c
│   │   └── i2c.h
│   └── spi/                # SPI总线
│       ├── spi.c
│       └── spi.h
├── io/                     # IO扩展驱动
│   └── xl9555/             # XL9555 IO扩展芯片
│       ├── xl9555.c
│       └── xl9555.h
├── board/                  # 开发板配置
│   ├── board_config.h      # 硬件配置
│   └── board.h             # 板级支持包接口
└── CMakeLists.txt          # 构建配置文件
```

## 功能说明

### 音频系统
- ES8388音频编解码器驱动
- I2S音频接口驱动

### 显示系统
- LCD显示驱动
- 触摸屏控制

### 存储系统
- SD卡驱动

### 输入系统
- 按键驱动

### 总线系统
- I2C总线驱动
- SPI总线驱动

### IO扩展
- XL9555 IO扩展芯片驱动

### 板级配置
- 硬件引脚定义
- 系统时钟配置
- 外设默认配置

## 使用说明

1. 所有硬件相关的配置都集中在 `board/board_config.h` 中管理
2. 各个模块都提供了标准的初始化和控制接口
3. 使用时先调用相应模块的初始化函数
4. 遵循模块化设计，各个驱动之间耦合度低

## 依赖关系

- driver: ESP-IDF驱动层
- esp_lcd: ESP-IDF LCD驱动支持
- fatfs: FAT文件系统支持