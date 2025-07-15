/**
 ****************************************************************************************************
 * @file        touch.c
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

#include "touch.h"
#include "board_config.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define TAG "TOUCH"

/* I2C配置 */
static i2c_config_t touch_i2c_conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = TOUCH_I2C_SDA,
    .scl_io_num = TOUCH_I2C_SCL,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 400000
};

/* 触摸屏初始化 */
esp_err_t bsp_touch_init(void)
{
    esp_err_t ret;

    /* 配置I2C */
    ret = i2c_param_config(I2C_NUM_0, &touch_i2c_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C参数配置失败");
        return ret;
    }

    /* 安装I2C驱动 */
    ret = i2c_driver_install(I2C_NUM_0, touch_i2c_conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C驱动安装失败");
        return ret;
    }

    /* 配置中断引脚 */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TOUCH_I2C_INT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO配置失败");
        return ret;
    }

    ESP_LOGI(TAG, "触摸屏初始化成功");
    return ESP_OK;
}

/* 触摸屏反初始化 */
esp_err_t bsp_touch_deinit(void)
{
    esp_err_t ret;

    /* 卸载I2C驱动 */
    ret = i2c_driver_delete(I2C_NUM_0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C驱动卸载失败");
        return ret;
    }

    ESP_LOGI(TAG, "触摸屏反初始化成功");
    return ESP_OK;
}

/* 读取触摸坐标 */
esp_err_t bsp_touch_read(uint16_t *x, uint16_t *y)
{
    /* TODO: 实现具体的触摸坐标读取逻辑 */
    return ESP_OK;
}