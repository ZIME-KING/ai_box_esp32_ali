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
#include "convert.h"

#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "ff.h"

static const char *TAG = "Convert";

/**
 * @brief
 * utf8编码转unicode字符集(usc4)，最大支持4字节utf8编码，(4字节以上在中日韩文中为生僻字)
 * @param *utf8 utf8变长编码字节集1~4个字节
 * @param *unicode
 * utf8编码转unicode字符集结果，最大4个字节，返回的字节序与utf8编码序一致
 * @return length 0：utf8解码异常，others：本次utf8编码长度
 */
uint8_t utf8_to_unicode_one(uint8_t *utf8, uint32_t *unicode) {
  const uint8_t lut_size = 3;
  const uint8_t length_lut[] = {2, 3, 4};
  const uint8_t range_lut[] = {0xE0, 0xF0, 0xF8};
  const uint8_t mask_lut[] = {0x1F, 0x0F, 0x07};

  uint8_t length = 0;
  uint8_t b = *(utf8 + 0);
  uint32_t i = 0;

  if (utf8 == NULL) {
    *unicode = 0;
    return 0;
  }
  // utf8编码兼容ASCII编码,使用0xxxxxx 表示00~7F
  if (b < 0x80) {
    *unicode = b;
    return 1;
  }
  // utf8不兼容ISO8859-1 ASCII拓展字符集
  // 同时最大支持编码6个字节即1111110X
  if (b < 0xC0 || b > 0xFD) {
    *unicode = 0;
    return 0;
  }
  for (i = 0; i < lut_size; i++) {
    if (b < range_lut[i]) {
      *unicode = b & mask_lut[i];
      length = length_lut[i];
      break;
    }
  }
  // 超过四字节的utf8编码不进行解析
  if (length == 0) {
    *unicode = 0;
    return 0;
  }
  // 取后续字节数据
  for (i = 1; i < length; i++) {
    b = *(utf8 + i);
    // 多字节utf8编码后续字节范围10xxxxxx?~?10111111?
    if (b < 0x80 || b > 0xBF) {
      break;
    }
    *unicode <<= 6;
    // ?00111111?
    *unicode |= (b & 0x3F);
  }
  // 长度校验
  return (i < length) ? 0 : length;
}

uint8_t unicode_to_utf16_one(uint32_t unicode, uint16_t *utf16) {
  // Unicode范围 U+000~U+FFFF
  // utf16编码方式：2 Byte存储，编码后等于Unicode值
  if (unicode <= 0xFFFF) {
    if (utf16 != NULL) {
      *utf16 = (unicode & 0xFFFF);
    }
    return 1;
  } else if (unicode <= 0xEFFFF) {
    if (utf16 != NULL) {
      // 高10位
      *(utf16 + 0) = 0xD800 + (unicode >> 10) - 0x40;
      // 低10位
      *(utf16 + 1) = 0xDC00 + (unicode & 0x03FF);
    }
    return 2;
  }

  return 0;
}

MyShort search_gb2312_by_utf16(uint16_t unicode, FIL *f_table) {
  FILINFO fileInfo;
  uint8_t res;
  MyShort gb2312;
  uint16_t readin = 0;
  uint32_t pack = 0;
  gb2312.value = 0;

  if (f_table != NULL) {
    // 四字节一组，低两字节为unicode，高两字节为gb2312
    f_lseek(f_table, 0);
    do {
      size_t bytes_read = 0; /* 读取录音大小 */
      res = f_read(f_table, (uint8_t *)&pack, sizeof(uint32_t),
                   (UINT *)&bytes_read);
      if (res != FR_OK) {
        break;
      }
      readin = (uint16_t)(pack & 0xFFFF);
    } while (unicode != readin);

    gb2312.bytes[0] = (uint8_t)((pack >> 16) & 0xFF);
    gb2312.bytes[1] = (uint8_t)((pack >> 24) & 0xFF);
    ESP_LOGV(TAG, "get GB: %x %x", gb2312.bytes[0], gb2312.bytes[1]);
  } else {
    ESP_LOGE(TAG, "open gb2312 table failed.");
  }
  return gb2312;
}
