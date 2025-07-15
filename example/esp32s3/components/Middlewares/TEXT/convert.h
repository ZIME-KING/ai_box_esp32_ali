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
#ifndef __CONVERT_H
#define __CONVERT_H

#include <stdint.h>

#include "ff.h"

typedef union _short {
  uint8_t bytes[2];
  uint16_t value;
} MyShort;

uint8_t utf8_to_unicode_one(uint8_t *utf8, uint32_t *unicode);
uint8_t unicode_to_utf16_one(uint32_t unicode, uint16_t *utf16);
MyShort search_gb2312_by_utf16(uint16_t unicode, FIL *f_table);

#endif