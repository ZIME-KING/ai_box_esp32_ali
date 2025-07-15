/**
 ****************************************************************************************************
 * @file        board_config.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-01
 * @brief       开发板硬件配置
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 ESP32-S3 开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#ifndef __BOARD_CONFIG_H
#define __BOARD_CONFIG_H

#include "driver/gpio.h"

/* 音频配置 */
#define AUDIO_INPUT_SAMPLE_RATE     16000  // 音频采样率

/* I2S麦克风配置 */
#define AUDIO_I2S_MIC_GPIO_BCLK     GPIO_NUM_41  // I2S BCLK
#define AUDIO_I2S_MIC_GPIO_LRCK     GPIO_NUM_42  // I2S LRCK
#define AUDIO_I2S_MIC_GPIO_DIN      GPIO_NUM_2   // I2S DIN

/* I2S扬声器配置 */
#define AUDIO_I2S_SPK_GPIO_BCLK     GPIO_NUM_41  // I2S BCLK
#define AUDIO_I2S_SPK_GPIO_LRCK     GPIO_NUM_42  // I2S LRCK
#define AUDIO_I2S_SPK_GPIO_DOUT     GPIO_NUM_1   // I2S DOUT

/* I2C配置 */
#define I2C_SCL_IO                  GPIO_NUM_41  // I2C SCL
#define I2C_SDA_IO                  GPIO_NUM_42  // I2C SDA
#define I2C_FREQ                    400000      // I2C频率
#define I2C_ADDRESS                 0x10        // I2C设备地址

/* LCD显示配置 */
#define LCD_H_RES                   240         // LCD水平分辨率
#define LCD_V_RES                   320         // LCD垂直分辨率

/* QSPI LCD配置 */
#define LCD_QSPI_SCK                GPIO_NUM_12  // QSPI时钟
#define LCD_QSPI_CS                 GPIO_NUM_11  // QSPI片选
#define LCD_QSPI_D0                 GPIO_NUM_13  // QSPI数据0
#define LCD_QSPI_D1                 GPIO_NUM_14  // QSPI数据1
#define LCD_QSPI_D2                 GPIO_NUM_15  // QSPI数据2
#define LCD_QSPI_D3                 GPIO_NUM_16  // QSPI数据3

/* 显示偏移配置 */
#define LCD_OFFSET_X                0           // X轴偏移
#define LCD_OFFSET_Y                0           // Y轴偏移

/* 触摸屏配置 */
#define TOUCH_I2C_SCL               GPIO_NUM_41  // 触摸屏I2C SCL
#define TOUCH_I2C_SDA               GPIO_NUM_42  // 触摸屏I2C SDA
#define TOUCH_I2C_INT               GPIO_NUM_3   // 触摸屏中断

/* 背光配置 */
#define LCD_BL_GPIO                 GPIO_NUM_4   // 背光控制引脚

/* 按键配置 */
#define KEY1_GPIO                   GPIO_NUM_0   // 按键1
#define KEY2_GPIO                   GPIO_NUM_14  // 按键2

/* SD卡SPI配置 */
#define SD_SPI_MISO                 GPIO_NUM_37  // SD卡MISO
#define SD_SPI_MOSI                 GPIO_NUM_35  // SD卡MOSI
#define SD_SPI_SCK                  GPIO_NUM_36  // SD卡SCK
#define SD_SPI_CS                   GPIO_NUM_34  // SD卡CS

/* XL9555 IO扩展配置 */
#define XL9555_I2C_ADDR             0x20        // XL9555地址
#define XL9555_INT_GPIO             GPIO_NUM_5   // XL9555中断引脚

#endif /* __BOARD_CONFIG_H */