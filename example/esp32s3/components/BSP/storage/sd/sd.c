/**
 ****************************************************************************************************
 * @file        sd.c
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

#include "storage/sd/sd.h"

sdmmc_card_t *card;                                                 /* SD / MMC卡结构 */
const char mount_point[] = MOUNT_POINT;                             /* 挂载点/根目录 */
esp_err_t ret = ESP_OK;
esp_err_t mount_ret = ESP_FAIL;

/**
 * @brief       SD卡初始化
 * @param       无
 * @retval      esp_err_t
 */
esp_err_t sd_spi_init(void)
{   
    ret = ESP_OK;

    if (MY_SD_Handle != NULL)                                       /* 再一次挂载或者初始化SD卡 */
    {
        if (mount_ret == ESP_OK)
        {
            esp_vfs_fat_sdcard_unmount(mount_point, card);          /* 取消挂载 */
            mount_ret = ESP_FAIL;
        }
    }
    else if (MY_SD_Handle == NULL)                                  /* 未初始化SPI驱动 */
    {
        my_spi_init();                                              /* 初始化SPI驱动 */
    }

    /* 文件系统挂载配置 */
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,                            /* 如果挂载失败：true会重新分区和格式化/false不会重新分区和格式化 */
        .max_files = 5,                                             /* 打开文件最大数量 */
        .allocation_unit_size = 4 * 1024 * sizeof(uint8_t)          /* 硬盘分区簇的大小 */
    };

    /* SD卡参数配置 */
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    /* SD卡引脚配置 */
    sdspi_device_config_t slot_config = {0};
    slot_config.host_id   = host.slot;
    slot_config.gpio_cs   = SD_NUM_CS;
    slot_config.gpio_cd   = GPIO_NUM_NC;
    slot_config.gpio_wp   = GPIO_NUM_NC;
    slot_config.gpio_int  = GPIO_NUM_NC;

    mount_ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);      /* 挂载文件系统 */
    ret |= mount_ret;
    vTaskDelay(pdMS_TO_TICKS(10));
    return ret;
}

/**
 * @brief       获取SD卡相关信息
 * @param       out_total_bytes：总大小
 * @param       out_free_bytes：剩余大小
 * @retval      无
 */
void sd_get_fatfs_usage(size_t *out_total_bytes, size_t *out_free_bytes)
{
    FATFS *fs;
    size_t free_clusters;
    int res = f_getfree("0:", (DWORD *)&free_clusters, &fs);
    assert(res == FR_OK);
    size_t total_sectors = (fs->n_fatent - 2) * fs->csize;
    size_t free_sectors = free_clusters * fs->csize;

    size_t sd_total = total_sectors / 1024;
    size_t sd_total_KB = sd_total * fs->ssize;
    size_t sd_free = free_sectors / 1024;
    size_t sd_free_KB = sd_free*fs->ssize;

    /* 假设总大小小于4GiB，对于SPI Flash应该为true */
    if (out_total_bytes != NULL)
    {
        *out_total_bytes = sd_total_KB;
    }
    
    if (out_free_bytes != NULL)
    {
        *out_free_bytes = sd_free_KB;
    }
}
