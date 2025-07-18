/**
 ****************************************************************************************************
 * @file        sd.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-01
 * @brief       SD卡 驱动代码
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

#ifndef __SD_H
#define __SD_H

#include <unistd.h>
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "bus/spi/spi.h"


/* 引脚定义 */
#define SD_NUM_CS       GPIO_NUM_2
#define MOUNT_POINT     "/0:"

/* 函数声明 */
esp_err_t sd_spi_init(void);    /* SD卡初始化 */
void sd_get_fatfs_usage(size_t *out_total_bytes, size_t *out_free_bytes);
#endif
