/**
 ****************************************************************************************************
 * @file        text.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-01
 * @brief       汉字显示 代码
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

#ifndef __TEXT_H
#define __TEXT_H

#include "esp_log.h"
#include "esp_partition.h"
#include "ff.h"
#include "fonts.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "spi_flash_mmap.h"
#include "lcd.h"

void text_show_char(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                    char *str, uint8_t str_len, uint8_t size, uint8_t mode,
                    uint32_t color); /* 显示单个汉字字符 */

#endif