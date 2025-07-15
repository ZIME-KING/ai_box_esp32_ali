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
#ifndef MAIN_LCD_SHOW_H
#define MAIN_LCD_SHOW_H

#include "ff.h"

typedef struct lcd_show_chars_node {
  uint32_t index; /* 当前节点在链表中的索引, 从0开始 */
  struct lcd_show_chars_node* prev;
  struct lcd_show_chars_node* next;
  uint8_t utf8[4];
  uint8_t utf8_bytes;
  uint8_t gb2312[2];
  uint8_t gb2312_bytes;
  uint32_t x;
  uint32_t y;
  uint32_t font_width;
  uint32_t font_height;
  bool showed;
} lcd_show_chars_node_t;

typedef struct {
  char area_name[32];
  char font_table_path[64];
  FIL* f_table;
  uint32_t lcd_width;
  uint32_t lcd_height;
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
  uint16_t font_color;
  uint16_t font_size; /* 16或者24 */
  uint16_t background_color;
  uint32_t node_count;
  lcd_show_chars_node_t* ori;
} lcd_show_area_t;

int lcd_show_init(lcd_show_area_t* lcd_area, const char* area_name,
                  const char* font_table_path, uint32_t x, uint32_t y,
                  uint32_t width, uint32_t height, uint32_t font_color,
                  uint32_t font_size, uint16_t background_color);
int lcd_show_deinit(lcd_show_area_t* lcd_area);
void lcd_show_clear(lcd_show_area_t* lcd_area);
void lcd_show_color(lcd_show_area_t* lcd_area, uint32_t new_font_color);
int lcd_show_text_string_append(lcd_show_area_t* lcd_area, const char* text);

#endif