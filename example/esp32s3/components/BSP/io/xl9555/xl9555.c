/**
 ****************************************************************************************************
 * @file        xl9555.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-01
 * @brief       XL9555驱动代码
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

#include "xl9555.h"

const char *xl9555_tag = "xl9555";
i2c_handle_t xl9555_handle = NULL;

esp_err_t xl9555_read_byte(uint8_t *data, size_t len)
{
    uint8_t reg_addr = XL9555_INPUT_PORT0_REG;
    
    return i2c_read_bytes(xl9555_handle, XL9555_ADDR, reg_addr, data, len);
}

esp_err_t xl9555_write_byte(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_write_bytes(xl9555_handle, XL9555_ADDR, reg, data, len);
}

/**
 * @brief       控制某个IO的电平
 * @param       pin     : 控制的IO
 * @param       val     : 电平
 * @retval      返回所有IO状态
 */
uint16_t xl9555_pin_write(uint16_t pin, int val)
{
    uint8_t w_data[2];
    uint16_t temp = 0x0000;

    xl9555_read_byte(w_data, 2);

    if (pin <= 0x0080)
    {
        if (val)
        {
            w_data[0] |= (uint8_t)(0xFF & pin);
        }
        else
        {
            w_data[0] &= ~(uint8_t)(0xFF & pin);
        }
    }
    else
    {
        if (val)
        {
            w_data[1] |= (uint8_t)(0xFF & (pin >> 8));
        }
        else
        {
            w_data[1] &= ~(uint8_t)(0xFF & (pin >> 8));
        }
    }

    temp = ((uint16_t)w_data[1] << 8) | w_data[0]; 

    xl9555_write_byte(XL9555_OUTPUT_PORT0_REG, w_data, 2);
    
    return temp;
}

/**
 * @brief       获取某个IO状态
 * @param       pin : 要获取状态的IO
 * @retval      此IO口的值(状态, 0/1)
 */
int xl9555_pin_read(uint16_t pin)
{
    uint16_t ret;
    uint8_t r_data[2];

    xl9555_read_byte(r_data, 2);

    ret = r_data[1] << 8 | r_data[0];

    return (ret & pin) ? 1 : 0;
}

/**
 * @brief       XL9555的IO配置
 * @param       config_value：IO配置输入或者输出
 * @retval      返回设置的数值
 */
void xl9555_ioconfig(uint16_t config_value)
{
    /* 从机地址 + CMD + data1(P0) + data2(P1) */
    /* P10、P11、P12、P13和P14为输入，其他引脚为输出 -->0001 1111 0000 0000 注意：0为输出，1为输入*/
    uint8_t data[2];
    esp_err_t err;

    data[0] = (uint8_t)(0xFF & config_value);
    data[1] = (uint8_t)(0xFF & (config_value >> 8));

    do
    {
        err = xl9555_write_byte(XL9555_CONFIG_PORT0_REG, data, 2);
        if (err != ESP_OK)
        {
            ESP_LOGE(xl9555_tag, "%s configure %X failed, ret: %d", __func__, config_value, err);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
        
    } while (err != ESP_OK);
}

/**
 * @brief       外部中断服务函数
 * @param       arg：中断引脚号
 * @note        IRAM_ATTR: 这里的IRAM_ATTR属性用于将中断处理函数存储在内部RAM中，目的在于减少延迟
 * @retval      无
 */
static void IRAM_ATTR xl9555_exit_gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    
    if (gpio_num == XL9555_INT_IO)
    {
        esp_rom_delay_us(20000);

        if (gpio_get_level(XL9555_INT_IO) == 0)
        {
            /* 中断处理 */
            // ESP_DRAM_LOGI("xl9555", "xl9555 int");
        }
    }
}

/**
 * @brief       外部中断初始化程序
 * @param       无
 * @retval      无
 */
void xl9555_int_init(void)
{
    gpio_config_t gpio_init_struct;

    /* 配置XL9555器件的INT中断引脚 */
    gpio_init_struct.mode         = GPIO_MODE_INPUT;        /* 选择为输入模式 */
    gpio_init_struct.pull_up_en   = GPIO_PULLUP_ENABLE;     /* 上拉使能 */
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;  /* 下拉失能 */
    gpio_init_struct.intr_type    = GPIO_INTR_NEGEDGE;      /* 下降沿触发 */
    gpio_init_struct.pin_bit_mask = 1ull << XL9555_INT_IO;  /* 设置的引脚的位掩码 */
    gpio_config(&gpio_init_struct);                         /* 配置使能 */
    
    /* 注册中断服务 */
    gpio_install_isr_service(0);
    
    /* 设置GPIO的中断回调函数 */
    gpio_isr_handler_add(XL9555_INT_IO, xl9555_exit_gpio_isr_handler, (void*)XL9555_INT_IO);
}

/**
 * @brief       初始化XL9555
 * @param       无
 * @retval      ESP_OK:初始化成功
 */
esp_err_t xl9555_init(void)
{
    uint8_t r_data[2];

    /* 初始化I2C总线 */
    if (xl9555_handle == NULL)
    {
        ESP_ERROR_CHECK(i2c_init());
        xl9555_handle = i2c_get_handle();
    }

    /* 上电先读取一次清除中断标志 */
    xl9555_read_byte(r_data, 2);
    /* 配置那些扩展管脚为输入输出模式 */
    xl9555_ioconfig(0xF003);
    /* 关闭蜂鸣器 */
    xl9555_pin_write(BEEP_IO, 1);
    /* 关闭喇叭 */
    xl9555_pin_write(SPK_EN_IO, 1);

    return ESP_OK;
}

/**
 * @brief       按键扫描函数
 * @param       mode:0->不连续;1->连续
 * @retval      键值, 定义如下:
 *              KEY0_PRES, 1, KEY0按下
 *              KEY1_PRES, 2, KEY1按下
 *              KEY2_PRES, 3, KEY2按下
 *              KEY3_PRES, 4, KEY3按下
 */
uint8_t xl9555_key_scan(uint8_t mode)
{
    uint8_t keyval = 0;
    static uint8_t key_up = 1;                                          /* 按键按松开标志 */

    if (mode)
    {
        key_up = 1;                                                     /* 支持连按 */
    }
    
    if (key_up && (KEY0 == 0 || KEY1 == 0 || KEY2 == 0  || KEY3 == 0 )) /* 按键松开标志为1, 且有任意一个按键按下了 */
    {
        esp_rom_delay_us(100000);                                  /* 去抖动 */
        key_up = 0;

        if (KEY0 == 0)
        {
            keyval = KEY0_PRES;
        }

        if (KEY1 == 0)
        {
            keyval = KEY1_PRES;
        }

        if (KEY2 == 0)
        {
            keyval = KEY2_PRES;
        }

        if (KEY3 == 0)
        {
            keyval = KEY3_PRES;
        }
    }
    else if (KEY0 == 1 && KEY1 == 1 && KEY2 == 1 && KEY3 == 1)          /* 没有任何按键按下, 标记按键松开 */
    {
        key_up = 1;
    }

    return keyval;                                                      /* 返回键值 */
}
