/**
 ****************************************************************************************************
 * @file        i2c.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-01
 * @brief       I2C驱动代码
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

#ifndef __I2C_H
#define __I2C_H

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

/* 引脚与相关参数定义 */
#define IIC_NUM_PORT       I2C_NUM_0        /* IIC0 */
#define IIC_SPEED_CLK      400000           /* 速率400K */
#define IIC_SDA_GPIO_PIN   GPIO_NUM_41      /* IIC0_SDA引脚 */
#define IIC_SCL_GPIO_PIN   GPIO_NUM_42      /* IIC0_SCL引脚 */

typedef i2c_master_bus_handle_t i2c_handle_t;  /* I2C句柄类型定义 */

extern i2c_handle_t bus_handle;  /* 总线句柄 */

/* 函数声明 */
esp_err_t i2c_init(void);                 /* 初始化I2C */
i2c_handle_t i2c_get_handle(void);        /* 获取I2C句柄 */
esp_err_t i2c_read_bytes(i2c_handle_t handle, uint8_t addr, uint8_t reg, uint8_t* data, size_t len);  /* 读取数据 */
esp_err_t i2c_write_bytes(i2c_handle_t handle, uint8_t addr, uint8_t reg, uint8_t* data, size_t len); /* 写入数据 */

#endif
