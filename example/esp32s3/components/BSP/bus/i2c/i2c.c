/**
 ****************************************************************************************************
 * @file        i2c.c
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

#include "i2c.h"
#include <string.h>
#include "freertos/FreeRTOS.h"

i2c_handle_t bus_handle = NULL;     /* 总线句柄 */

/**
 * @brief       初始化I2C
 * @param       无
 * @retval      ESP_OK:初始化成功
 */
esp_err_t i2c_init(void)
{
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source                     = I2C_CLK_SRC_DEFAULT,  /* 时钟源 */
        .i2c_port                       = IIC_NUM_PORT,         /* I2C端口 */
        .scl_io_num                     = IIC_SCL_GPIO_PIN,     /* SCL管脚 */
        .sda_io_num                     = IIC_SDA_GPIO_PIN,     /* SDA管脚 */
        .glitch_ignore_cnt              = 7,                    /* 故障周期 */
        .flags.enable_internal_pullup   = true,                 /* 内部上拉 */
    };
    /* 新建I2C总线 */
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

    return ESP_OK;
}

/**
 * @brief       获取I2C句柄
 * @param       无
 * @retval      I2C句柄
 */
i2c_handle_t i2c_get_handle(void)
{
    return bus_handle;
}

/**
 * @brief       从I2C设备读取数据
 * @param       handle: I2C句柄
 * @param       addr: 设备地址
 * @param       reg: 寄存器地址
 * @param       data: 数据缓冲区
 * @param       len: 数据长度
 * @retval      ESP_OK: 读取成功
 */
esp_err_t i2c_read_bytes(i2c_handle_t handle, uint8_t addr, uint8_t reg, uint8_t* data, size_t len)
{
    uint8_t write_buf[1] = {reg};
 //   ESP_ERROR_CHECK(i2c_master_transmit_receive(handle, addr, write_buf, 1, data, len, pdMS_TO_TICKS(100)));
    return ESP_OK;
}

/**
 * @brief       向I2C设备写入数据
 * @param       handle: I2C句柄
 * @param       addr: 设备地址
 * @param       reg: 寄存器地址
 * @param       data: 数据缓冲区
 * @param       len: 数据长度
 * @retval      ESP_OK: 写入成功
 */
esp_err_t i2c_write_bytes(i2c_handle_t handle, uint8_t addr, uint8_t reg, uint8_t* data, size_t len)
{
    uint8_t write_buf[len + 1];
    write_buf[0] = reg;
    memcpy(write_buf + 1, data, len);
//    ESP_ERROR_CHECK(i2c_master_transmit_receive(handle, addr, write_buf, len + 1, pdMS_TO_TICKS(100)));
    return ESP_OK;
}
