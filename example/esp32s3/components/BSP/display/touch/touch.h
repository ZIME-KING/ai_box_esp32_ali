/**
 ****************************************************************************************************
 * @file        touch.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-01
 * @brief       触摸屏驱动代码
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

#ifndef __TOUCH_H
#define __TOUCH_H

#include "esp_err.h"
#include <stdint.h>

/* 触摸屏初始化 */
esp_err_t bsp_touch_init(void);

/* 触摸屏反初始化 */
esp_err_t bsp_touch_deinit(void);

/* 读取触摸坐标 */
esp_err_t bsp_touch_read(uint16_t *x, uint16_t *y);

#endif /* __TOUCH_H */