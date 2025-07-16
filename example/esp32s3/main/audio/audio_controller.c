/*
 * Copyright 2025 Alibaba Group Holding Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "audio_controller.h"

#include "audio_common.h"
#include "audio_controller.h"
#include "audio_element.h"
#include "audio_mem.h"
#include "audio_pipeline.h"
#include "buffer_pool.h"
#include "driver/i2s.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_peripherals.h"
#include "ff.h"
#include "filter_resample.h"
#include "freertos/FreeRTOS.h"
#include "mp3_decoder.h"
#include "es8388.h"
#include "i2s.h"
#include "xl9555.h"

static const char *TAG = "AudioController";

static audio_ctrl_t *audio_ctrl = NULL;
#define DEFAULT_AUDIO_CTRL_SR 16000

int recorder_init(audio_ctrl_t *audio) {
  audio_ctrl = audio;
  audio_ctrl->recorder_lock = xSemaphoreCreateRecursiveMutex();
  xSemaphoreGiveRecursive(audio_ctrl->recorder_lock);

  if (audio_ctrl->recorder_sr == 0) {
    audio_ctrl->recorder_sr = DEFAULT_AUDIO_CTRL_SR;
  }

  audio_ctrl->recorder_status |= 0x80;
  return 0;
}

int recorder_deinit() {
  audio_ctrl->recorder_status &= 0x7F;
  return 0;
}

int recorder_reset() { return 0; }

static void recorder_chip_start() {
  if (audio_ctrl->codec_dev == NULL) {
      // 创建数据接口（I2S）
      audio_codec_data_if_t *data_if = NULL;
      audio_codec_i2s_cfg_default_t i2s_cfg = {
          .rx_handle = i2s_keep[0]->rx_handle,
          .tx_handle = i2s_keep[0]->tx_handle,
      };
      data_if = audio_codec_new_i2s_data(&i2s_cfg);
      if (data_if == NULL) {
          ESP_LOGE(TAG, "Failed to create I2S data interface");
          return;
      }
      audio_ctrl->data_if = data_if;
  
      // 创建控制接口（I2C）
      audio_codec_i2c_cfg_t i2c_cfg = {
          .port = I2C_NUM_0,
          .addr = ES8311_CODEC_DEFAULT_ADDR,
      };
      audio_ctrl->ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
      if (audio_ctrl->ctrl_if == NULL) {
          ESP_LOGE(TAG, "Failed to create I2C control interface");
          goto err_ctrl_if;
      }

      // 创建 GPIO 接口
      audio_codec_gpio_if_cfg_t gpio_cfg = {
          .reset_gpio = GPIO_NUM_48,  // ES8311 reset pin
          .mute_gpio = GPIO_NUM_NC,   // No mute pin
          .record_led_gpio = GPIO_NUM_NC,  // No record LED
          .play_led_gpio = GPIO_NUM_NC,    // No play LED
      };
      audio_ctrl->gpio_if = audio_codec_new_gpio(&gpio_cfg);
      if (audio_ctrl->gpio_if == NULL) {
          ESP_LOGE(TAG, "Failed to create GPIO interface");
          goto err_gpio_if;
      }
  
      // 创建编解码器接口
      audio_codec_if_t *codec_if = audio_codec_new_es8311();
      if (codec_if == NULL) {
          ESP_LOGE(TAG, "Failed to create ES8311 codec interface");
          goto err_codec_if;
      }
      audio_ctrl->codec_if = codec_if;

      // 创建编解码器设备
      esp_codec_dev_cfg_t dev_cfg = {
          .codec_if = codec_if,
          .ctrl_if = audio_ctrl->ctrl_if,
          .data_if = audio_ctrl->data_if,
          .gpio_if = audio_ctrl->gpio_if,
      };
      esp_codec_dev_handle_t codec_dev = esp_codec_dev_new(&dev_cfg);
      if (codec_dev == NULL) {
          ESP_LOGE(TAG, "Failed to create codec device");
          goto err_codec_dev;
      }
      audio_ctrl->codec_dev = codec_dev;

      // 配置录音参数
      esp_codec_dev_sample_info_t fs = {
          .sample_rate = audio_ctrl->recorder_sr,
          .channel = 2,
          .bits_per_sample = 16,
      };
      esp_codec_dev_set_in_channel_gain(codec_dev, ESP_CODEC_DEV_CHANNEL_LEFT | ESP_CODEC_DEV_CHANNEL_RIGHT, 0);
      esp_codec_dev_sample_info_set(codec_dev, ESP_CODEC_DEV_SAMPLE_INFO_TYPE_IN, &fs);
      return;

err_codec_dev:
      audio_codec_delete_codec_if(audio_ctrl->codec_if);
      audio_ctrl->codec_if = NULL;
err_codec_if:
      audio_codec_delete_gpio_if(audio_ctrl->gpio_if);
      audio_ctrl->gpio_if = NULL;
      goto err_gpio_if;
  }
}

static void player_chip_start() {
  if (audio_ctrl->codec_dev) {
      // 配置播放参数
      esp_codec_dev_sample_info_t fs = {
          .sample_rate = audio_ctrl->player_sr,
          .channel = 2,
          .bits_per_sample = 16,
      };
      esp_codec_dev_set_out_channel_gain(audio_ctrl->codec_dev, ESP_CODEC_DEV_CHANNEL_LEFT | ESP_CODEC_DEV_CHANNEL_RIGHT, 0);
      esp_codec_dev_sample_info_set(audio_ctrl->codec_dev, ESP_CODEC_DEV_SAMPLE_INFO_TYPE_OUT, &fs);
  }
}

static int recorder_fetch_data(uint8_t *buffer, int len) {
    if (audio_ctrl->codec_dev == NULL) {
        return -1;
    }
    int ret = esp_codec_dev_read(audio_ctrl->codec_dev, buffer, len);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to read data from codec device");
        return -1;
    }
    return ret;
}

static int player_hw_play(uint8_t *buffer, int len) {
    if (audio_ctrl->codec_dev == NULL) {
        return -1;
    }
    int ret = esp_codec_dev_write(audio_ctrl->codec_dev, buffer, len);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to write data to codec device");
        return -1;
    }
    return ret;
}

int recorder_deinit() {
  if (audio_ctrl->codec_dev != NULL) {
    esp_codec_dev_close(audio_ctrl->codec_dev);
    esp_codec_dev_delete(audio_ctrl->codec_dev);
    audio_ctrl->codec_dev = NULL;
  }
  audio_ctrl->recorder_status &= 0x7F;
  return 0;
}

int player_deinit() {
  /* 停止解码任务流 */
  audio_pipeline_stop();
  /* 释放解码任务流, 释放解码器和播放器缓存 */
  audio_pipeline_deinit();
  /* 等待播放器任务结束 */
  audio_ctrl->player_task_working = false;

  if (audio_ctrl->codec_dev != NULL) {
    esp_codec_dev_close(audio_ctrl->codec_dev);
    esp_codec_dev_delete(audio_ctrl->codec_dev);
    audio_ctrl->codec_dev = NULL;
  }

  audio_ctrl->player_status &= 0x7F;
  return 0;
}

int player_reset() {
  buffer_pool_reset(&(audio_ctrl->player_pool));
  buffer_pool_reset(&(audio_ctrl->decoder_in_pool));
  return 0;
}

static void player_chip_start() {

// //  return;
//   /* ES8388初始化配置，有效降低启动时发出沙沙声 */
//   es8388_adda_cfg(1, 0);       /* 打开DAC，关闭ADC */
//   es8388_input_cfg(0);         /* 录音关闭 */
//   es8388_output_cfg(1, 1);     /* 喇叭通道和耳机通道打开 */
//   es8388_hpvol_set(33);        /* 设置喇叭 */
//   es8388_spkvol_set(22);       /* 设置耳机 */
// //  xl9555_pin_write(SPK_EN_IO, 0); /* 打开喇叭 */
//   vTaskDelay(pdMS_TO_TICKS(20));
}

void player_start(int sample_rate) {
    ESP_LOGI(TAG, "player_start status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);

    if (audio_ctrl->codec_dev == NULL) {
        recorder_chip_start();  // 初始化编解码器设备
    }

    player_chip_start();

    if (audio_ctrl->codec_dev != NULL) {
        esp_codec_dev_open(audio_ctrl->codec_dev, ESP_CODEC_DEV_WORK_MODE_TX);
        audio_ctrl->player_status |= 0x08;
    }

    buffer_pool_reset(&(audio_ctrl->player_pool));
    buffer_pool_reset(&(audio_ctrl->decoder_in_pool));

    audio_ctrl->audio_ctrl_callback(AUDIO_CTRL_PLAYER, AUDIO_PLAYER_STARTED);

    ESP_LOGI(TAG, "player_start done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->player_status);
}

void player_stop() {
    ESP_LOGI(TAG, "player_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);

    /* clear draining flag */
    audio_ctrl->player_status &= 0xFB;

    if ((audio_ctrl->player_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->player_status &= 0xF7;
    }

    buffer_pool_reset(&(audio_ctrl->decoder_in_pool));
    buffer_pool_reset(&(audio_ctrl->player_pool));

    ESP_LOGI(TAG, "player_stop done, status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);
}

void player_drain() {
  ESP_LOGI(TAG, "player_drain status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  audio_ctrl->player_status |= 0x04;
  if ((audio_ctrl->player_status & 0x40) == 0x40) {
    drain_data(&(audio_ctrl->decoder_in_pool));
  } else {
    drain_data(&(audio_ctrl->player_pool));
  }
}

size_t player_insert_data(uint8_t *data, size_t data_size) {
  // ESP_LOGV(TAG, "player_insert_data status: 0x%x/0x%x, data_size:%d",
  //          audio_ctrl->codec_status, audio_ctrl->player_status, data_size);
  int written_data = 0;
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    if ((audio_ctrl->player_status & 0x40) == 0x40) {
      written_data = write_data(&(audio_ctrl->decoder_in_pool),
                                (const unsigned char *)data, data_size);
    } else {
      if ((audio_ctrl->player_status & 0x20) == 0x20) {
        written_data = write_data(&(audio_ctrl->player_pool),
                                  (const unsigned char *)data, data_size);
      }
    }
  }
  if (written_data < 0) {
    ESP_LOGE(TAG, "Write data into pool failed:%d", written_data);
    written_data = 0;
  }
  return written_data;
}

void recorder_start() {
    xSemaphoreTakeRecursive(audio_ctrl->recorder_lock, portMAX_DELAY);
    ESP_LOGI(TAG, "recoder_start status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    recorder_chip_start();

    if (audio_ctrl->codec_dev != NULL) {
        esp_codec_dev_open(audio_ctrl->codec_dev, ESP_CODEC_DEV_WORK_MODE_RX);
        audio_ctrl->recorder_status |= 0x08;
    }

    ESP_LOGI(TAG, "recoder_start done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
    xSemaphoreGiveRecursive(audio_ctrl->recorder_lock);
}

void recorder_stop() {
    ESP_LOGI(TAG, "recorder_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    if ((audio_ctrl->recorder_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->recorder_status &= 0xF7;
    }

    ESP_LOGI(TAG, "recoder_stop done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
}

size_t player_hw_play(uint8_t *data, size_t data_size) {
  ESP_LOGV(TAG, "player_hw_play status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    return esp_codec_dev_write(audio_ctrl->codec_dev, data, data_size);
  } else {
    return 0;
  }
}

int player_init(audio_ctrl_t *audio) {
    audio_ctrl = audio;
    audio_ctrl->player_lock = xSemaphoreCreateRecursiveMutex();
    xSemaphoreGiveRecursive(audio_ctrl->player_lock);

    if (audio_ctrl->decoder_sr == 0) {
        audio_ctrl->decoder_sr = DEFAULT_AUDIO_CTRL_SR;
    }
    if (audio_ctrl->player_sr == 0) {
        audio_ctrl->player_sr = DEFAULT_AUDIO_CTRL_SR;
    }

    /* 创建解码器和播放器缓存, 创建解码任务流 */
    audio_pipeline_init(audio_ctrl->decoder_format);

    /* 创建送数据到播放器的任务 */
    TaskHandle_t play_task_handler;
    audio_ctrl->player_task_working = true;
    xTaskCreate(player_task, "play_task_work", CONFIG_MAIN_TASK_STACK_SIZE,
                &play_task_handler, 18, NULL);

    audio_ctrl->player_status |= 0x80;

    /* 启动解码任务流 */
    audio_work_pipeline_start();

    return 0;
}

err_gpio_if:
      audio_codec_delete_ctrl_if(audio_ctrl->ctrl_if);
      audio_ctrl->ctrl_if = NULL;
err_ctrl_if:
      audio_codec_delete_data_if(audio_ctrl->data_if);
      audio_ctrl->data_if = NULL;
      return;
}

void player_stop() {
    ESP_LOGI(TAG, "player_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);

    /* clear draining flag */
    audio_ctrl->player_status &= 0xFB;

    if ((audio_ctrl->player_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->player_status &= 0xF7;
    }

    buffer_pool_reset(&(audio_ctrl->decoder_in_pool));
    buffer_pool_reset(&(audio_ctrl->player_pool));

    ESP_LOGI(TAG, "player_stop done, status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);
}

void player_drain() {
  ESP_LOGI(TAG, "player_drain status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  audio_ctrl->player_status |= 0x04;
  if ((audio_ctrl->player_status & 0x40) == 0x40) {
    drain_data(&(audio_ctrl->decoder_in_pool));
  } else {
    drain_data(&(audio_ctrl->player_pool));
  }
}

size_t player_insert_data(uint8_t *data, size_t data_size) {
  // ESP_LOGV(TAG, "player_insert_data status: 0x%x/0x%x, data_size:%d",
  //          audio_ctrl->codec_status, audio_ctrl->player_status, data_size);
  int written_data = 0;
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    if ((audio_ctrl->player_status & 0x40) == 0x40) {
      written_data = write_data(&(audio_ctrl->decoder_in_pool),
                                (const unsigned char *)data, data_size);
    } else {
      if ((audio_ctrl->player_status & 0x20) == 0x20) {
        written_data = write_data(&(audio_ctrl->player_pool),
                                  (const unsigned char *)data, data_size);
      }
    }
  }
  if (written_data < 0) {
    ESP_LOGE(TAG, "Write data into pool failed:%d", written_data);
    written_data = 0;
  }
  return written_data;
}

void recorder_start() {
    xSemaphoreTakeRecursive(audio_ctrl->recorder_lock, portMAX_DELAY);
    ESP_LOGI(TAG, "recoder_start status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    recorder_chip_start();

    if (audio_ctrl->codec_dev != NULL) {
        esp_codec_dev_open(audio_ctrl->codec_dev, ESP_CODEC_DEV_WORK_MODE_RX);
        audio_ctrl->recorder_status |= 0x08;
    }

    ESP_LOGI(TAG, "recoder_start done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
    xSemaphoreGiveRecursive(audio_ctrl->recorder_lock);
}

void recorder_stop() {
    ESP_LOGI(TAG, "recorder_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    if ((audio_ctrl->recorder_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->recorder_status &= 0xF7;
    }

    ESP_LOGI(TAG, "recoder_stop done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
}

size_t player_hw_play(uint8_t *data, size_t data_size) {
  ESP_LOGV(TAG, "player_hw_play status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    return i2s_tx_write(data, data_size);
  } else {
    return 0;
  }
}

int player_init(audio_ctrl_t *audio) {
    audio_ctrl = audio;
    audio_ctrl->player_lock = xSemaphoreCreateRecursiveMutex();
    xSemaphoreGiveRecursive(audio_ctrl->player_lock);

    if (audio_ctrl->decoder_sr == 0) {
        audio_ctrl->decoder_sr = DEFAULT_AUDIO_CTRL_SR;
    }
    if (audio_ctrl->player_sr == 0) {
        audio_ctrl->player_sr = DEFAULT_AUDIO_CTRL_SR;
    }

    /* 创建解码器和播放器缓存, 创建解码任务流 */
    audio_pipeline_init(audio_ctrl->decoder_format);

    /* 创建送数据到播放器的任务 */
    TaskHandle_t play_task_handler;
    audio_ctrl->player_task_working = true;
    xTaskCreate(player_task, "play_task_work", CONFIG_MAIN_TASK_STACK_SIZE,
                &play_task_handler, 18, NULL);

    audio_ctrl->player_status |= 0x80;

    /* 启动解码任务流 */
    audio_work_pipeline_start();

    return 0;
}

err_gpio_if:
      audio_codec_delete_ctrl_if(audio_ctrl->ctrl_if);
      audio_ctrl->ctrl_if = NULL;
err_ctrl_if:
      audio_codec_delete_data_if(audio_ctrl->data_if);
      audio_ctrl->data_if = NULL;
      return;
}

void player_stop() {
    ESP_LOGI(TAG, "player_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);

    /* clear draining flag */
    audio_ctrl->player_status &= 0xFB;

    if ((audio_ctrl->player_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->player_status &= 0xF7;
    }

    buffer_pool_reset(&(audio_ctrl->decoder_in_pool));
    buffer_pool_reset(&(audio_ctrl->player_pool));

    ESP_LOGI(TAG, "player_stop done, status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);
}

void player_drain() {
  ESP_LOGI(TAG, "player_drain status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  audio_ctrl->player_status |= 0x04;
  if ((audio_ctrl->player_status & 0x40) == 0x40) {
    drain_data(&(audio_ctrl->decoder_in_pool));
  } else {
    drain_data(&(audio_ctrl->player_pool));
  }
}

size_t player_insert_data(uint8_t *data, size_t data_size) {
  // ESP_LOGV(TAG, "player_insert_data status: 0x%x/0x%x, data_size:%d",
  //          audio_ctrl->codec_status, audio_ctrl->player_status, data_size);
  int written_data = 0;
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    if ((audio_ctrl->player_status & 0x40) == 0x40) {
      written_data = write_data(&(audio_ctrl->decoder_in_pool),
                                (const unsigned char *)data, data_size);
    } else {
      if ((audio_ctrl->player_status & 0x20) == 0x20) {
        written_data = write_data(&(audio_ctrl->player_pool),
                                  (const unsigned char *)data, data_size);
      }
    }
  }
  if (written_data < 0) {
    ESP_LOGE(TAG, "Write data into pool failed:%d", written_data);
    written_data = 0;
  }
  return written_data;
}

void recorder_start() {
    xSemaphoreTakeRecursive(audio_ctrl->recorder_lock, portMAX_DELAY);
    ESP_LOGI(TAG, "recoder_start status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    recorder_chip_start();

    if (audio_ctrl->codec_dev != NULL) {
        esp_codec_dev_open(audio_ctrl->codec_dev, ESP_CODEC_DEV_WORK_MODE_RX);
        audio_ctrl->recorder_status |= 0x08;
    }

    ESP_LOGI(TAG, "recoder_start done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
    xSemaphoreGiveRecursive(audio_ctrl->recorder_lock);
}

void recorder_stop() {
    ESP_LOGI(TAG, "recorder_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    if ((audio_ctrl->recorder_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->recorder_status &= 0xF7;
    }

    ESP_LOGI(TAG, "recoder_stop done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
}

size_t player_hw_play(uint8_t *data, size_t data_size) {
  ESP_LOGV(TAG, "player_hw_play status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    return i2s_tx_write(data, data_size);
  } else {
    return 0;
  }
}

int player_init(audio_ctrl_t *audio) {
    audio_ctrl = audio;
    audio_ctrl->player_lock = xSemaphoreCreateRecursiveMutex();
    xSemaphoreGiveRecursive(audio_ctrl->player_lock);

    if (audio_ctrl->decoder_sr == 0) {
        audio_ctrl->decoder_sr = DEFAULT_AUDIO_CTRL_SR;
    }
    if (audio_ctrl->player_sr == 0) {
        audio_ctrl->player_sr = DEFAULT_AUDIO_CTRL_SR;
    }

    /* 创建解码器和播放器缓存, 创建解码任务流 */
    audio_pipeline_init(audio_ctrl->decoder_format);

    /* 创建送数据到播放器的任务 */
    TaskHandle_t play_task_handler;
    audio_ctrl->player_task_working = true;
    xTaskCreate(player_task, "play_task_work", CONFIG_MAIN_TASK_STACK_SIZE,
                &play_task_handler, 18, NULL);

    audio_ctrl->player_status |= 0x80;

    /* 启动解码任务流 */
    audio_work_pipeline_start();

    return 0;
}

err_gpio_if:
      audio_codec_delete_ctrl_if(audio_ctrl->ctrl_if);
      audio_ctrl->ctrl_if = NULL;
err_ctrl_if:
      audio_codec_delete_data_if(audio_ctrl->data_if);
      audio_ctrl->data_if = NULL;
      return;
}

void player_stop() {
    ESP_LOGI(TAG, "player_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);

    /* clear draining flag */
    audio_ctrl->player_status &= 0xFB;

    if ((audio_ctrl->player_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->player_status &= 0xF7;
    }

    buffer_pool_reset(&(audio_ctrl->decoder_in_pool));
    buffer_pool_reset(&(audio_ctrl->player_pool));

    ESP_LOGI(TAG, "player_stop done, status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);
}

void player_drain() {
  ESP_LOGI(TAG, "player_drain status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  audio_ctrl->player_status |= 0x04;
  if ((audio_ctrl->player_status & 0x40) == 0x40) {
    drain_data(&(audio_ctrl->decoder_in_pool));
  } else {
    drain_data(&(audio_ctrl->player_pool));
  }
}

size_t player_insert_data(uint8_t *data, size_t data_size) {
  // ESP_LOGV(TAG, "player_insert_data status: 0x%x/0x%x, data_size:%d",
  //          audio_ctrl->codec_status, audio_ctrl->player_status, data_size);
  int written_data = 0;
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    if ((audio_ctrl->player_status & 0x40) == 0x40) {
      written_data = write_data(&(audio_ctrl->decoder_in_pool),
                                (const unsigned char *)data, data_size);
    } else {
      if ((audio_ctrl->player_status & 0x20) == 0x20) {
        written_data = write_data(&(audio_ctrl->player_pool),
                                  (const unsigned char *)data, data_size);
      }
    }
  }
  if (written_data < 0) {
    ESP_LOGE(TAG, "Write data into pool failed:%d", written_data);
    written_data = 0;
  }
  return written_data;
}

void recorder_start() {
    xSemaphoreTakeRecursive(audio_ctrl->recorder_lock, portMAX_DELAY);
    ESP_LOGI(TAG, "recoder_start status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    recorder_chip_start();

    if (audio_ctrl->codec_dev != NULL) {
        esp_codec_dev_open(audio_ctrl->codec_dev, ESP_CODEC_DEV_WORK_MODE_RX);
        audio_ctrl->recorder_status |= 0x08;
    }

    ESP_LOGI(TAG, "recoder_start done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
    xSemaphoreGiveRecursive(audio_ctrl->recorder_lock);
}

void recorder_stop() {
    ESP_LOGI(TAG, "recorder_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    if ((audio_ctrl->recorder_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->recorder_status &= 0xF7;
    }

    ESP_LOGI(TAG, "recoder_stop done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
}

size_t player_hw_play(uint8_t *data, size_t data_size) {
  ESP_LOGV(TAG, "player_hw_play status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    return i2s_tx_write(data, data_size);
  } else {
    return 0;
  }
}

int player_init(audio_ctrl_t *audio) {
    audio_ctrl = audio;
    audio_ctrl->player_lock = xSemaphoreCreateRecursiveMutex();
    xSemaphoreGiveRecursive(audio_ctrl->player_lock);

    if (audio_ctrl->decoder_sr == 0) {
        audio_ctrl->decoder_sr = DEFAULT_AUDIO_CTRL_SR;
    }
    if (audio_ctrl->player_sr == 0) {
        audio_ctrl->player_sr = DEFAULT_AUDIO_CTRL_SR;
    }

    /* 创建解码器和播放器缓存, 创建解码任务流 */
    audio_pipeline_init(audio_ctrl->decoder_format);

    /* 创建送数据到播放器的任务 */
    TaskHandle_t play_task_handler;
    audio_ctrl->player_task_working = true;
    xTaskCreate(player_task, "play_task_work", CONFIG_MAIN_TASK_STACK_SIZE,
                &play_task_handler, 18, NULL);

    audio_ctrl->player_status |= 0x80;

    /* 启动解码任务流 */
    audio_work_pipeline_start();

    return 0;
}

err_gpio_if:
      audio_codec_delete_ctrl_if(audio_ctrl->ctrl_if);
      audio_ctrl->ctrl_if = NULL;
err_ctrl_if:
      audio_codec_delete_data_if(audio_ctrl->data_if);
      audio_ctrl->data_if = NULL;
      return;
}

void player_stop() {
    ESP_LOGI(TAG, "player_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);

    /* clear draining flag */
    audio_ctrl->player_status &= 0xFB;

    if ((audio_ctrl->player_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->player_status &= 0xF7;
    }

    buffer_pool_reset(&(audio_ctrl->decoder_in_pool));
    buffer_pool_reset(&(audio_ctrl->player_pool));

    ESP_LOGI(TAG, "player_stop done, status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);
}

void player_drain() {
  ESP_LOGI(TAG, "player_drain status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  audio_ctrl->player_status |= 0x04;
  if ((audio_ctrl->player_status & 0x40) == 0x40) {
    drain_data(&(audio_ctrl->decoder_in_pool));
  } else {
    drain_data(&(audio_ctrl->player_pool));
  }
}

size_t player_insert_data(uint8_t *data, size_t data_size) {
  // ESP_LOGV(TAG, "player_insert_data status: 0x%x/0x%x, data_size:%d",
  //          audio_ctrl->codec_status, audio_ctrl->player_status, data_size);
  int written_data = 0;
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    if ((audio_ctrl->player_status & 0x40) == 0x40) {
      written_data = write_data(&(audio_ctrl->decoder_in_pool),
                                (const unsigned char *)data, data_size);
    } else {
      if ((audio_ctrl->player_status & 0x20) == 0x20) {
        written_data = write_data(&(audio_ctrl->player_pool),
                                  (const unsigned char *)data, data_size);
      }
    }
  }
  if (written_data < 0) {
    ESP_LOGE(TAG, "Write data into pool failed:%d", written_data);
    written_data = 0;
  }
  return written_data;
}

void recorder_start() {
    xSemaphoreTakeRecursive(audio_ctrl->recorder_lock, portMAX_DELAY);
    ESP_LOGI(TAG, "recoder_start status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    recorder_chip_start();

    if (audio_ctrl->codec_dev != NULL) {
        esp_codec_dev_open(audio_ctrl->codec_dev, ESP_CODEC_DEV_WORK_MODE_RX);
        audio_ctrl->recorder_status |= 0x08;
    }

    ESP_LOGI(TAG, "recoder_start done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
    xSemaphoreGiveRecursive(audio_ctrl->recorder_lock);
}

void recorder_stop() {
    ESP_LOGI(TAG, "recorder_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    if ((audio_ctrl->recorder_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->recorder_status &= 0xF7;
    }

    ESP_LOGI(TAG, "recoder_stop done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
}

size_t player_hw_play(uint8_t *data, size_t data_size) {
  ESP_LOGV(TAG, "player_hw_play status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    return i2s_tx_write(data, data_size);
  } else {
    return 0;
  }
}

int player_init(audio_ctrl_t *audio) {
    audio_ctrl = audio;
    audio_ctrl->player_lock = xSemaphoreCreateRecursiveMutex();
    xSemaphoreGiveRecursive(audio_ctrl->player_lock);

    if (audio_ctrl->decoder_sr == 0) {
        audio_ctrl->decoder_sr = DEFAULT_AUDIO_CTRL_SR;
    }
    if (audio_ctrl->player_sr == 0) {
        audio_ctrl->player_sr = DEFAULT_AUDIO_CTRL_SR;
    }

    /* 创建解码器和播放器缓存, 创建解码任务流 */
    audio_pipeline_init(audio_ctrl->decoder_format);

    /* 创建送数据到播放器的任务 */
    TaskHandle_t play_task_handler;
    audio_ctrl->player_task_working = true;
    xTaskCreate(player_task, "play_task_work", CONFIG_MAIN_TASK_STACK_SIZE,
                &play_task_handler, 18, NULL);

    audio_ctrl->player_status |= 0x80;

    /* 启动解码任务流 */
    audio_work_pipeline_start();

    return 0;
}

err_gpio_if:
      audio_codec_delete_ctrl_if(audio_ctrl->ctrl_if);
      audio_ctrl->ctrl_if = NULL;
err_ctrl_if:
      audio_codec_delete_data_if(audio_ctrl->data_if);
      audio_ctrl->data_if = NULL;
      return;
}

void player_stop() {
    ESP_LOGI(TAG, "player_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);

    /* clear draining flag */
    audio_ctrl->player_status &= 0xFB;

    if ((audio_ctrl->player_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->player_status &= 0xF7;
    }

    buffer_pool_reset(&(audio_ctrl->decoder_in_pool));
    buffer_pool_reset(&(audio_ctrl->player_pool));

    ESP_LOGI(TAG, "player_stop done, status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);
}

void player_drain() {
  ESP_LOGI(TAG, "player_drain status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  audio_ctrl->player_status |= 0x04;
  if ((audio_ctrl->player_status & 0x40) == 0x40) {
    drain_data(&(audio_ctrl->decoder_in_pool));
  } else {
    drain_data(&(audio_ctrl->player_pool));
  }
}

size_t player_insert_data(uint8_t *data, size_t data_size) {
  // ESP_LOGV(TAG, "player_insert_data status: 0x%x/0x%x, data_size:%d",
  //          audio_ctrl->codec_status, audio_ctrl->player_status, data_size);
  int written_data = 0;
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    if ((audio_ctrl->player_status & 0x40) == 0x40) {
      written_data = write_data(&(audio_ctrl->decoder_in_pool),
                                (const unsigned char *)data, data_size);
    } else {
      if ((audio_ctrl->player_status & 0x20) == 0x20) {
        written_data = write_data(&(audio_ctrl->player_pool),
                                  (const unsigned char *)data, data_size);
      }
    }
  }
  if (written_data < 0) {
    ESP_LOGE(TAG, "Write data into pool failed:%d", written_data);
    written_data = 0;
  }
  return written_data;
}

void recorder_start() {
    xSemaphoreTakeRecursive(audio_ctrl->recorder_lock, portMAX_DELAY);
    ESP_LOGI(TAG, "recoder_start status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    recorder_chip_start();

    if (audio_ctrl->codec_dev != NULL) {
        esp_codec_dev_open(audio_ctrl->codec_dev, ESP_CODEC_DEV_WORK_MODE_RX);
        audio_ctrl->recorder_status |= 0x08;
    }

    ESP_LOGI(TAG, "recoder_start done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
    xSemaphoreGiveRecursive(audio_ctrl->recorder_lock);
}

void recorder_stop() {
    ESP_LOGI(TAG, "recorder_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    if ((audio_ctrl->recorder_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->recorder_status &= 0xF7;
    }

    ESP_LOGI(TAG, "recoder_stop done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
}

size_t player_hw_play(uint8_t *data, size_t data_size) {
  ESP_LOGV(TAG, "player_hw_play status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    return i2s_tx_write(data, data_size);
  } else {
    return 0;
  }
}

int player_init(audio_ctrl_t *audio) {
    audio_ctrl = audio;
    audio_ctrl->player_lock = xSemaphoreCreateRecursiveMutex();
    xSemaphoreGiveRecursive(audio_ctrl->player_lock);

    if (audio_ctrl->decoder_sr == 0) {
        audio_ctrl->decoder_sr = DEFAULT_AUDIO_CTRL_SR;
    }
    if (audio_ctrl->player_sr == 0) {
        audio_ctrl->player_sr = DEFAULT_AUDIO_CTRL_SR;
    }

    /* 创建解码器和播放器缓存, 创建解码任务流 */
    audio_pipeline_init(audio_ctrl->decoder_format);

    /* 创建送数据到播放器的任务 */
    TaskHandle_t play_task_handler;
    audio_ctrl->player_task_working = true;
    xTaskCreate(player_task, "play_task_work", CONFIG_MAIN_TASK_STACK_SIZE,
                &play_task_handler, 18, NULL);

    audio_ctrl->player_status |= 0x80;

    /* 启动解码任务流 */
    audio_work_pipeline_start();

    return 0;
}

err_gpio_if:
      audio_codec_delete_ctrl_if(audio_ctrl->ctrl_if);
      audio_ctrl->ctrl_if = NULL;
err_ctrl_if:
      audio_codec_delete_data_if(audio_ctrl->data_if);
      audio_ctrl->data_if = NULL;
      return;
}

void player_stop() {
    ESP_LOGI(TAG, "player_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);

    /* clear draining flag */
    audio_ctrl->player_status &= 0xFB;

    if ((audio_ctrl->player_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->player_status &= 0xF7;
    }

    buffer_pool_reset(&(audio_ctrl->decoder_in_pool));
    buffer_pool_reset(&(audio_ctrl->player_pool));

    ESP_LOGI(TAG, "player_stop done, status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);
}

void player_drain() {
  ESP_LOGI(TAG, "player_drain status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  audio_ctrl->player_status |= 0x04;
  if ((audio_ctrl->player_status & 0x40) == 0x40) {
    drain_data(&(audio_ctrl->decoder_in_pool));
  } else {
    drain_data(&(audio_ctrl->player_pool));
  }
}

size_t player_insert_data(uint8_t *data, size_t data_size) {
  // ESP_LOGV(TAG, "player_insert_data status: 0x%x/0x%x, data_size:%d",
  //          audio_ctrl->codec_status, audio_ctrl->player_status, data_size);
  int written_data = 0;
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    if ((audio_ctrl->player_status & 0x40) == 0x40) {
      written_data = write_data(&(audio_ctrl->decoder_in_pool),
                                (const unsigned char *)data, data_size);
    } else {
      if ((audio_ctrl->player_status & 0x20) == 0x20) {
        written_data = write_data(&(audio_ctrl->player_pool),
                                  (const unsigned char *)data, data_size);
      }
    }
  }
  if (written_data < 0) {
    ESP_LOGE(TAG, "Write data into pool failed:%d", written_data);
    written_data = 0;
  }
  return written_data;
}

void recorder_start() {
    xSemaphoreTakeRecursive(audio_ctrl->recorder_lock, portMAX_DELAY);
    ESP_LOGI(TAG, "recoder_start status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    recorder_chip_start();

    if (audio_ctrl->codec_dev != NULL) {
        esp_codec_dev_open(audio_ctrl->codec_dev, ESP_CODEC_DEV_WORK_MODE_RX);
        audio_ctrl->recorder_status |= 0x08;
    }

    ESP_LOGI(TAG, "recoder_start done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
    xSemaphoreGiveRecursive(audio_ctrl->recorder_lock);
}

void recorder_stop() {
    ESP_LOGI(TAG, "recorder_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    if ((audio_ctrl->recorder_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->recorder_status &= 0xF7;
    }

    ESP_LOGI(TAG, "recoder_stop done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
}

size_t player_hw_play(uint8_t *data, size_t data_size) {
  ESP_LOGV(TAG, "player_hw_play status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    return i2s_tx_write(data, data_size);
  } else {
    return 0;
  }
}

int player_init(audio_ctrl_t *audio) {
    audio_ctrl = audio;
    audio_ctrl->player_lock = xSemaphoreCreateRecursiveMutex();
    xSemaphoreGiveRecursive(audio_ctrl->player_lock);

    if (audio_ctrl->decoder_sr == 0) {
        audio_ctrl->decoder_sr = DEFAULT_AUDIO_CTRL_SR;
    }
    if (audio_ctrl->player_sr == 0) {
        audio_ctrl->player_sr = DEFAULT_AUDIO_CTRL_SR;
    }

    /* 创建解码器和播放器缓存, 创建解码任务流 */
    audio_pipeline_init(audio_ctrl->decoder_format);

    /* 创建送数据到播放器的任务 */
    TaskHandle_t play_task_handler;
    audio_ctrl->player_task_working = true;
    xTaskCreate(player_task, "play_task_work", CONFIG_MAIN_TASK_STACK_SIZE,
                &play_task_handler, 18, NULL);

    audio_ctrl->player_status |= 0x80;

    /* 启动解码任务流 */
    audio_work_pipeline_start();

    return 0;
}

err_gpio_if:
      audio_codec_delete_ctrl_if(audio_ctrl->ctrl_if);
      audio_ctrl->ctrl_if = NULL;
err_ctrl_if:
      audio_codec_delete_data_if(audio_ctrl->data_if);
      audio_ctrl->data_if = NULL;
      return;
}

void player_stop() {
    ESP_LOGI(TAG, "player_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);

    /* clear draining flag */
    audio_ctrl->player_status &= 0xFB;

    if ((audio_ctrl->player_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->player_status &= 0xF7;
    }

    buffer_pool_reset(&(audio_ctrl->decoder_in_pool));
    buffer_pool_reset(&(audio_ctrl->player_pool));

    ESP_LOGI(TAG, "player_stop done, status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);
}

void player_drain() {
  ESP_LOGI(TAG, "player_drain status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  audio_ctrl->player_status |= 0x04;
  if ((audio_ctrl->player_status & 0x40) == 0x40) {
    drain_data(&(audio_ctrl->decoder_in_pool));
  } else {
    drain_data(&(audio_ctrl->player_pool));
  }
}

size_t player_insert_data(uint8_t *data, size_t data_size) {
  // ESP_LOGV(TAG, "player_insert_data status: 0x%x/0x%x, data_size:%d",
  //          audio_ctrl->codec_status, audio_ctrl->player_status, data_size);
  int written_data = 0;
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    if ((audio_ctrl->player_status & 0x40) == 0x40) {
      written_data = write_data(&(audio_ctrl->decoder_in_pool),
                                (const unsigned char *)data, data_size);
    } else {
      if ((audio_ctrl->player_status & 0x20) == 0x20) {
        written_data = write_data(&(audio_ctrl->player_pool),
                                  (const unsigned char *)data, data_size);
      }
    }
  }
  if (written_data < 0) {
    ESP_LOGE(TAG, "Write data into pool failed:%d", written_data);
    written_data = 0;
  }
  return written_data;
}

void recorder_start() {
    xSemaphoreTakeRecursive(audio_ctrl->recorder_lock, portMAX_DELAY);
    ESP_LOGI(TAG, "recoder_start status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    recorder_chip_start();

    if (audio_ctrl->codec_dev != NULL) {
        esp_codec_dev_open(audio_ctrl->codec_dev, ESP_CODEC_DEV_WORK_MODE_RX);
        audio_ctrl->recorder_status |= 0x08;
    }

    ESP_LOGI(TAG, "recoder_start done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
    xSemaphoreGiveRecursive(audio_ctrl->recorder_lock);
}

void recorder_stop() {
    ESP_LOGI(TAG, "recorder_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    if ((audio_ctrl->recorder_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->recorder_status &= 0xF7;
    }

    ESP_LOGI(TAG, "recoder_stop done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
}

size_t player_hw_play(uint8_t *data, size_t data_size) {
  ESP_LOGV(TAG, "player_hw_play status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    return i2s_tx_write(data, data_size);
  } else {
    return 0;
  }
}

int player_init(audio_ctrl_t *audio) {
    audio_ctrl = audio;
    audio_ctrl->player_lock = xSemaphoreCreateRecursiveMutex();
    xSemaphoreGiveRecursive(audio_ctrl->player_lock);

    if (audio_ctrl->decoder_sr == 0) {
        audio_ctrl->decoder_sr = DEFAULT_AUDIO_CTRL_SR;
    }
    if (audio_ctrl->player_sr == 0) {
        audio_ctrl->player_sr = DEFAULT_AUDIO_CTRL_SR;
    }

    /* 创建解码器和播放器缓存, 创建解码任务流 */
    audio_pipeline_init(audio_ctrl->decoder_format);

    /* 创建送数据到播放器的任务 */
    TaskHandle_t play_task_handler;
    audio_ctrl->player_task_working = true;
    xTaskCreate(player_task, "play_task_work", CONFIG_MAIN_TASK_STACK_SIZE,
                &play_task_handler, 18, NULL);

    audio_ctrl->player_status |= 0x80;

    /* 启动解码任务流 */
    audio_work_pipeline_start();

    return 0;
}

err_gpio_if:
      audio_codec_delete_ctrl_if(audio_ctrl->ctrl_if);
      audio_ctrl->ctrl_if = NULL;
err_ctrl_if:
      audio_codec_delete_data_if(audio_ctrl->data_if);
      audio_ctrl->data_if = NULL;
      return;
}

void player_stop() {
    ESP_LOGI(TAG, "player_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);

    /* clear draining flag */
    audio_ctrl->player_status &= 0xFB;

    if ((audio_ctrl->player_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->player_status &= 0xF7;
    }

    buffer_pool_reset(&(audio_ctrl->decoder_in_pool));
    buffer_pool_reset(&(audio_ctrl->player_pool));

    ESP_LOGI(TAG, "player_stop done, status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);
}

void player_drain() {
  ESP_LOGI(TAG, "player_drain status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  audio_ctrl->player_status |= 0x04;
  if ((audio_ctrl->player_status & 0x40) == 0x40) {
    drain_data(&(audio_ctrl->decoder_in_pool));
  } else {
    drain_data(&(audio_ctrl->player_pool));
  }
}

size_t player_insert_data(uint8_t *data, size_t data_size) {
  // ESP_LOGV(TAG, "player_insert_data status: 0x%x/0x%x, data_size:%d",
  //          audio_ctrl->codec_status, audio_ctrl->player_status, data_size);
  int written_data = 0;
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    if ((audio_ctrl->player_status & 0x40) == 0x40) {
      written_data = write_data(&(audio_ctrl->decoder_in_pool),
                                (const unsigned char *)data, data_size);
    } else {
      if ((audio_ctrl->player_status & 0x20) == 0x20) {
        written_data = write_data(&(audio_ctrl->player_pool),
                                  (const unsigned char *)data, data_size);
      }
    }
  }
  if (written_data < 0) {
    ESP_LOGE(TAG, "Write data into pool failed:%d", written_data);
    written_data = 0;
  }
  return written_data;
}

void recorder_start() {
    xSemaphoreTakeRecursive(audio_ctrl->recorder_lock, portMAX_DELAY);
    ESP_LOGI(TAG, "recoder_start status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    recorder_chip_start();

    if (audio_ctrl->codec_dev != NULL) {
        esp_codec_dev_open(audio_ctrl->codec_dev, ESP_CODEC_DEV_WORK_MODE_RX);
        audio_ctrl->recorder_status |= 0x08;
    }

    ESP_LOGI(TAG, "recoder_start done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
    xSemaphoreGiveRecursive(audio_ctrl->recorder_lock);
}

void recorder_stop() {
    ESP_LOGI(TAG, "recorder_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    if ((audio_ctrl->recorder_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->recorder_status &= 0xF7;
    }

    ESP_LOGI(TAG, "recoder_stop done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
}

size_t player_hw_play(uint8_t *data, size_t data_size) {
  ESP_LOGV(TAG, "player_hw_play status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    return i2s_tx_write(data, data_size);
  } else {
    return 0;
  }
}

int player_init(audio_ctrl_t *audio) {
    audio_ctrl = audio;
    audio_ctrl->player_lock = xSemaphoreCreateRecursiveMutex();
    xSemaphoreGiveRecursive(audio_ctrl->player_lock);

    if (audio_ctrl->decoder_sr == 0) {
        audio_ctrl->decoder_sr = DEFAULT_AUDIO_CTRL_SR;
    }
    if (audio_ctrl->player_sr == 0) {
        audio_ctrl->player_sr = DEFAULT_AUDIO_CTRL_SR;
    }

    /* 创建解码器和播放器缓存, 创建解码任务流 */
    audio_pipeline_init(audio_ctrl->decoder_format);

    /* 创建送数据到播放器的任务 */
    TaskHandle_t play_task_handler;
    audio_ctrl->player_task_working = true;
    xTaskCreate(player_task, "play_task_work", CONFIG_MAIN_TASK_STACK_SIZE,
                &play_task_handler, 18, NULL);

    audio_ctrl->player_status |= 0x80;

    /* 启动解码任务流 */
    audio_work_pipeline_start();

    return 0;
}

err_gpio_if:
      audio_codec_delete_ctrl_if(audio_ctrl->ctrl_if);
      audio_ctrl->ctrl_if = NULL;
err_ctrl_if:
      audio_codec_delete_data_if(audio_ctrl->data_if);
      audio_ctrl->data_if = NULL;
      return;
}

void player_stop() {
    ESP_LOGI(TAG, "player_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);

    /* clear draining flag */
    audio_ctrl->player_status &= 0xFB;

    if ((audio_ctrl->player_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->player_status &= 0xF7;
    }

    buffer_pool_reset(&(audio_ctrl->decoder_in_pool));
    buffer_pool_reset(&(audio_ctrl->player_pool));

    ESP_LOGI(TAG, "player_stop done, status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);
}

void player_drain() {
  ESP_LOGI(TAG, "player_drain status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  audio_ctrl->player_status |= 0x04;
  if ((audio_ctrl->player_status & 0x40) == 0x40) {
    drain_data(&(audio_ctrl->decoder_in_pool));
  } else {
    drain_data(&(audio_ctrl->player_pool));
  }
}

size_t player_insert_data(uint8_t *data, size_t data_size) {
  // ESP_LOGV(TAG, "player_insert_data status: 0x%x/0x%x, data_size:%d",
  //          audio_ctrl->codec_status, audio_ctrl->player_status, data_size);
  int written_data = 0;
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    if ((audio_ctrl->player_status & 0x40) == 0x40) {
      written_data = write_data(&(audio_ctrl->decoder_in_pool),
                                (const unsigned char *)data, data_size);
    } else {
      if ((audio_ctrl->player_status & 0x20) == 0x20) {
        written_data = write_data(&(audio_ctrl->player_pool),
                                  (const unsigned char *)data, data_size);
      }
    }
  }
  if (written_data < 0) {
    ESP_LOGE(TAG, "Write data into pool failed:%d", written_data);
    written_data = 0;
  }
  return written_data;
}

void recorder_start() {
    xSemaphoreTakeRecursive(audio_ctrl->recorder_lock, portMAX_DELAY);
    ESP_LOGI(TAG, "recoder_start status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    recorder_chip_start();

    if (audio_ctrl->codec_dev != NULL) {
        esp_codec_dev_open(audio_ctrl->codec_dev, ESP_CODEC_DEV_WORK_MODE_RX);
        audio_ctrl->recorder_status |= 0x08;
    }

    ESP_LOGI(TAG, "recoder_start done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
    xSemaphoreGiveRecursive(audio_ctrl->recorder_lock);
}

void recorder_stop() {
    ESP_LOGI(TAG, "recorder_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    if ((audio_ctrl->recorder_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->recorder_status &= 0xF7;
    }

    ESP_LOGI(TAG, "recoder_stop done, status: 0x%x/0x%x",
             audio_ctrl->codec_status, audio_ctrl->recorder_status);
}

size_t player_hw_play(uint8_t *data, size_t data_size) {
  ESP_LOGV(TAG, "player_hw_play status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    return i2s_tx_write(data, data_size);
  } else {
    return 0;
  }
}

int player_init(audio_ctrl_t *audio) {
    audio_ctrl = audio;
    audio_ctrl->player_lock = xSemaphoreCreateRecursiveMutex();
    xSemaphoreGiveRecursive(audio_ctrl->player_lock);

    if (audio_ctrl->decoder_sr == 0) {
        audio_ctrl->decoder_sr = DEFAULT_AUDIO_CTRL_SR;
    }
    if (audio_ctrl->player_sr == 0) {
        audio_ctrl->player_sr = DEFAULT_AUDIO_CTRL_SR;
    }

    /* 创建解码器和播放器缓存, 创建解码任务流 */
    audio_pipeline_init(audio_ctrl->decoder_format);

    /* 创建送数据到播放器的任务 */
    TaskHandle_t play_task_handler;
    audio_ctrl->player_task_working = true;
    xTaskCreate(player_task, "play_task_work", CONFIG_MAIN_TASK_STACK_SIZE,
                &play_task_handler, 18, NULL);

    audio_ctrl->player_status |= 0x80;

    /* 启动解码任务流 */
    audio_work_pipeline_start();

    return 0;
}

err_gpio_if:
      audio_codec_delete_ctrl_if(audio_ctrl->ctrl_if);
      audio_ctrl->ctrl_if = NULL;
err_ctrl_if:
      audio_codec_delete_data_if(audio_ctrl->data_if);
      audio_ctrl->data_if = NULL;
      return;
}

void player_stop() {
    ESP_LOGI(TAG, "player_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);

    /* clear draining flag */
    audio_ctrl->player_status &= 0xFB;

    if ((audio_ctrl->player_status & 0x08) == 0x08) {
        esp_codec_dev_close(audio_ctrl->codec_dev);
        audio_ctrl->player_status &= 0xF7;
    }

    buffer_pool_reset(&(audio_ctrl->decoder_in_pool));
    buffer_pool_reset(&(audio_ctrl->player_pool));

    ESP_LOGI(TAG, "player_stop done, status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->player_status);
}

void player_drain() {
  ESP_LOGI(TAG, "player_drain status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  audio_ctrl->player_status |= 0x04;
  if ((audio_ctrl->player_status & 0x40) == 0x40) {
    drain_data(&(audio_ctrl->decoder_in_pool));
  } else {
    drain_data(&(audio_ctrl->player_pool));
  }
}

size_t player_insert_data(uint8_t *data, size_t data_size) {
  // ESP_LOGV(TAG, "player_insert_data status: 0x%x/0x%x, data_size:%d",
  //          audio_ctrl->codec_status, audio_ctrl->player_status, data_size);
  int written_data = 0;
  if ((audio_ctrl->player_status & 0x08) == 0x08) {
    if ((audio_ctrl->player_status & 0x40) == 0x40) {
      written_data = write_data(&(audio_ctrl->decoder_in_pool),
                                (const unsigned char *)data, data_size);
    } else {
      if ((audio_ctrl->player_status & 0x20) == 0x20) {
        written_data = write_data(&(audio_ctrl->player_pool),
                                  (const unsigned char *)data, data_size);
      }
    }
  }
  if (written_data < 0) {
    ESP_LOGE(TAG, "Write data into pool failed:%d", written_data);
    written_data = 0;
  }
  return written_data;
}

void recorder_start() {
    xSemaphoreTakeRecursive(audio_ctrl->recorder_lock, portMAX_DELAY);
    ESP_LOGI(TAG, "recoder_start status: 0x%x/0x%x", audio_ctrl->codec_status,
             audio_ctrl->recorder_status);

    recorder_chip_start();

    if (audio_ctrl->codec_dev != NULL) {
        esp_codec_dev_open(audio_ctrl->codec_dev, ESP_