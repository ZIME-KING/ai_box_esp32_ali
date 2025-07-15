/**
 ****************************************************************************************************
 * @file        i2s.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-01
 * @brief       I2S驱动代码
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

#include "audio/i2s/i2s.h"

#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "MyI2S";

i2s_chan_handle_t tx_handle = NULL; /* I2S发送通道句柄 */
i2s_chan_handle_t rx_handle = NULL; /* I2S接收通道句柄 */
i2s_std_config_t my_tx_cfg;         /* 标准模式配置结构体 */
i2s_std_config_t my_rx_cfg;         /* 标准模式配置结构体 */

static IRAM_ATTR bool i2s_rx_queue_overflow_callback(i2s_chan_handle_t handle,
                                                     i2s_event_data_t *event,
                                                     void *user_ctx) {
  // 处理 RX 队列溢出事件 ...
  ESP_LOGW(TAG, "RX - The buffer size of DMA :%d", event->size);
  return false;
}

static IRAM_ATTR bool i2s_tx_queue_overflow_callback(i2s_chan_handle_t handle,
                                                     i2s_event_data_t *event,
                                                     void *user_ctx) {
  // 处理 RX 队列溢出事件 ...
  ESP_LOGW(TAG, "TX - The buffer size of DMA :%d", event->size);
  return false;
}

i2s_event_callbacks_t rx_cbs = {
    .on_recv = NULL,
    .on_recv_q_ovf = i2s_rx_queue_overflow_callback,
    .on_sent = NULL,
    .on_send_q_ovf = NULL,
};

i2s_event_callbacks_t tx_cbs = {
    .on_recv = NULL,
    .on_recv_q_ovf = i2s_tx_queue_overflow_callback,
    .on_sent = NULL,
    .on_send_q_ovf = NULL,
};

/*
 * @brief       初始化I2S
 * @param       无
 * @retval      ESP_OK:初始化成功;其他:失败
 */
esp_err_t myi2s_init(int sample_rate) {
  //   i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
  //       I2S_NUM, I2S_ROLE_MASTER); /* 默认的通道配置(I2S0,主机) */
  //   chan_cfg.auto_clear = true;    /* 自动清除DMA缓冲区遗留的数据 */
  i2s_chan_config_t chan_cfg = {
      .id = I2S_NUM,
      .role = I2S_ROLE_MASTER,
      .dma_desc_num = 2,
      .dma_frame_num = 1024,
      .auto_clear = true,
      .intr_priority = 0,
  };
  ESP_ERROR_CHECK(
      i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle)); /* 分配新的I2S通道 */

  i2s_std_config_t std_cfg = {
      /* 标准通信模式配置 */
      //   .clk_cfg =
      //       {
      //           /* 时钟配置
      //              可用I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE)宏函数辅助配置
      //            */
      //           .sample_rate_hz = I2S_SAMPLE_RATE, /* I2S采样率 */
      //           .clk_src = I2S_CLK_SRC_DEFAULT,    /* I2S时钟源 */
      //           .mclk_multiple =
      //               I2S_MCLK_MULTIPLE, /*
      //               I2S主时钟MCLK相对于采样率的倍数(默认256)
      //                                   */
      //       },
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),

      .slot_cfg =
          {
              /*
              声道配置,可用I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                  I2S_SLOT_MODE_STEREO)宏函数辅助配置(支持16位宽采样数据) */
              .data_bit_width =
                  I2S_DATA_BIT_WIDTH_16BIT, /* 声道支持16位宽的采样数据 */
              .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, /* 通道位宽 */
              .slot_mode = I2S_SLOT_MODE_MONO,           /* 单通道 */
              .slot_mask = I2S_STD_SLOT_LEFT,            /* 启用通道 */
              .ws_width = I2S_DATA_BIT_WIDTH_16BIT,      /* WS信号位宽 */
              .ws_pol = false,                           /* WS信号极性 */
              .bit_shift = true,     /* 位移位(Philips模式下配置) */
              .left_align = true,    /* 左对齐 */
              .big_endian = false,   /* 小端模式 */
              .bit_order_lsb = false /* MSB */
          },
      //   .slot_cfg =
      //   I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
      //                                                   I2S_SLOT_MODE_MONO),

      .gpio_cfg =
          {
              /* 引脚配置 */
              .mclk = I2S_MCK_IO, /* 主时钟线 */
              .bclk = I2S_BCK_IO, /* 位时钟线 */
              .ws = I2S_WS_IO,    /* 字(声道)选择线 */
              .dout = I2S_DO_IO,  /* 串行数据输出线 */
              .din = I2S_DI_IO,   /* 串行数据输入线 */
              .invert_flags =
                  {
                      /* 引脚翻转(不反相) */
                      .mclk_inv = false,
                      .bclk_inv = false,
                      .ws_inv = false,
                  },
          },
  };

  i2s_std_config_t tx_std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
      //   .slot_cfg =
      //   I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
      //                                                   I2S_SLOT_MODE_MONO),
      .slot_cfg =
          {
              /*
              声道配置,可用I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                  I2S_SLOT_MODE_STEREO)宏函数辅助配置(支持16位宽采样数据) */
              .data_bit_width =
                  I2S_DATA_BIT_WIDTH_16BIT, /* 声道支持16位宽的采样数据 */
              .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, /* 通道位宽 */
              .slot_mode = I2S_SLOT_MODE_MONO,           /* 单通道 */
              .slot_mask = I2S_STD_SLOT_LEFT,            /* 启用通道 */
              .ws_width = I2S_DATA_BIT_WIDTH_16BIT,      /* WS信号位宽 */
              .ws_pol = false,                           /* WS信号极性 */
              .bit_shift = true,     /* 位移位(Philips模式下配置) */
              .left_align = true,    /* 左对齐 */
              .big_endian = false,   /* 小端模式 */
              .bit_order_lsb = false /* MSB */
          },

      .gpio_cfg =
          {
              /* 引脚配置 */
              .mclk = I2S_MCK_IO, /* 主时钟线 */
              .bclk = I2S_BCK_IO, /* 位时钟线 */
              .ws = I2S_WS_IO,    /* 字(声道)选择线 */
              .dout = I2S_DO_IO,  /* 串行数据输出线 */
              .din = I2S_DI_IO,   /* 串行数据输入线 */
              .invert_flags =
                  {
                      /* 引脚翻转(不反相) */
                      .mclk_inv = false,
                      .bclk_inv = false,
                      .ws_inv = false,
                  },
          },
  };

  my_tx_cfg = tx_std_cfg;
  my_rx_cfg = std_cfg;

  // i2s_channel_register_event_callback(tx_handle, &tx_cbs, NULL);
  // i2s_channel_register_event_callback(rx_handle, &rx_cbs, NULL);

  ESP_ERROR_CHECK(
      i2s_channel_init_std_mode(tx_handle, &tx_std_cfg)); /* 初始化TX通道 */
  ESP_ERROR_CHECK(
      i2s_channel_init_std_mode(rx_handle, &std_cfg)); /* 初始化RX通道 */
  //   ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));      /* 启用TX通道 */
  //   ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));      /* 启用RX通道 */

  return ESP_OK;
}

/**
 * @brief       I2S TRX启动
 * @param       无
 * @retval      无
 */
void i2s_trx_start(void) {
  ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
  ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}

/**
 * @brief       I2S TRX停止
 * @param       无
 * @retval      无
 */
void i2s_trx_stop(void) {
  ESP_ERROR_CHECK(i2s_channel_disable(tx_handle));
  ESP_ERROR_CHECK(i2s_channel_disable(rx_handle));
}

void i2s_tx_start(void) { ESP_ERROR_CHECK(i2s_channel_enable(tx_handle)); }

void i2s_tx_stop(void) { ESP_ERROR_CHECK(i2s_channel_disable(tx_handle)); }

void i2s_rx_start(void) { ESP_ERROR_CHECK(i2s_channel_enable(rx_handle)); }

void i2s_rx_stop(void) { ESP_ERROR_CHECK(i2s_channel_disable(rx_handle)); }

/**
 * @brief       I2S卸载
 * @param       无
 * @retval      无
 */
void i2s_deinit(void) {
  ESP_ERROR_CHECK(i2s_del_channel(tx_handle));
  ESP_ERROR_CHECK(i2s_del_channel(rx_handle));
}

/**
 * @brief       设置采样率和位宽
 * @param       sampleRate  :采样率
 * @param       bits_sample :位宽
 * @retval      无
 */
void i2s_set_samplerate_bits_sample(int samplerate, int bits_sample) {
  //   i2s_trx_stop();

  /* 如果需要更新声道或时钟配置,需要在更新前先禁用通道 */
  my_tx_cfg.slot_cfg.ws_width = bits_sample; /* 位宽 */
  ESP_ERROR_CHECK(
      i2s_channel_reconfig_std_slot(tx_handle, &my_tx_cfg.slot_cfg));
  my_tx_cfg.clk_cfg.sample_rate_hz = samplerate; /* 设置采样率 */
  ESP_ERROR_CHECK(
      i2s_channel_reconfig_std_clock(tx_handle, &my_tx_cfg.clk_cfg));

  /* 如果需要更新声道或时钟配置,需要在更新前先禁用通道 */
  my_rx_cfg.slot_cfg.ws_width = bits_sample; /* 位宽 */
  ESP_ERROR_CHECK(
      i2s_channel_reconfig_std_slot(rx_handle, &my_rx_cfg.slot_cfg));
  my_rx_cfg.clk_cfg.sample_rate_hz = samplerate; /* 设置采样率 */
  ESP_ERROR_CHECK(
      i2s_channel_reconfig_std_clock(rx_handle, &my_rx_cfg.clk_cfg));
}

void i2s_set_rx_samplerate_bits_sample(int samplerate, int bits_sample) {
  //   i2s_trx_stop();

  /* 如果需要更新声道或时钟配置,需要在更新前先禁用通道 */
  my_rx_cfg.slot_cfg.ws_width = bits_sample; /* 位宽 */
  ESP_ERROR_CHECK(
      i2s_channel_reconfig_std_slot(rx_handle, &my_rx_cfg.slot_cfg));
  my_rx_cfg.clk_cfg.sample_rate_hz = samplerate; /* 设置采样率 */
  ESP_ERROR_CHECK(
      i2s_channel_reconfig_std_clock(rx_handle, &my_rx_cfg.clk_cfg));
}

void i2s_set_tx_samplerate_bits_sample(int samplerate, int bits_sample) {
  //   i2s_trx_stop();

  /* 如果需要更新声道或时钟配置,需要在更新前先禁用通道 */
  my_tx_cfg.slot_cfg.ws_width = bits_sample; /* 位宽 */
  ESP_ERROR_CHECK(
      i2s_channel_reconfig_std_slot(tx_handle, &my_tx_cfg.slot_cfg));
  my_tx_cfg.clk_cfg.sample_rate_hz = samplerate; /* 设置采样率 */
  ESP_ERROR_CHECK(
      i2s_channel_reconfig_std_clock(tx_handle, &my_tx_cfg.clk_cfg));
}

/**
 * @brief       I2S传输数据
 * @param       buffer: 数据存储区的首地址
 * @param       frame_size: 数据大小
 * @retval      发送的数据长度
 */
size_t i2s_tx_write(uint8_t *buffer, uint32_t frame_size) {
  size_t bytes_written;
  ESP_ERROR_CHECK(i2s_channel_write(tx_handle, buffer, frame_size,
                                    &bytes_written, pdMS_TO_TICKS(1000)));
  return bytes_written;
}

/**
 * @brief       I2S读取数据
 * @param       buffer: 读取数据存储区的首地址
 * @param       frame_size: 读取数据大小
 * @retval      接收的数据长度
 */
size_t i2s_rx_read(uint8_t *buffer, uint32_t frame_size) {
  size_t bytes_written = 0;
  ESP_ERROR_CHECK(i2s_channel_read(rx_handle, buffer, frame_size,
                                   &bytes_written, pdMS_TO_TICKS(1000)));
  return bytes_written;
}
