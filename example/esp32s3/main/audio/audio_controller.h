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
#ifndef MAIN_AUDIO_CONTROLLER_H
#define MAIN_AUDIO_CONTROLLER_H

#include "audio_common.h"
#include "audio_element.h"
#include "audio_mem.h"
#include "audio_pipeline.h"
#include "buffer_pool.h"
#include "driver/i2s.h"
#include "ff.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "es8388.h"
#include "i2s.h"
#include "xl9555.h"

typedef enum {
  AUDIO_CTRL_RECORDER,
  AUDIO_CTRL_PLAYER,
} audio_ctrl_type_t;

typedef enum {
  AUDIO_RECORDER_STARTED,
  AUDIO_RECORDER_STOPPED,
  AUDIO_PLAYER_STARTED,
  AUDIO_PLAYER_STOPPED,
} audio_event_t;

typedef void (*audio_ctrl_callback_t)(audio_ctrl_type_t, audio_event_t);

typedef struct {
  uint8_t codec_status;    /**
                            * [0] 1.I2S初始化
                            */
  uint8_t recorder_status; /**
                            * [7]: 0.未初始化 1.初始化
                            *
                            * [3]: 0.未运行 1.运行
                            */
  uint32_t recorder_sr;
  SemaphoreHandle_t recorder_lock;

  uint8_t player_status; /**
                          * [7]: 0.未初始化 1.初始化
                          * [6]: 1.使用解码器缓存
                          * [5]: 1.使用播放缓存
                          *
                          * [3]: 0.未运行 1.运行
                          * [2]: 1.draining
                          * [1]: 1.解码器缓存非空
                          * [0]: 1.播放缓存非空
                          */
  uint32_t decoder_sr;
  SemaphoreHandle_t player_lock;
  bool decoder_task_working;
  uint32_t player_sr;
  bool player_task_working;
  char decoder_format[8];        /* 待解码格式, 如mp3 */
  char sdcard_decoding_dir[64];  /* 待解码数据缓存的目录 */
  char sdcard_decoding_base[64]; /* 待解码数据缓存的文件名base */
  char sdcard_player_dir[64];    /* 播放数据缓存的目录 */
  char sdcard_player_base[64];   /* 播放数据缓存的文件名base */
  buffer_pool_t decoder_in_pool;
  buffer_pool_t player_pool;

  audio_ctrl_callback_t audio_ctrl_callback;

  audio_pipeline_handle_t audio_work_pipeline;
  audio_element_handle_t audio_decoder;
  ringbuf_handle_t decoder_outbuf;
  audio_element_handle_t player_filter;

  bool enable_using_sdcard_cache;
} audio_ctrl_t;

/* RECORDER */
#define REC_RX_BUF_SIZE 3 * 1024 /* 定义RX 数组大小 需要小于4088 */

int recorder_init(audio_ctrl_t *audio);
int recorder_deinit();
int recorder_reset();
void recorder_start();
void recorder_stop();
size_t recorder_fetch_data(uint8_t *data, size_t data_size);
void recorder_new_pathname(const char *dname, uint8_t *pname);

/* PLAYER */
#define PLA_TX_BUFSIZE 3 * 1024

int player_init(audio_ctrl_t *audio);
int player_deinit();
int player_reset();
void player_start();
void player_stop();
void player_drain();
size_t player_insert_data(uint8_t *data, size_t data_size);
size_t player_hw_play(uint8_t *data, size_t data_size);

#endif  // MAIN_AUDIO_CONTROLLER_H
