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

int recorder_init(audio_ctrl_t *audio)
{
  audio_ctrl = audio;
  audio_ctrl->recorder_lock = xSemaphoreCreateRecursiveMutex();
  xSemaphoreGiveRecursive(audio_ctrl->recorder_lock);

  if (audio_ctrl->recorder_sr == 0)
  {
    audio_ctrl->recorder_sr = DEFAULT_AUDIO_CTRL_SR;
  }

  audio_ctrl->recorder_status |= 0x80;
  return 0;
}

int recorder_deinit()
{
  audio_ctrl->recorder_status &= 0x7F;
  return 0;
}

int recorder_reset() { return 0; }

#define USER_ES8311

#ifdef ATK_ES8388
static void recorder_chip_start()
{
  es8388_adda_cfg(0, 1);    /* 开启ADC */
  es8388_input_cfg(0);      /* 开启输入通道(通道1,MIC所在通道) */
  es8388_mic_gain(8);       /* MIC增益设置为最大 */
  es8388_alc_ctrl(1, 6, 6); /* 开启右通道ALC控制,以提高录音音量 */
  es8388_output_cfg(0, 0);  /* 关闭通道1和2的输出 */
  es8388_spkvol_set(0);     /* 关闭喇叭. */
  es8388_i2s_cfg(0, 3);     /* 飞利浦标准,16位数据长度 */
}
#endif

#ifdef USER_ES8311
static void recorder_chip_start()
{
}
#endif

void recorder_start()
{
  xSemaphoreTakeRecursive(audio_ctrl->recorder_lock, portMAX_DELAY);
  ESP_LOGI(TAG, "recoder_start status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->recorder_status);
  if ((audio_ctrl->codec_status & 0x01) == 0x00)
  {
    //设置输入的采样率通过协议设置的目前16k
    ESP_LOGI(TAG, "audio_ctrl->recorder_sr: %d", (int)audio_ctrl->recorder_sr);
    myi2s_init(audio_ctrl->recorder_sr); /* 初始化用于录音和播放的i2s */
    audio_ctrl->codec_status |= 0x01;
  }

  recorder_chip_start();

  if ((audio_ctrl->recorder_status & 0x08) == 0x00)
  {
    i2s_rx_start();
    audio_ctrl->recorder_status |= 0x08;
  }
  if ((audio_ctrl->player_status & 0x08) == 0x00)
  {
    i2s_tx_start();
    audio_ctrl->player_status |= 0x08;
  }

  ESP_LOGI(TAG, "recoder_start done, status: 0x%x/0x%x",audio_ctrl->codec_status, audio_ctrl->recorder_status);
  xSemaphoreGiveRecursive(audio_ctrl->recorder_lock);
}

void recorder_stop()
{
  ESP_LOGI(TAG, "recorder_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->recorder_status);
  if ((audio_ctrl->recorder_status & 0x08) == 0x08)
  {
    audio_ctrl->recorder_status &= 0xF7;
    i2s_rx_stop();
  }
  if ((audio_ctrl->codec_status & 0x01) == 0x01 &&
      (audio_ctrl->recorder_status & 0x08) == 0x00 &&
      (audio_ctrl->player_status & 0x08) == 0x00)
  {
    audio_ctrl->codec_status &= 0xFE;
    i2s_deinit();
  }
  ESP_LOGI(TAG, "recoder_stop done, status: 0x%x/0x%x",
           audio_ctrl->codec_status, audio_ctrl->recorder_status);
}

size_t recorder_fetch_data(uint8_t *data, size_t data_size)
{
  if ((audio_ctrl->codec_status & 0x01) == 0x01 &&
      (audio_ctrl->recorder_status & 0x08) == 0x08)
  {
    return i2s_rx_read(data, data_size);
  }
  else
  {
    return 0;
  }
}

void recorder_new_pathname(const char *dname, uint8_t *pname)
{
  uint8_t res;
  uint16_t index = 0;
  FIL *ftemp;
  ftemp = (FIL *)malloc(sizeof(FIL)); /* 开辟FIL字节的内存区域 */

  if (ftemp == NULL)
  {
    return; /* 内存申请失败 */
  }

  while (index < 0xFFFF)
  {
    sprintf((char *)pname, "%s/REC%05d.pcm", dname, index);
    res = f_open(ftemp, (const TCHAR *)pname, FA_READ); /* 尝试打开这个文件 */

    if (res == FR_NO_FILE)
    {
      break; /* 该文件名不存在=正是我们需要的. */
    }

    index++;
  }

  free(ftemp);
}

/* -- PLAYER --*/
static int audio_decoder_read_cb(audio_element_handle_t el, char *buf, int len,
                                 TickType_t wait_time, void *ctx)
{
  ESP_LOGV(TAG,
           "start audio_decoder_read_cb to get audio data with player status "
           "0x%x ...",
           audio_ctrl->player_status);
  int read_size = 0;
  // 播放器运行则等待获得数据
  while (read_size <= 0 && ((audio_ctrl->player_status & 0x80) == 0x80))
  {
    read_size =
        read_data(&(audio_ctrl->decoder_in_pool), (unsigned char *)buf, len);
    if (read_size > 0)
    {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  if (read_size == 0)
  {
    ESP_LOGW(TAG, "no decoding data in decoder_in_pool");
    if ((audio_ctrl->player_status & 0x40) == 0x40)
    {
      drain_data(&(audio_ctrl->player_pool));
    }
    return AEL_IO_DONE;
  }
  else
  {
    return read_size;
  }
}

static int audio_player_pcm_write_cb(audio_element_handle_t el, char *buf,
                                     int len, TickType_t wait_time, void *ctx)
{
  ESP_LOGV(TAG,
           "start audio_player_pcm_write_cb write pcm data with player status "
           "0x%x ...",
           audio_ctrl->player_status);
  int written_bytes = 0;
  uint8_t try_count = 3;
  while (written_bytes == 0 && try_count-- > 0)
  {
    written_bytes =
        write_data(&(audio_ctrl->player_pool), (const unsigned char *)buf, len);
    if (written_bytes == 0)
    {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
  return written_bytes;
}

static int audio_work_pipeline_init(char *format)
{
  /* mp3的缓存因为解码比较快, 可以设置小一点 */
  uint8_t cache1_multiple = 10;
  uint8_t cache1_limit_multiple = 0; // nodelay
  uint8_t cache2_multiple = 20;
  buffer_pool_init(
      &(audio_ctrl->decoder_in_pool), "DECODER_POOL",
      PLA_TX_BUFSIZE * cache1_multiple, PLA_TX_BUFSIZE * cache1_limit_multiple,
      PLA_TX_BUFSIZE * cache2_multiple, audio_ctrl->sdcard_decoding_dir,
      audio_ctrl->sdcard_decoding_base, audio_ctrl->decoder_format, audio_ctrl->enable_using_sdcard_cache);
  audio_ctrl->player_status |= 0x40;

  /* 播放音频因为播放缓慢, 需要设置大一点 */
  cache1_multiple = 50;
  cache1_limit_multiple = 0; // nodelay
  cache2_multiple = 40;
  buffer_pool_init(
      &(audio_ctrl->player_pool), "PLAYER_POOL",
      PLA_TX_BUFSIZE * cache1_multiple, PLA_TX_BUFSIZE * cache1_limit_multiple,
      PLA_TX_BUFSIZE * cache2_multiple, audio_ctrl->sdcard_player_dir,
      audio_ctrl->sdcard_player_base, "pcm", audio_ctrl->enable_using_sdcard_cache);
  audio_ctrl->player_status |= 0x20;

  audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
  audio_ctrl->audio_work_pipeline = audio_pipeline_init(&pipeline_cfg);
  mem_assert(audio_ctrl->audio_work_pipeline);

  mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
  audio_ctrl->audio_decoder = mp3_decoder_init(&mp3_cfg);
  audio_element_set_read_cb(audio_ctrl->audio_decoder, audio_decoder_read_cb,
                            NULL);

  audio_ctrl->decoder_outbuf = rb_create(8 * 1024, 1);
  audio_element_set_output_ringbuf(audio_ctrl->audio_decoder,
                                   audio_ctrl->decoder_outbuf);
  audio_pipeline_register(audio_ctrl->audio_work_pipeline,
                          audio_ctrl->audio_decoder, "mp3_decoder");

  rsp_filter_cfg_t rsp_cfg_w = DEFAULT_RESAMPLE_FILTER_CONFIG();
  rsp_cfg_w.src_rate = audio_ctrl->decoder_sr;
  rsp_cfg_w.src_ch = 1;
  rsp_cfg_w.dest_rate = audio_ctrl->player_sr;
  rsp_cfg_w.dest_ch = 1;
  rsp_cfg_w.complexity = 5;
  audio_ctrl->player_filter = rsp_filter_init(&rsp_cfg_w);
  audio_element_set_write_cb(audio_ctrl->player_filter,
                             audio_player_pcm_write_cb, NULL);
  audio_pipeline_register(audio_ctrl->audio_work_pipeline,
                          audio_ctrl->player_filter, "filter_w");

  const char *link_tag[2] = {"mp3_decoder", "filter_w"};
  audio_pipeline_link(audio_ctrl->audio_work_pipeline, &link_tag[0], 2);

  return 0;
}

static int audio_work_pipeline_deinit()
{
  audio_pipeline_terminate(audio_ctrl->audio_work_pipeline);
  audio_pipeline_unregister(audio_ctrl->audio_work_pipeline,
                            audio_ctrl->audio_decoder);
  audio_pipeline_unregister(audio_ctrl->audio_work_pipeline,
                            audio_ctrl->player_filter);

  audio_pipeline_remove_listener(audio_ctrl->audio_work_pipeline);

  audio_pipeline_deinit(audio_ctrl->audio_work_pipeline);
  audio_element_deinit(audio_ctrl->player_filter);
  audio_element_deinit(audio_ctrl->audio_decoder);
  rb_destroy(audio_ctrl->decoder_outbuf);

  buffer_pool_deinit(&(audio_ctrl->decoder_in_pool));
  buffer_pool_deinit(&(audio_ctrl->player_pool));

  return 0;
}

static int audio_work_pipeline_start()
{
  audio_pipeline_run(audio_ctrl->audio_work_pipeline);
  return 0;
}

static int audio_work_pipeline_stop()
{
  audio_pipeline_stop(audio_ctrl->audio_work_pipeline);
  audio_pipeline_wait_for_stop(audio_ctrl->audio_work_pipeline);
  return 0;
}

static void player_task(void *arg)
{
  ESP_LOGI(TAG, "player_task begin ...");
  uint8_t *data = malloc(PLA_TX_BUFSIZE);

  while (audio_ctrl->player_task_working)
  {
    /**
     * audio_ctrl->player_status
     * [7]: 0.未初始化 1.初始化
     * [6]: 1.使用解码器缓存
     * [5]: 1.使用播放缓存
     *
     * [3]: 0.未运行 1.运行
     * [2]: 1.draining
     * [1]: 1.解码器缓存非空
     * [0]: 1.播放缓存非空
     */
    if ((audio_ctrl->player_status & 0x08) != 0)
    {
      int read_bytes =
          read_data(&(audio_ctrl->player_pool), data, PLA_TX_BUFSIZE);
      if (read_bytes > 0)
      {
        ESP_LOGD(TAG, "%dbytes audio data ready to read from buffer.",
                 read_bytes);
        size_t play_bytes = player_hw_play(data, read_bytes);
        ESP_LOGD(TAG, "%dbytes audio data read from buffer played.",
                 play_bytes);
      }
      else if (read_bytes == 0)
      {
        if ((audio_ctrl->player_status & 0x04) == 0x04)
        {
          /* is draining */
          if ((audio_ctrl->player_status & 0x40) == 0x40)
          {
            /* using decoder cache */
            if (buffer_pool_is_empty(&(audio_ctrl->decoder_in_pool)) &&
                buffer_pool_is_empty(&(audio_ctrl->player_pool)))
            {
              vTaskDelay(pdMS_TO_TICKS(120));
              audio_ctrl->audio_ctrl_callback(AUDIO_CTRL_PLAYER,
                                              AUDIO_PLAYER_STOPPED);
            }
          }
          else
          {
            /* only use player cache */
            if (buffer_pool_is_empty(&(audio_ctrl->player_pool)))
            {
              vTaskDelay(pdMS_TO_TICKS(120));
              audio_ctrl->audio_ctrl_callback(AUDIO_CTRL_PLAYER,
                                              AUDIO_PLAYER_STOPPED);
            }
          }
        }
        else
        {
          vTaskDelay(pdMS_TO_TICKS(20));
        }
      }
      else
      {
        vTaskDelay(pdMS_TO_TICKS(20));
      }
    }
    else
    {
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  } // while

  vTaskDelay(pdMS_TO_TICKS(100));

  free(data);
  data = NULL;

  ESP_LOGI(TAG, "thread player_task exit ...");
  vTaskDelete(NULL);
}

int player_init(audio_ctrl_t *audio)
{
  audio_ctrl = audio;
  audio_ctrl->player_lock = xSemaphoreCreateRecursiveMutex();
  xSemaphoreGiveRecursive(audio_ctrl->player_lock);

  if (audio_ctrl->decoder_sr == 0)
  {
    audio_ctrl->decoder_sr = DEFAULT_AUDIO_CTRL_SR;
  }
  if (audio_ctrl->player_sr == 0)
  {
    audio_ctrl->player_sr = DEFAULT_AUDIO_CTRL_SR;
  }

  /* 创建解码器和播放器缓存, 创建解码任务流 */
  audio_work_pipeline_init(audio_ctrl->decoder_format);

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

int player_deinit()
{
  /* 停止解码任务流 */
  audio_work_pipeline_stop();
  /* 释放解码任务流, 释放解码器和播放器缓存 */
  audio_work_pipeline_deinit();
  /* 等待播放器任务结束 */
  audio_ctrl->player_task_working = false;

  audio_ctrl->player_status &= 0x7F;
  return 0;
}

int player_reset()
{
  buffer_pool_reset(&(audio_ctrl->player_pool));
  buffer_pool_reset(&(audio_ctrl->decoder_in_pool));
  return 0;
}

#ifdef ATK_ES8388
static void player_chip_start()
{
  /* ES8388初始化配置，有效降低启动时发出沙沙声 */
  // es8388_adda_cfg(1, 0);       /* 打开DAC，关闭ADC */
  // es8388_input_cfg(0);         /* 录音关闭 */
  // es8388_output_cfg(1, 1);     /* 喇叭通道和耳机通道打开 */
  // es8388_hpvol_set(33);        /* 设置喇叭 */
  // es8388_spkvol_set(22);       /* 设置耳机 */
  // xl9555_pin_write(SPK_EN_IO, 0); /* 打开喇叭 */
  vTaskDelay(pdMS_TO_TICKS(20));
}
#endif

#ifdef USER_ES8311
static void player_chip_start()
{

  vTaskDelay(pdMS_TO_TICKS(20));
}
#endif

void player_start(int sample_rate)
{
  ESP_LOGI(TAG, "player_start status: 0x%x/0x%x", audio_ctrl->codec_status, audio_ctrl->player_status);

  if ((audio_ctrl->codec_status & 0x01) == 0x00)
  {
    //设置播放的采样率通过协议设置的目前16k
    ESP_LOGI(TAG, "audio_ctrl->recorder_sr: %d", (int)audio_ctrl->player_sr);
    myi2s_init(audio_ctrl->player_sr); /* 初始化用于录音和播放的i2s */
    audio_ctrl->codec_status |= 0x01;
  }

  player_chip_start();

  buffer_pool_reset(&(audio_ctrl->player_pool));
  buffer_pool_reset(&(audio_ctrl->decoder_in_pool));

  /* set running flag */
  if ((audio_ctrl->recorder_status & 0x08) == 0x00)
  {
    i2s_rx_start();
    audio_ctrl->recorder_status |= 0x08;
  }
  /* set running flag */
  if ((audio_ctrl->player_status & 0x08) == 0x00)
  {
    i2s_tx_start();
    audio_ctrl->player_status |= 0x08;
  }

  audio_ctrl->audio_ctrl_callback(AUDIO_CTRL_PLAYER, AUDIO_PLAYER_STARTED);

  ESP_LOGI(TAG, "player_start done, status: 0x%x/0x%x", audio_ctrl->codec_status, audio_ctrl->player_status);
}

void player_stop()
{
  ESP_LOGI(TAG, "player_stop status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);

  /* clear draining flag */
  audio_ctrl->player_status &= 0xFB;

  /* clear running flag */
  if ((audio_ctrl->player_status & 0x08) == 0x08)
  {
    audio_ctrl->player_status &= 0xF7;
    i2s_tx_stop();
  }
  /* clear codec init flag */
  if ((audio_ctrl->codec_status & 0x01) == 0x01 &&
      (audio_ctrl->recorder_status & 0x08) == 0x00 &&
      (audio_ctrl->player_status & 0x08) == 0x00)
  {
    audio_ctrl->codec_status &= 0xFE;
    i2s_deinit();
  }
  buffer_pool_reset(&(audio_ctrl->decoder_in_pool));
  buffer_pool_reset(&(audio_ctrl->player_pool));
  ESP_LOGI(TAG, "player_stop done, status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
}

void player_drain()
{
  ESP_LOGI(TAG, "player_drain status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  audio_ctrl->player_status |= 0x04;
  if ((audio_ctrl->player_status & 0x40) == 0x40)
  {
    drain_data(&(audio_ctrl->decoder_in_pool));
  }
  else
  {
    drain_data(&(audio_ctrl->player_pool));
  }
}

// void player_drain()
// {
//   ESP_LOGI(TAG, "player_drain status: 0x%x/0x%x", audio_ctrl->codec_status,
//            audio_ctrl->player_status);
//   audio_ctrl->player_status |= 0x04;
//   if ((audio_ctrl->player_status & 0x40) == 0x40)
//   {
//     drain_data(&(audio_ctrl->decoder_in_pool));
//   }
//   else
//   {
//     drain_data(&(audio_ctrl->player_pool));
//   }
// }


size_t player_insert_data(uint8_t *data, size_t data_size)
{
  // ESP_LOGV(TAG, "player_insert_data status: 0x%x/0x%x, data_size:%d",
  //          audio_ctrl->codec_status, audio_ctrl->player_status, data_size);
  int written_data = 0;
  if ((audio_ctrl->player_status & 0x08) == 0x08)
  {
    if ((audio_ctrl->player_status & 0x40) == 0x40)
    {
      written_data = write_data(&(audio_ctrl->decoder_in_pool),
                                (const unsigned char *)data, data_size);
    }
    else
    {
      if ((audio_ctrl->player_status & 0x20) == 0x20)
      {
        written_data = write_data(&(audio_ctrl->player_pool),
                                  (const unsigned char *)data, data_size);
      }
    }
  }
  if (written_data < 0)
  {
    ESP_LOGE(TAG, "Write data into pool failed:%d", written_data);
    written_data = 0;
  }
  return written_data;
}

size_t player_hw_play(uint8_t *data, size_t data_size)
{
  ESP_LOGV(TAG, "player_hw_play status: 0x%x/0x%x", audio_ctrl->codec_status,
           audio_ctrl->player_status);
  if ((audio_ctrl->player_status & 0x08) == 0x08)
  {
    return i2s_tx_write(data, data_size);
  }
  else
  {
    return 0;
  }
}