/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "es8311.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"

#include "es8311_reg.h"
#include "myi2c.h"

i2c_master_dev_handle_t es8311_handle = NULL;
/*
 * Clock coefficient structure
 */
struct _coeff_div
{
    uint32_t mclk;     /* mclk frequency */
    uint32_t rate;     /* sample rate */
    uint8_t pre_div;   /* the pre divider with range from 1 to 8 */
    uint8_t pre_multi; /* the pre multiplier with 0: 1x, 1: 2x, 2: 4x, 3: 8x selection */
    uint8_t adc_div;   /* adcclk divider */
    uint8_t dac_div;   /* dacclk divider */
    uint8_t fs_mode;   /* double speed or single speed, =0, ss, =1, ds */
    uint8_t lrck_h;    /* adclrck divider and daclrck divider */
    uint8_t lrck_l;
    uint8_t bclk_div; /* sclk divider */
    uint8_t adc_osr;  /* adc osr */
    uint8_t dac_osr;  /* dac osr */
};

/* codec hifi mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
    /*!<mclk     rate   pre_div  mult  adc_div dac_div fs_mode lrch  lrcl  bckdiv osr */
    /* 8k */
    {12288000, 8000, 0x06, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 8000, 0x03, 0x01, 0x03, 0x03, 0x00, 0x05, 0xff, 0x18, 0x10, 0x10},
    {16384000, 8000, 0x08, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000, 8000, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 8000, 0x03, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {4096000, 8000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 8000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2048000, 8000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 8000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1024000, 8000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 11.025k */
    {11289600, 11025, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800, 11025, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400, 11025, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200, 11025, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 12k */
    {12288000, 12000, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 12000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 12000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 12000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 16k */
    {12288000, 16000, 0x03, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 16000, 0x03, 0x01, 0x03, 0x03, 0x00, 0x02, 0xff, 0x0c, 0x10, 0x10},
    {16384000, 16000, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000, 16000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 16000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {4096000, 16000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 16000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2048000, 16000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 16000, 0x03, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1024000, 16000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 22.05k */
    {11289600, 22050, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800, 22050, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400, 22050, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200, 22050, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {705600, 22050, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 24k */
    {12288000, 24000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 24000, 0x03, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 24000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 24000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 24000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 32k */
    {12288000, 32000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 32000, 0x03, 0x02, 0x03, 0x03, 0x00, 0x02, 0xff, 0x0c, 0x10, 0x10},
    {16384000, 32000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000, 32000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 32000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {4096000, 32000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 32000, 0x03, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2048000, 32000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 32000, 0x03, 0x03, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},
    {1024000, 32000, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 44.1k */
    {11289600, 44100, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800, 44100, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400, 44100, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200, 44100, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 48k */
    {12288000, 48000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 48000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 48000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 48000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 48000, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 64k */
    {12288000, 64000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 64000, 0x03, 0x02, 0x03, 0x03, 0x01, 0x01, 0x7f, 0x06, 0x10, 0x10},
    {16384000, 64000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000, 64000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 64000, 0x01, 0x02, 0x03, 0x03, 0x01, 0x01, 0x7f, 0x06, 0x10, 0x10},
    {4096000, 64000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 64000, 0x01, 0x03, 0x03, 0x03, 0x01, 0x01, 0x7f, 0x06, 0x10, 0x10},
    {2048000, 64000, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 64000, 0x01, 0x03, 0x01, 0x01, 0x01, 0x00, 0xbf, 0x03, 0x18, 0x18},
    {1024000, 64000, 0x01, 0x03, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},

    /* 88.2k */
    {11289600, 88200, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800, 88200, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400, 88200, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200, 88200, 0x01, 0x03, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},

    /* 96k */
    {12288000, 96000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 96000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000, 96000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000, 96000, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000, 96000, 0x01, 0x03, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},
};

static const char *TAG = "ES8311";

esp_err_t es8311_write_reg(uint8_t reg_addr, uint8_t data)
{
    esp_err_t ret;
    uint8_t *buf = malloc(2);
    if (buf == NULL)
    {
        ESP_LOGE(TAG, "%s memory failed", __func__);
        return ESP_ERR_NO_MEM; /* 分配内存失败 */
    }
    buf[0] = reg_addr;
    buf[1] = data; /* 拷贝数据至存储区当中 */
    do
    {
        i2c_master_bus_wait_all_done(bus_handle, 1000);
        ret = i2c_master_transmit(es8311_handle, buf, 2, 1000);
    } while (ret != ESP_OK);

    free(buf); /* 发送完成释放内存 */

    return ret;
}
esp_err_t es8311_read_reg(uint8_t reg_addr, uint8_t *reg_value)
{
    uint8_t ret = 0;
    ret=i2c_master_transmit_receive(es8311_handle, &reg_addr, 1, reg_value, 1, -1);
    return ret;
}
/*
 * look for the coefficient in coeff_div[] table
 */
static int get_coeff(uint32_t mclk, uint32_t rate)
{
    for (int i = 0; i < (sizeof(coeff_div) / sizeof(coeff_div[0])); i++)
    {
        if (coeff_div[i].rate == rate && coeff_div[i].mclk == mclk)
        {
            return i;
        }
    }

    return -1;
}
esp_err_t es8311_sample_frequency_config(int mclk_frequency, int sample_frequency)
{
    uint8_t regv;

    /* Get clock coefficients from coefficient table */
    int coeff = get_coeff(mclk_frequency, sample_frequency);

    if (coeff < 0)
    {
        ESP_LOGE(TAG, "Unable to configure sample rate %dHz with %dHz MCLK", sample_frequency, mclk_frequency);
        return ESP_ERR_INVALID_ARG;
    }

    const struct _coeff_div *const selected_coeff = &coeff_div[coeff];

    /* register 0x02 */
    ESP_RETURN_ON_ERROR(es8311_read_reg(ES8311_CLK_MANAGER_REG02, &regv), TAG, "I2C read/write error");
    regv &= 0x07;
    regv |= (selected_coeff->pre_div - 1) << 5;
    regv |= selected_coeff->pre_multi << 3;
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_CLK_MANAGER_REG02, regv), TAG, "I2C read/write error");

    /* register 0x03 */
    const uint8_t reg03 = (selected_coeff->fs_mode << 6) | selected_coeff->adc_osr;
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_CLK_MANAGER_REG03, reg03), TAG, "I2C read/write error");

    /* register 0x04 */
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_CLK_MANAGER_REG04, selected_coeff->dac_osr), TAG, "I2C read/write error");

    /* register 0x05 */
    const uint8_t reg05 = ((selected_coeff->adc_div - 1) << 4) | (selected_coeff->dac_div - 1);
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_CLK_MANAGER_REG05, reg05), TAG, "I2C read/write error");

    /* register 0x06 */
    ESP_RETURN_ON_ERROR(es8311_read_reg(ES8311_CLK_MANAGER_REG06, &regv), TAG, "I2C read/write error");
    regv &= 0xE0;

    if (selected_coeff->bclk_div < 19)
    {
        regv |= (selected_coeff->bclk_div - 1) << 0;
    }
    else
    {
        regv |= (selected_coeff->bclk_div) << 0;
    }

    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_CLK_MANAGER_REG06, regv), TAG, "I2C read/write error");

    /* register 0x07 */
    ESP_RETURN_ON_ERROR(es8311_read_reg(ES8311_CLK_MANAGER_REG07, &regv), TAG, "I2C read/write error");
    regv &= 0xC0;
    regv |= selected_coeff->lrck_h << 0;
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_CLK_MANAGER_REG07, regv), TAG, "I2C read/write error");

    /* register 0x08 */
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_CLK_MANAGER_REG08, selected_coeff->lrck_l), TAG, "I2C read/write error");

    return ESP_OK;
}
static esp_err_t es8311_clock_config(const es8311_clock_config_t *const clk_cfg, es8311_resolution_t res)
{
    uint8_t reg06;
    uint8_t reg01 = 0x3F; // Enable all clocks
    int mclk_hz;

    /* Select clock source for internal MCLK and determine its frequency */
    if (clk_cfg->mclk_from_mclk_pin)
    {
        mclk_hz = clk_cfg->mclk_frequency;
    }
    else
    {
        mclk_hz = clk_cfg->sample_frequency * (int)res * 2;
        reg01 |= BIT(7); // Select BCLK (a.k.a. SCK) pin
    }

    if (clk_cfg->mclk_inverted)
    {
        reg01 |= BIT(6); // Invert MCLK pin
    }
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_CLK_MANAGER_REG01, reg01), TAG, "I2C read/write error");

    ESP_RETURN_ON_ERROR(es8311_read_reg(ES8311_CLK_MANAGER_REG06, &reg06), TAG, "I2C read/write error");
    if (clk_cfg->sclk_inverted)
    {
        reg06 |= BIT(5);
    }
    else
    {
        reg06 &= ~BIT(5);
    }
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_CLK_MANAGER_REG06, reg06), TAG, "I2C read/write error");

    /* Configure clock dividers */
    return es8311_sample_frequency_config(mclk_hz, clk_cfg->sample_frequency);
}
static esp_err_t es8311_resolution_config(const es8311_resolution_t res, uint8_t *reg)
{
    switch (res)
    {
    case ES8311_RESOLUTION_16:
        *reg |= (3 << 2);
        break;
    case ES8311_RESOLUTION_18:
        *reg |= (2 << 2);
        break;
    case ES8311_RESOLUTION_20:
        *reg |= (1 << 2);
        break;
    case ES8311_RESOLUTION_24:
        *reg |= (0 << 2);
        break;
    case ES8311_RESOLUTION_32:
        *reg |= (4 << 2);
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}
static esp_err_t es8311_fmt_config(const es8311_resolution_t res_in, const es8311_resolution_t res_out)
{
    uint8_t reg09 = 0; // SDP In
    uint8_t reg0a = 0; // SDP Out

    ESP_LOGI(TAG, "ES8311 in Slave mode and I2S format");
    uint8_t reg00;
    ESP_RETURN_ON_ERROR(es8311_read_reg(ES8311_RESET_REG00, &reg00), TAG, "I2C read/write error");
    reg00 &= 0xBF;
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_RESET_REG00, reg00), TAG, "I2C read/write error"); // Slave serial port - default

    /* Setup SDP In and Out resolution */
    es8311_resolution_config(res_in, &reg09);
    es8311_resolution_config(res_out, &reg0a);

    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_SDPIN_REG09, reg09), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_SDPOUT_REG0A, reg0a), TAG, "I2C read/write error");

    return ESP_OK;
}
esp_err_t es8311_microphone_config(bool digital_mic)
{
    uint8_t reg14 = 0x1A; // enable analog MIC and max PGA gain

    /* PDM digital microphone enable or disable */
    if (digital_mic)
    {
        reg14 |= BIT(6);
    }
    es8311_write_reg(ES8311_ADC_REG17, 0xC8); // Set ADC gain @todo move this to ADC config section

    return es8311_write_reg(ES8311_SYSTEM_REG14, reg14);
}
esp_err_t es8311_init(const es8311_clock_config_t *const clk_cfg, const es8311_resolution_t res_in, const es8311_resolution_t res_out)
{
    ESP_RETURN_ON_FALSE(
        (clk_cfg->sample_frequency >= 8000) && (clk_cfg->sample_frequency <= 96000),
        ESP_ERR_INVALID_ARG, TAG, "ES8311 init needs frequency in interval [8000; 96000] Hz");
    if (!clk_cfg->mclk_from_mclk_pin)
    {
        ESP_RETURN_ON_FALSE(res_out == res_in, ESP_ERR_INVALID_ARG, TAG, "Resolution IN/OUT must be equal if MCLK is taken from SCK pin");
    }

    /* Reset ES8311 to its default */
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_RESET_REG00, 0x1F), TAG, "I2C read/write error");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_RESET_REG00, 0x00), TAG, "I2C read/write error");
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_RESET_REG00, 0x80), TAG, "I2C read/write error"); // Power-on command

    /* Setup clock: source, polarity and clock dividers */
    ESP_RETURN_ON_ERROR(es8311_clock_config(clk_cfg, res_out), TAG, "");

    /* Setup audio format (fmt): master/slave, resolution, I2S */
    ESP_RETURN_ON_ERROR(es8311_fmt_config(res_in, res_out), TAG, "");

    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_SYSTEM_REG0D, 0x01), TAG, "I2C read/write error"); // Power up analog circuitry - NOT default
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_SYSTEM_REG0E, 0x02), TAG, "I2C read/write error"); // Enable analog PGA, enable ADC modulator - NOT default
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_SYSTEM_REG12, 0x00), TAG, "I2C read/write error"); // power-up DAC - NOT default
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_SYSTEM_REG13, 0x10), TAG, "I2C read/write error"); // Enable output to HP drive - NOT default
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_ADC_REG1C, 0x6A), TAG, "I2C read/write error");    // ADC Equalizer bypass, cancel DC offset in digital domain
    ESP_RETURN_ON_ERROR(es8311_write_reg(ES8311_DAC_REG37, 0x08), TAG, "I2C read/write error");    // Bypass DAC equalizer - NOT default

    return ESP_OK;
}
esp_err_t es8311_voice_volume_set(int volume, int *volume_set)
{
    if (volume < 0)
    {
        volume = 0;
    }
    else if (volume > 100)
    {
        volume = 100;
    }

    int reg32;
    if (volume == 0)
    {
        reg32 = 0;
    }
    else
    {
        reg32 = ((volume) * 256 / 100) - 1;
    }

    // provide user with real volume set
    if (volume_set != NULL)
    {
        *volume_set = volume;
    }
    return es8311_write_reg(ES8311_DAC_REG32, reg32);
}
esp_err_t es8311_voice_volume_get(int *volume)
{
    uint8_t reg32;
    ESP_RETURN_ON_ERROR(es8311_read_reg(ES8311_DAC_REG32, &reg32), TAG, "I2C read/write error");

    if (reg32 == 0)
    {
        *volume = 0;
    }
    else
    {
        *volume = ((reg32 * 100) / 256) + 1;
    }
    return ESP_OK;
}
esp_err_t es8311_voice_mute(bool mute)
{
    uint8_t reg31;
    ESP_RETURN_ON_ERROR(es8311_read_reg(ES8311_DAC_REG31, &reg31), TAG, "I2C read/write error");

    if (mute)
    {
        reg31 |= BIT(6) | BIT(5);
    }
    else
    {
        reg31 &= ~(BIT(6) | BIT(5));
    }

    return es8311_write_reg(ES8311_DAC_REG31, reg31);
}
esp_err_t es8311_microphone_gain_set(es8311_mic_gain_t gain_db)
{
    return es8311_write_reg(ES8311_ADC_REG16, gain_db); // ADC gain scale up
}
esp_err_t es8311_voice_fade(const es8311_fade_t fade)
{
    uint8_t reg37;
    ESP_RETURN_ON_ERROR(es8311_read_reg(ES8311_DAC_REG37, &reg37), TAG, "I2C read/write error");
    reg37 &= 0x0F;
    reg37 |= (fade << 4);
    return es8311_write_reg(ES8311_DAC_REG37, reg37);
}
esp_err_t es8311_microphone_fade(const es8311_fade_t fade)
{
    uint8_t reg15;
    ESP_RETURN_ON_ERROR(es8311_read_reg(ES8311_ADC_REG15, &reg15), TAG, "I2C read/write error");
    reg15 &= 0x0F;
    reg15 |= (fade << 4);
    return es8311_write_reg(ES8311_ADC_REG15, reg15);
}
void es8311_register_dump()
{
    for (int reg = 0; reg < 0x4A; reg++)
    {
        uint8_t value;
        ESP_ERROR_CHECK(es8311_read_reg(reg, &value));
        printf("REG:%02x: %02x", reg, value);
    }
}


// esp_err_t myiic_init(void)
// {
//     i2c_master_bus_config_t i2c_bus_config = {
//         .clk_source                     = I2C_CLK_SRC_DEFAULT,  /* 时钟源 */
//         .i2c_port                       = IIC_NUM_PORT,         /* I2C端口 */
//         .scl_io_num                     = IIC_SCL_GPIO_PIN,     /* SCL管脚 */
//         .sda_io_num                     = IIC_SDA_GPIO_PIN,     /* SDA管脚 */
//         .glitch_ignore_cnt              = 7,                    /* 故障周期 */
//         .flags.enable_internal_pullup   = true,                 /* 内部上拉 */
//     };
//     /* 新建I2C总线 */
//     ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

//     return ESP_OK;
// }


uint8_t user_es8311_init(void) {
    esp_err_t ret = ESP_OK;
  /* I2C总线已在board_dnesp32s3_init()中初始化 */
  if (bus_handle == NULL) {
    ESP_LOGE(TAG, "I2C bus handle is NULL");
    return 1;
  }
  i2c_device_config_t es8311_i2c_dev_conf = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7, /* 从机地址长度 */
      .scl_speed_hz = IIC_SPEED_CLK,         /* 传输速率 */
      .device_address = 0x18,         /* 从机7位的地址 */
  };
  /* I2C总线上添加ES8388设备 */
  ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &es8311_i2c_dev_conf, &es8311_handle));
  /* 等待i2c总线空闲 */
  ESP_ERROR_CHECK(i2c_master_bus_wait_all_done(bus_handle, 1000));

    es8311_clock_config_t clk_cfg = {
      .mclk_inverted = false,
      .sclk_inverted = false,
      .mclk_from_mclk_pin = true,
      .mclk_frequency = 16000*256, // i2s 配置16K*256为mclk主时钟
      .sample_frequency = 16000   // 16KHz 采样率
    };

  ret |= es8311_init(&clk_cfg, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
  ret |= es8311_voice_volume_set(95, NULL);                 // Set volume to 50%
  ret |= es8311_microphone_config(true);                    // Analog microphone
  ret |= es8311_microphone_gain_set(ES8311_MIC_GAIN_MAX);
  ret |= es8311_sample_frequency_config(16000*256, 256);
  return ret;
}


void es8311_delete()
{
    //free(dev);
}