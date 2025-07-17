# 设置最小CMake版本要求
cmake_minimum_required(VERSION 3.5)

# 添加源文件
set(SOURCES
    "audio/codec/es8388.c"
    "audio/codec/es8311.c"
    "audio/i2s/i2s.c"
    "display/lcd/lcd.c"
    "display/touch/touch.c"
    "storage/sd/sd.c"
    "input/key/key.c"
    "bus/i2c/myi2c.c"
    "bus/spi/spi.c"
    "io/xl9555/xl9555.c"
)

# 添加头文件路径
set(INCLUDE_DIRS
    "."
    "board"
    "audio/codec"
    "audio/i2s"
    "display/lcd"
    "display/touch"
    "storage/sd"
    "input/key"
    "bus/i2c"
    "bus/spi"
    "io/xl9555"
)

# 注册组件
idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    REQUIRES driver esp_lcd fatfs
)