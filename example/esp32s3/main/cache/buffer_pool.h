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
#ifndef MAIN_BUFFER_POOL_H
#define MAIN_BUFFER_POOL_H

#include "ff.h"

typedef struct {
  char pool_name[32];
  uint8_t runtime_status; /*
                           * [4]: 0.可写入,1.写尽
                           * [1]: 0.cache1读取时,1.cache2读取时
                           * [0]: 0.cache1写入时,1.cache2写入时
                           */

  unsigned char* cache1; /* 优先写入数据到一级缓存 */
  size_t cache1_limit;
  bool cache1_pre_flag;
  bool cache1_first_read_flag;
  bool cache1_first_write_flag;
  size_t head;
  size_t tail;
  SemaphoreHandle_t cache1_rw_lock;

  bool using_sdcard_cache;

  unsigned char*
      cache2; /* 一级缓存写满后写二级缓存,二级缓存写满即马上写入SD卡 */
  size_t cache2_limit;
  size_t cache1_threshold;
  size_t cache2_size; /* 二级缓存已经写入的数据长度 */
  size_t fri;         /* 已读文件的索引,从1开始 */
  size_t fwi;         /* 已写文件的索引,从1开始 */
  char sd_cache_dir[64];
  char sd_cache_filebase[64];
  char audio_format[8];
  FIL* f_write;
  FIL* f_read;
  char f_read_filename[192];
  SemaphoreHandle_t cache2_r_lock;
  SemaphoreHandle_t cache2_w_lock;
} buffer_pool_t;

int buffer_pool_init(buffer_pool_t* pool, const char* pool_name,
                     size_t cache1_limit, size_t cache1_threshold,
                     size_t cache2_limit, const char* sd_dir_base,
                     const char* sd_file_base, const char* format,
                     bool using_sdcard_cache);
int buffer_pool_deinit(buffer_pool_t* pool);
int buffer_pool_reset(buffer_pool_t* pool);
int read_data(buffer_pool_t* pool, unsigned char* data, size_t length);
int write_data(buffer_pool_t* pool, const unsigned char* data, size_t length);
void drain_data(buffer_pool_t* pool);
bool buffer_pool_is_empty(buffer_pool_t* pool);

#endif  // MAIN_BUFFER_POOL_H
