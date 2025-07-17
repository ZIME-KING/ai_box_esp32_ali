/**
 ****************************************************************************************************
 * @file        board.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-01
 * @brief       板级支持包接口
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

#ifndef __BOARD_H
#define __BOARD_H

#include "esp_err.h"
#include "board_config.h"

/* 错误码定义 */
#define BSP_ERROR_BASE              0x1000
#define BSP_ERROR_I2C_INIT          (BSP_ERROR_BASE + 1)
#define BSP_ERROR_SPI_INIT          (BSP_ERROR_BASE + 2)
#define BSP_ERROR_LCD_INIT          (BSP_ERROR_BASE + 3)
#define BSP_ERROR_TOUCH_INIT        (BSP_ERROR_BASE + 4)
#define BSP_ERROR_AUDIO_INIT        (BSP_ERROR_BASE + 5)
#define BSP_ERROR_SD_INIT           (BSP_ERROR_BASE + 6)
#define BSP_ERROR_XL9555_INIT       (BSP_ERROR_BASE + 7)

/* 音频系统API */
esp_err_t bsp_audio_init(void);                          // 初始化音频系统
esp_err_t bsp_audio_deinit(void);                        // 反初始化音频系统
esp_err_t bsp_audio_start_record(void);                  // 开始录音
esp_err_t bsp_audio_stop_record(void);                   // 停止录音
esp_err_t bsp_audio_start_play(void);                    // 开始播放
esp_err_t bsp_audio_stop_play(void);                     // 停止播放
esp_err_t bsp_audio_set_volume(uint8_t volume);          // 设置音量

/* 显示系统API */
esp_err_t bsp_lcd_init(void);                           // 初始化LCD
esp_err_t bsp_lcd_deinit(void);                         // 反初始化LCD
esp_err_t bsp_lcd_set_backlight(uint8_t level);          // 设置背光亮度
esp_err_t bsp_lcd_display_on(void);                     // 打开显示
esp_err_t bsp_lcd_display_off(void);                    // 关闭显示

/* 触摸系统API */
esp_err_t bsp_touch_init(void);                         // 初始化触摸屏
esp_err_t bsp_touch_deinit(void);                       // 反初始化触摸屏
esp_err_t bsp_touch_read(uint16_t *x, uint16_t *y);      // 读取触摸坐标

/* 存储系统API */
esp_err_t bsp_sd_init(void);                            // 初始化SD卡
esp_err_t bsp_sd_deinit(void);                          // 反初始化SD卡
esp_err_t bsp_sd_is_present(void);                      // 检查SD卡是否插入

/* 按键系统API */
esp_err_t bsp_key_init(void);                           // 初始化按键
esp_err_t bsp_key_deinit(void);                         // 反初始化按键
uint8_t bsp_key_scan(uint8_t mode);                      // 扫描按键

/* IO扩展API */
esp_err_t bsp_xl9555_init(void);                        // 初始化XL9555
esp_err_t bsp_xl9555_deinit(void);                      // 反初始化XL9555
esp_err_t bsp_xl9555_write_pin(uint8_t pin, uint8_t level); // 写IO引脚
esp_err_t bsp_xl9555_read_pin(uint8_t pin, uint8_t *level); // 读IO引脚

/* 板级初始化API */
esp_err_t bsp_board_init(void);                         // 初始化开发板
esp_err_t bsp_board_deinit(void);                       // 反初始化开发板

#endif /* __BOARD_H */