
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
#include "buffer_pool.h"

#include <string.h>

#include "esp_log.h"

static const char* TAG = "BufferPool";

static size_t write_cache_into_sd(buffer_pool_t* pool) {
  if (pool->cache2_size == 0) {
    return 0;
  }
  uint8_t* pname = malloc(255);
  if (pname == NULL) {
    ESP_LOGE(TAG, "write_cache_into_sd pname malloc failed!");
    return 0;
  }
  memset(pname, 0, 255);
  size_t now_file_write = pool->fwi + 1;
  sprintf((char*)pname, "%s/%s%d.%s", pool->sd_cache_dir,
          pool->sd_cache_filebase, now_file_write, pool->audio_format);

  pool->f_write = (FIL*)malloc(sizeof(FIL));
  FRESULT res =
      f_open(pool->f_write, (const TCHAR*)pname, FA_CREATE_ALWAYS | FA_WRITE);
  if (res != FR_OK) {
    ESP_LOGE(TAG, "write_cache_into_sd failed to open file: %s", pname);
    free(pool->f_write);
    pool->f_write = NULL;
    free(pname);
    return 0;
  } else {
    ESP_LOGD(TAG, "write_cache_into_sd open file: %s", pname);
  }

  UINT bytes_written;
  res = f_write(pool->f_write, pool->cache2, pool->cache2_size, &bytes_written);
  if (res != FR_OK) {
    ESP_LOGE(TAG, "write_cache_into_sd failed to write data to file: %s",
             pname);
  } else {
    ESP_LOGI(TAG, "write %dbytes data of %s into file: %s", pool->cache2_size,
             pool->pool_name, pname);
  }
  pool->cache2_size = 0;

  res = f_close(pool->f_write);
  if (res != FR_OK) {
    ESP_LOGE(TAG, "write_cache_into_sd failed of %s to close file: %s",
             pool->pool_name, pname);
  } else {
    ESP_LOGI(TAG, "write_cache_into_sd of %s close file: %s", pool->pool_name,
             pname);
  }
  pool->fwi++;
  free(pool->f_write);
  pool->f_write = NULL;
  free(pname);
  return (size_t)bytes_written;
}

static size_t read_data_into_sd(buffer_pool_t* pool, unsigned char* data,
                                size_t length) {
  if (pool->fwi == 0) {
    ESP_LOGW(TAG, "No file can read.");
    return 0;
  }

  if (pool->f_read == NULL && pool->fwi > pool->fri) {
    memset(pool->f_read_filename, 0, 192);
    size_t now_file_read = pool->fri + 1;
    sprintf(pool->f_read_filename, "%s/%s%d.%s", pool->sd_cache_dir,
            pool->sd_cache_filebase, now_file_read, pool->audio_format);

    pool->f_read = (FIL*)malloc(sizeof(FIL));
    FRESULT rval =
        f_open(pool->f_read, (const TCHAR*)pool->f_read_filename, FA_READ);
    if (rval != FR_OK) {
      ESP_LOGE(TAG, "read_data_into_sd of %s failed to open file: %s",
               pool->pool_name, pool->f_read_filename);
      free(pool->f_read);
      pool->f_read = NULL;
      return 0;
    } else {
      ESP_LOGI(TAG, "read_data_into_sd of %s open file: %s", pool->pool_name,
               pool->f_read_filename);
    }
  }

  size_t bytes_read = 0;
  if (pool->f_read != NULL) {
    f_read(pool->f_read, data, length, (UINT*)&bytes_read);
    if (bytes_read == 0) {
      FRESULT res = f_close(pool->f_read);
      if (res != FR_OK) {
        ESP_LOGE(TAG, "read_data_into_sd failed to close file: %s",
                 pool->f_read_filename);
      } else {
        ESP_LOGI(TAG, "read_data_into_sd of %s close file: %s", pool->pool_name,
                 pool->f_read_filename);
      }
      free(pool->f_read);
      pool->f_read = NULL;
      ESP_LOGD(TAG, "read_data_into_sd read empty, close %s.",
               pool->f_read_filename);
      pool->fri++;
    } else {
      // ESP_LOGI(TAG, "read_data_into_sd of %s read %dbytes", pool->pool_name,
      //          bytes_read);
    }
  }

  return bytes_read;
}

static void remove_all_files(const char* path) {
  FRESULT res;
  FILINFO fno;
  FF_DIR dir;

  char filePath[256];  // 用于保存文件路径

  // 打开目录
  res = f_opendir(&dir, path);
  if (res != FR_OK) {
    ESP_LOGE(TAG, "Failed to open directory: %s", path);
    return;
  }

  while (1) {
    // 读取目录中的一个文件
    res = f_readdir(&dir, &fno);
    if (res != FR_OK || fno.fname[0] == 0) break;  // 错误或读取至目录结尾

    // 忽略目录
    if (fno.fattrib & AM_DIR) continue;

    // 构造完整的文件路径
    sprintf(filePath, "%s/%s", path, fno.fname);

    // 删除文件
    res = f_unlink(filePath);
    if (res != FR_OK) {
      ESP_LOGE(TAG, "Failed to delete file: %s", filePath);
      break;
    } else {
      ESP_LOGI(TAG, "Deleted file: %s", filePath);
    }
  }

  // 关闭目录
  f_closedir(&dir);
  return;
}

int buffer_pool_init(buffer_pool_t* pool, const char* pool_name,
                     size_t cache1_limit, size_t cache1_threshold,
                     size_t cache2_limit, const char* sd_dir_base,
                     const char* sd_file_base, const char* format,
                     bool using_sdcard_cache) {
  if (pool == NULL || cache1_limit == 0 || cache2_limit == 0 ||
      sd_dir_base == 0 || sd_file_base == 0 || strlen(sd_dir_base) == 0 ||
      strlen(sd_file_base) == 0) {
    ESP_LOGE(TAG, "Invalid init params.");
    return -1;
  }

  pool->using_sdcard_cache = using_sdcard_cache;

  if (pool->using_sdcard_cache &&
      (sd_dir_base == 0 || sd_file_base == 0 || strlen(sd_dir_base) == 0 ||
       strlen(sd_file_base) == 0)) {
    ESP_LOGE(TAG, "Invalid init params.");
    return -1;
  }

  memset(pool->pool_name, 0, 32);
  strncpy(pool->pool_name, pool_name, 32);
  pool->runtime_status = 0;
  pool->cache1 = malloc(cache1_limit);
  pool->cache1_limit = cache1_limit;
  pool->cache1_pre_flag = true;
  pool->cache1_first_read_flag = true;
  pool->cache1_first_write_flag = true;
  pool->cache1_threshold = cache1_threshold;
  pool->head = 0;
  pool->tail = 0;
  pool->cache1_rw_lock = xSemaphoreCreateRecursiveMutex();
  xSemaphoreGiveRecursive(pool->cache1_rw_lock);

  pool->using_sdcard_cache = using_sdcard_cache;
  if (pool->using_sdcard_cache) {
    FF_DIR recdir;
    while (f_opendir(&recdir, sd_dir_base)) {
      f_mkdir(sd_dir_base); /* 创建该目录 */
    }

    pool->cache2 = malloc(cache2_limit);
    pool->cache2_limit = cache2_limit;
    pool->cache2_size = 0;
    pool->fri = 0;
    pool->fwi = 0;

    memset(pool->sd_cache_dir, 0, 64);
    strncpy(pool->sd_cache_dir, sd_dir_base, 63);
    memset(pool->sd_cache_filebase, 0, 64);
    strncpy(pool->sd_cache_filebase, sd_file_base, 63);
    memset(pool->audio_format, 0, 8);
    strncpy(pool->audio_format, format, 8);

    pool->f_write = NULL;
    pool->f_read = NULL;
    pool->cache2_r_lock = xSemaphoreCreateRecursiveMutex();
    xSemaphoreGiveRecursive(pool->cache2_r_lock);
    pool->cache2_w_lock = xSemaphoreCreateRecursiveMutex();
    xSemaphoreGiveRecursive(pool->cache2_w_lock);

    // remove all cache files
    remove_all_files(pool->sd_cache_dir);
  } else {
    pool->cache2 = NULL;
    pool->cache2_limit = 0;
    pool->cache2_size = 0;
    pool->fri = 0;
    pool->fwi = 0;

    memset(pool->sd_cache_dir, 0, 64);
    memset(pool->sd_cache_filebase, 0, 64);
    memset(pool->audio_format, 0, 8);

    pool->f_write = NULL;
    pool->f_read = NULL;
    memset(pool->f_read_filename, 0, 192);
    pool->cache2_r_lock = NULL;
    pool->cache2_w_lock = NULL;
  }
  return 0;
}

int buffer_pool_deinit(buffer_pool_t* pool) {
  pool->runtime_status = 0;
  if (pool->cache1) {
    free(pool->cache1);
    pool->cache1 = NULL;
  }
  pool->cache1_limit = 0;
  pool->head = 0;
  pool->tail = 0;

  if (pool->using_sdcard_cache) {
    if (pool->cache2) {
      free(pool->cache2);
      pool->cache2 = NULL;
    }
    pool->cache2_limit = 0;
    pool->cache2_size = 0;
    pool->fri = 0;
    pool->fwi = 0;

    // remove all cache files
    remove_all_files(pool->sd_cache_dir);

    memset(pool->sd_cache_dir, 0, 64);
    memset(pool->sd_cache_filebase, 0, 64);

    if (pool->f_write != NULL) {
      f_close(pool->f_write);
      pool->f_write = NULL;
    }
    if (pool->f_read != NULL) {
      f_close(pool->f_read);
      pool->f_read = NULL;
    }
  }

  return 0;
}

int buffer_pool_reset(buffer_pool_t* pool) {
  pool->runtime_status = 0;
  pool->head = 0;
  pool->tail = 0;
  pool->cache1_pre_flag = true;
  pool->cache1_first_read_flag = true;
  pool->cache1_first_write_flag = true;

  if (pool->using_sdcard_cache) {
    pool->cache2_size = 0;
    pool->fri = 0;
    pool->fwi = 0;

    if (pool->f_write != NULL) {
      f_close(pool->f_write);
      pool->f_write = NULL;
    }
    if (pool->f_read != NULL) {
      f_close(pool->f_read);
      pool->f_read = NULL;
    }

    // remove all cache files
    remove_all_files(pool->sd_cache_dir);
  }
  return 0;
}

int read_data(buffer_pool_t* pool, unsigned char* data, size_t length) {
  if (pool == NULL || data == NULL || length == 0) {
    ESP_LOGE(TAG, "Invalid reading params.");
    return -1;
  }

  int read_bytes = 0;                       /* data读偏移 */
  size_t need_bytes = length;               /* data剩余需要读的数据 */
  if ((pool->runtime_status & 0x02) == 0) { /* 0.cache1读取时,1.cache2读取时 */
    if (pool->cache1_pre_flag && pool->head == 0 && pool->tail == 0) {
      // ESP_LOGV(TAG, "read_data cache1 head:%d tail:%d, waiting more ...",
      //          pool->head, pool->tail);
      return -2;
    }

    if (pool->cache1_pre_flag && pool->head == 0 &&
        (pool->tail - pool->head < pool->cache1_threshold)) {
      // ESP_LOGV(TAG, "read_data cache1 head:%d tail:%d, waiting more (%d)
      // ...",
      //          pool->head, pool->tail, pool->cache1_threshold);
      return -3;
    }

    xSemaphoreTakeRecursive(pool->cache1_rw_lock, portMAX_DELAY);
    pool->cache1_pre_flag = false;
    size_t reading_bytes = (pool->tail - pool->head) < need_bytes
                               ? (pool->tail - pool->head)
                               : need_bytes;
    if (reading_bytes > 0) {
      memcpy(data + read_bytes, pool->cache1 + pool->head, reading_bytes);
      pool->head += reading_bytes;
      read_bytes += reading_bytes;
      need_bytes -= reading_bytes;
    }

    if (pool->head == pool->tail) {
      pool->head = 0;
      pool->tail = 0;
    }

    if ((pool->head == pool->cache1_limit) ||
        (pool->head == 0 && pool->fwi > pool->fri)) {
      if (pool->using_sdcard_cache) {
        /* cache1读完 */
        pool->runtime_status |= 0x02;
        ESP_LOGI(TAG, "%s cache1 is empty, start use cache2 ...",
                 pool->pool_name);
      } else {
        ESP_LOGW(TAG, "%s cache1 is empty, read_data failed.", pool->pool_name);
      }
    }
    xSemaphoreGiveRecursive(pool->cache1_rw_lock);
  }

  if (pool->using_sdcard_cache) {
    while (need_bytes > 0 && (pool->runtime_status & 0x02) == 2) {
      // xSemaphoreTakeRecursive(pool->cache2_r_lock, portMAX_DELAY);
      size_t reading_bytes = read_data_into_sd(
          pool, data + read_bytes,
          (length - read_bytes) < need_bytes ? (length - read_bytes)
                                             : need_bytes);
      read_bytes += reading_bytes;
      need_bytes -= reading_bytes;
      // xSemaphoreGiveRecursive(pool->cache2_r_lock);
      if (reading_bytes == 0) {
        break;
      }
    }  // while
  }

  if (pool->cache1_first_read_flag && read_bytes > 0) {
    ESP_LOGI(TAG, "First read %dbytes from pool %s!", read_bytes,
             pool->pool_name);
    pool->cache1_first_read_flag = false;
  }

  return read_bytes;
}

int write_data(buffer_pool_t* pool, const unsigned char* data, size_t length) {
  if (pool == NULL || data == NULL || length == 0) {
    ESP_LOGE(TAG, "Invalid writing params.");
    return -1;
  }

  int written_bytes = 0;        /* data写入的偏移 */
  size_t remain_bytes = length; /* data剩余需要写入的数据 */
  if ((pool->runtime_status & 0x01) == 0) { /* 0.cache1写入时,1.cache2写入时 */
    xSemaphoreTakeRecursive(pool->cache1_rw_lock, portMAX_DELAY);
    if (!pool->using_sdcard_cache &&
        pool->tail + remain_bytes > pool->cache1_limit) {
      size_t remain_bytes_to_read = pool->tail - pool->head;
      bool move_data_flag =
          (remain_bytes_to_read + remain_bytes) <= pool->cache1_limit ? true
                                                                      : false;
      /* 挪动未读数据, 腾出空间 */
      if (move_data_flag) {
        unsigned char* tmp_cache1 = malloc(remain_bytes_to_read);
        if (tmp_cache1) {
          memcpy(tmp_cache1, pool->cache1 + pool->head, remain_bytes_to_read);
          pool->head = 0;
          pool->tail = remain_bytes_to_read;
          memcpy(pool->cache1 + pool->head, tmp_cache1, remain_bytes_to_read);
          free(tmp_cache1);
          // ESP_LOGV(TAG,
          //          "%s tidy cache1, new head is %d, tail is %d, "
          //          "ready to write %dbytes.",
          //          pool->pool_name, pool->head, pool->tail, remain_bytes);
        }
      }

      /* 空间不够重新分配 */
      if (pool->tail + remain_bytes > pool->cache1_limit) {
        size_t new_cache1_limit = pool->tail + remain_bytes + 1;
        unsigned char* new_cache1 = realloc(pool->cache1, new_cache1_limit);
        if (new_cache1 != NULL) {
          pool->cache1 = new_cache1;
          pool->cache1_limit = new_cache1_limit;
          // ESP_LOGV(TAG,
          //          "%s realloc cache1, new size is %d, head is %d, tail is
          //          %d, " "ready to write %dbytes.", pool->pool_name,
          //          pool->cache1_limit, pool->head, pool->tail, remain_bytes);
        } else {
          ESP_LOGE(TAG, "%s realloc failed.", pool->pool_name);
        }
      }
    }
    size_t writing_bytes = (pool->cache1_limit - pool->tail) < remain_bytes
                               ? (pool->cache1_limit - pool->tail)
                               : remain_bytes;
    memcpy(pool->cache1 + pool->tail, data + written_bytes, writing_bytes);
    pool->tail += writing_bytes;
    written_bytes += writing_bytes;
    remain_bytes -= writing_bytes;
    if (pool->tail == pool->cache1_limit) {
      /* cache1写满 */
      if (pool->using_sdcard_cache) {
        pool->runtime_status |= 0x01;
        ESP_LOGI(TAG, "%s cache1 is full, start use cache2 ...",
                 pool->pool_name);
      } else {
        ESP_LOGW(TAG, "%s cache1 is full, write_data failed.", pool->pool_name);
      }
    }
    xSemaphoreGiveRecursive(pool->cache1_rw_lock);
  }

  if (pool->using_sdcard_cache) {
    xSemaphoreTakeRecursive(pool->cache2_w_lock, portMAX_DELAY);
    while (remain_bytes > 0 && (pool->runtime_status & 0x01) == 1) {
      size_t writing_bytes =
          (pool->cache2_limit - pool->cache2_size) < remain_bytes
              ? (pool->cache2_limit - pool->cache2_size)
              : remain_bytes;
      memcpy(pool->cache2 + pool->cache2_size, data + written_bytes,
             writing_bytes);
      pool->cache2_size += writing_bytes;
      written_bytes += writing_bytes;
      remain_bytes -= writing_bytes;

      if (pool->cache2_size == pool->cache2_limit) {
        /* cache2写满, 写入SD卡 */
        write_cache_into_sd(pool);
      }
    }  // while
    xSemaphoreGiveRecursive(pool->cache2_w_lock);
  }

  if (pool->cache1_first_write_flag && written_bytes > 0) {
    ESP_LOGD(TAG, "First write %dbytes into pool!", written_bytes);
    pool->cache1_first_write_flag = false;
  }

  if (written_bytes == 0) {
    ESP_LOGI(TAG,
             "write_data %s zero, runtime_status:0x%x fwi:%d fri:%d, tail:%d "
             "head:%d",
             pool->pool_name, pool->runtime_status, pool->fwi, pool->fri,
             pool->tail, pool->head);
  }

  return written_bytes;
}

void drain_data(buffer_pool_t* pool) {
  pool->runtime_status |= 0x10;
  if (pool->using_sdcard_cache) {
    xSemaphoreTakeRecursive(pool->cache2_w_lock, portMAX_DELAY);
    pool->runtime_status |= 0x01;
    write_cache_into_sd(pool);
    xSemaphoreGiveRecursive(pool->cache2_w_lock);
  }
  return;
}

bool buffer_pool_is_empty(buffer_pool_t* pool) {
  if ((pool->runtime_status & 0x02) == 2) {
    /* 如果cache2正在读取 */
    if (pool->fwi > pool->fri) {
      return false;
    } else if (pool->fwi == pool->fri && pool->f_read != NULL) {
      return false;
    }
  }
  if ((pool->runtime_status & 0x02) == 0 && pool->tail > pool->head) {
    /* 如果cache1正在读取, 且有数据 */
    return false;
  }
  if (pool->cache2_size > 0) {
    drain_data(pool);
    return false;
  }

  ESP_LOGI(
      TAG,
      "buffer_pool %s is empty, runtime_status:0x%x fwi:%d fri:%d, tail:%d "
      "head:%d, cache2_size:%d",
      pool->pool_name, pool->runtime_status, pool->fwi, pool->fri, pool->tail,
      pool->head, pool->cache2_size);
  return true;
}