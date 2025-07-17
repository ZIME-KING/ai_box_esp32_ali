/**
 ****************************************************************************************************
 * @file        es8388.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-01
 * @brief       ES8388驱动代码
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

#include "es8388.h"
#include "bus/i2c/myi2c.h"

static const char *es8388_tag = "ES8388";
i2c_master_dev_handle_t es8388_handle = NULL;

/**
 * @brief       ES8388写寄存器
 * @param       reg_addr:寄存器地址
 * @param       data:写入的数据
 * @retval      无
 */
esp_err_t es8388_write_reg(uint8_t reg_addr, uint8_t data) {
  esp_err_t ret;
  uint8_t *buf = malloc(2);
  if (buf == NULL) {
    ESP_LOGE(es8388_tag, "%s memory failed", __func__);
    return ESP_ERR_NO_MEM; /* 分配内存失败 */
  }

  buf[0] = reg_addr;
  buf[1] = data; /* 拷贝数据至存储区当中 */

  do {
    i2c_master_bus_wait_all_done(bus_handle, 1000);
    ret = i2c_master_transmit(es8388_handle, buf, 2, 1000);
  } while (ret != ESP_OK);

  free(buf); /* 发送完成释放内存 */

  return ret;
}

/**
 * @brief       ES8388读寄存器
 * @param       reg_add:寄存器地址
 * @param       p_data:读取的数据
 * @retval      无
 */
esp_err_t es8388_read_reg(uint8_t reg_addr, uint8_t *pdata) {
  uint8_t reg_data = 0;
  i2c_master_transmit_receive(es8388_handle, &reg_addr, 1, &reg_data, 1, -1);
  return reg_data;
}

/**
 * @brief       ES8388初始化
 * @param       无
 * @retval      0,初始化正常
 *              其他,错误代码
 */
uint8_t es8388_init(void) {
  uint8_t ret_val = 0;

  /* I2C总线已在board_dnesp32s3_init()中初始化 */
  if (bus_handle == NULL) {
    ESP_LOGE(es8388_tag, "I2C bus handle is NULL");
    return 1;
  }

  i2c_device_config_t es8388_i2c_dev_conf = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7, /* 从机地址长度 */
      .scl_speed_hz = IIC_SPEED_CLK,         /* 传输速率 */
      .device_address = ES8388_ADDR,         /* 从机7位的地址 */
  };
  /* I2C总线上添加ES8388设备 */
  ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &es8388_i2c_dev_conf,
                                            &es8388_handle));
  ESP_ERROR_CHECK(i2c_master_bus_wait_all_done(bus_handle, 1000));

  ret_val |= es8388_write_reg(0, 0x80); /* 软复位ES8388 */
  ret_val |= es8388_write_reg(0, 0x00);
  vTaskDelay(pdMS_TO_TICKS(200)); /* 等待复位 */

  ret_val |= es8388_write_reg(0x01, 0x58);
  ret_val |= es8388_write_reg(0x01, 0x50);
  ret_val |= es8388_write_reg(0x02, 0xF3);
  ret_val |= es8388_write_reg(0x02, 0xF0);

  ret_val |= es8388_write_reg(0x03, 0x09); /* 麦克风偏置电源关闭 */
  ret_val |= es8388_write_reg(0x00, 0x06); /* 使能参考 500K驱动使能 */
  ret_val |= es8388_write_reg(0x04, 0x00); /* DAC电源管理，不打开任何通道 */
  ret_val |= es8388_write_reg(0x08, 0x00); /* MCLK不分频 */
  ret_val |= es8388_write_reg(0x2B, 0x80); /* DAC控制 DACLRC与ADCLRC相同 */

  ret_val |= es8388_write_reg(0x09, 0x88); /* ADC L/R PGA增益配置为+24dB */
  ret_val |= es8388_write_reg(
      0x0C, 0x4C); /* ADC数据选择为left data = left ADC, right data = left ADC
                      音频数据为16bit */
  ret_val |= es8388_write_reg(0x0D, 0x02); /* ADC配置 MCLK/采样率=256 */
  ret_val |= es8388_write_reg(
      0x10, 0x00); /* ADC数字音量控制将信号衰减 L  设置为最小！！！ */
  ret_val |= es8388_write_reg(
      0x11, 0x00); /* ADC数字音量控制将信号衰减 R  设置为最小！！！ */

  ret_val |= es8388_write_reg(0x17, 0x18); /* DAC音频数据为16bit */
  ret_val |= es8388_write_reg(0x18, 0x02); /* DAC配置 MCLK/采样率=256 */
  ret_val |= es8388_write_reg(
      0x1A, 0x00); /* DAC数字音量控制将信号衰减 L  设置为最小！！！ */
  ret_val |= es8388_write_reg(
      0x1B, 0x00); /* DAC数字音量控制将信号衰减 R  设置为最小！！！ */
  ret_val |= es8388_write_reg(0x27, 0xB8); /* L混频器 */
  ret_val |= es8388_write_reg(0x2A, 0xB8); /* R混频器 */
  vTaskDelay(pdMS_TO_TICKS(100));

  if (ret_val != ESP_OK) {
    ESP_LOGI(es8388_tag, "ES8388 fail");
    return 1;
  } else {
    ESP_LOGI(es8388_tag, "ES8388 success");
    vTaskDelay(pdMS_TO_TICKS(100));
    return 0;
  }

  es8388_adda_cfg(0, 0);   /* 开启DAC关闭ADC */
  es8388_input_cfg(0);     /* 关闭录音输入 */
  es8388_output_cfg(0, 0); /* DAC选择通道输出 */
  es8388_hpvol_set(0);     /* 设置耳机音量 */
  es8388_spkvol_set(0);    /* 设置喇叭音量 */

  return 0;
}

/**
 * @brief       ES8388反初始化
 * @param       无
 * @retval      0,初始化正常
 *              其他,错误代码
 */
esp_err_t es8388_deinit(void) {
  return es8388_write_reg(0x02, 0xFF); /* 复位和暂停ES8388 */
}

/**
 * @brief       设置ES8388工作模式
 * @param       fmt : 工作模式
 *    @arg      0, 飞利浦标准I2S;
 *    @arg      1, MSB(左对齐);
 *    @arg      2, LSB(右对齐);
 *    @arg      3, PCM/DSP
 * @param       len : 数据长度
 *    @arg      0, 24bit
 *    @arg      1, 20bit
 *    @arg      2, 18bit
 *    @arg      3, 16bit
 *    @arg      4, 32bit
 * @retval      无
 */
void es8388_i2s_cfg(uint8_t fmt, uint8_t len) {
  fmt &= 0x03;
  len &= 0x07;                                      /* 限定范围 */
  es8388_write_reg(23, (fmt << 1) | (len << 3)); /* R23,ES8388工作模式设置 */
}

/**
 * @brief       设置耳机音量
 * @param       volume : 音量大小(0 ~ 33)
 * @retval      无
 */
void es8388_hpvol_set(uint8_t volume) {
  if (volume > 33) {
    volume = 33;
  }

  es8388_write_reg(0x2E, volume);
  es8388_write_reg(0x2F, volume);
}

/**
 * @brief       设置喇叭音量
 * @param       volume : 音量大小(0 ~ 33)
 * @retval      无
 */
void es8388_spkvol_set(uint8_t volume) {
  if (volume > 33) {
    volume = 33;
  }

  es8388_write_reg(0x30, volume);
  es8388_write_reg(0x31, volume);
}

/**
 * @brief       设置3D环绕声
 * @param       depth : 3D效果深度,0~7(3D效果最大)
 * @retval      无
 */
void es8388_3d_set(uint8_t depth) {
  depth &= 0x7;                                /* 限定范围 */
  es8388_write_reg(0x7C, depth << 2); /* 填写3D效果 */
}

/**
 * @brief       ES8388 DAC/ADC配置
 * @param       dacen : DAC使能(0)/关闭(1)
 * @param       adcen : ADC使能(0)/关闭(1)
 * @retval      无
 */
void es8388_adda_cfg(uint8_t dacen, uint8_t adcen) {
  uint8_t res = 0;

  res = es8388_read_reg(0x02, &res); /* 读取ES8388状态 */

  if (dacen) {
    res &= ~(1 << 0); /* 使能DAC */
  } else {
    res |= 1 << 0; /* 关闭DAC */
  }

  if (adcen) {
    res &= ~(1 << 1); /* 使能ADC */
  } else {
    res |= 1 << 1; /* 关闭ADC */
  }

  es8388_write_reg(0x02, res); /* 设置ES8388状态 */
}

/**
 * @brief       ES8388 DAC输出通道配置
 * @param       o1en : 通道1(LIN1/RIN1)使能(1)/关闭(0)
 * @param       o2en : 通道2(LIN2/RIN2)使能(1)/关闭(0)
 * @retval      无
 */
void es8388_output_cfg(uint8_t o1en, uint8_t o2en) {
  uint8_t res = 0;

  res = es8388_read_reg(0x04, &res); /* 读取ES8388状态 */

  if (o1en) {
    res |= 3 << 4; /* 开启通道1 */
  } else {
    res &= ~(3 << 4); /* 关闭通道1 */
  }

  if (o2en) {
    res |= 3 << 0; /* 开启通道2 */
  } else {
    res &= ~(3 << 0); /* 关闭通道2 */
  }

  es8388_write_reg(0x04, res); /* 设置ES8388状态 */
}

/**
 * @brief       ES8388 MIC增益设置(MIC PGA增益)
 * @param       gain : 0~8, 0表示0dB,1~8,表示+3~+24dB,3dB/Step
 * @retval      无
 */
void es8388_mic_gain(uint8_t gain) {
  gain &= 0x0f;

  es8388_write_reg(0x09, gain << 4); /* 设置MIC PGA增益 */
}

/**
 * @brief       ES8388 ALC设置
 * @param       sel : 0,关闭ALC; 1,右通道ALC; 2,左通道ALC; 3,立体声ALC
 * @param       maxgain : 最大增益,0~7,对应-6.5~+35.5dB
 * @param       mingain : 最小增益,0~7,对应-12~+30dB
 * @retval      无
 */
void es8388_alc_ctrl(uint8_t sel, uint8_t maxgain, uint8_t mingain) {
  es8388_write_reg(0x12, sel << 6);                /* 选择ALC通道 */
  es8388_write_reg(0x13, (maxgain << 4) | mingain); /* 设置最大/最小增益 */
}

/**
 * @brief       ES8388 ADC输入通道配置
 * @param       in : 输入通道选择
 *    @arg      0,通道1(LIN1/RIN1)输入
 *    @arg      1,通道2(LIN2/RIN2)输入
 *    @arg      2,通道3(LIN2/RIN2)输入
 *    @arg      3,通道4(LIN2/RIN2)输入
 *    @arg      4,DIFF输入
 * @retval      无
 */
void es8388_input_cfg(uint8_t in) {
  es8388_write_reg(0x0A, (5 * in) << 4); /* ADC1 输入通道选择L/R INPUT1 */
}
