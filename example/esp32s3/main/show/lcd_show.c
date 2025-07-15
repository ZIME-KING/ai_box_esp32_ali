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

#include "lcd_show.h"

#include <string.h>

#include "convert.h"
#include "esp_log.h"
#include "lcd.h"
#include "text.h"

// #define ENABLE_LCD_SHOW_DEBUG
// #define ENABLE_LCD_SHOW_DEBUG2

static const char* TAG = "LcdShow";

static bool is_chinese_utf8(unsigned char* str) {
  if (str[0] >= 0xE4 && str[0] <= 0xE9) {
    return true;
  }
  return false;
}

static int lcd_show_erase(lcd_show_area_t* lcd_area, uint32_t index) {
  lcd_show_chars_node_t* cur_node = lcd_area->ori;
  while (cur_node != NULL) {
    lcd_show_chars_node_t* next_node = cur_node->next;
    uint32_t cur_index = cur_node->index;
    if (cur_index == index) {
#ifdef ENABLE_LCD_SHOW_DEBUG
      ESP_LOGI(TAG, "erase node index:%ld", cur_index);
#endif
      /* 从这个节点开始删除 */
      if (cur_node->prev) {
        cur_node->prev->next = NULL;
      }
      free(cur_node);
      lcd_area->node_count--;
    } else if (cur_index > index) {
#ifdef ENABLE_LCD_SHOW_DEBUG
      ESP_LOGI(TAG, "erase node index:%ld", cur_index);
#endif
      free(cur_node);
      lcd_area->node_count--;
    }
    cur_node = next_node;
  }
  if (lcd_area->node_count == 0) {
    lcd_area->ori = NULL;
  }
  return 0;
}

static uint32_t lcd_show_find_diff(lcd_show_area_t* lcd_area,
                                   const char* utf8_text,
                                   uint32_t* utf8_text_offset) {
  uint32_t index = 0;
  uint32_t utf8_text_bytes = strlen(utf8_text);

  if (lcd_area->node_count > 0 && utf8_text_bytes > 0) {
    lcd_show_chars_node_t* cur_node = lcd_area->ori;
    while (cur_node != NULL) {
      bool break_flag = false;
      for (int i = 0; i < cur_node->utf8_bytes; i++) {
        if (cur_node->utf8[i] != utf8_text[*utf8_text_offset]) {
          break_flag = true;
          break;
        } else {
          (*utf8_text_offset)++;
          if ((*utf8_text_offset) > utf8_text_bytes) {
            break_flag = true;
            break;
          }
        }
      }  // for
      if (break_flag) {
#ifdef ENABLE_LCD_SHOW_DEBUG
        ESP_LOGI(TAG, "list index %ld is diff from node index %ld.", index,
                 cur_node->index);
#endif
        break;
      } else {
#ifdef ENABLE_LCD_SHOW_DEBUG
        ESP_LOGI(TAG, "list index %ld is the same with node index %ld.", index,
                 cur_node->index);
#endif
      }

      /* 设置下个节点 */
      index++;
      cur_node = cur_node->next;
    }  // while
  }

  return index;
}

static int lcd_show_text_str_append(lcd_show_area_t* lcd_area,
                                    const char* utf8_text) {
  lcd_show_chars_node_t* cur_node = lcd_area->ori;
  lcd_show_chars_node_t* prev_node = NULL;
  uint32_t utf8_text_bytes = strlen(utf8_text);

  /* 到链表尾部空节点 */
  while (cur_node != NULL) {
    prev_node = cur_node;
    cur_node = cur_node->next;
  }

  /* 逐字符转成gb2312并存到新节点中 */
  uint32_t utf8_text_offset = 0;
  while (utf8_text_offset < utf8_text_bytes) {
    uint32_t unicode = 0;
    uint8_t utf8_one_len =
        utf8_to_unicode_one((uint8_t*)(utf8_text + utf8_text_offset), &unicode);
    if (utf8_one_len == 0) {
      return 0;
    } else if (utf8_one_len > 1) {
      if (!is_chinese_utf8(utf8_text + utf8_text_offset)) {
        utf8_text_offset += utf8_one_len;
        continue;
      }
    }

    if (cur_node != NULL) {
      prev_node = cur_node;
    }

#ifdef ENABLE_LCD_SHOW_DEBUG
    ESP_LOGI(TAG, " = Last round cur_node:%p prev_node:%p", cur_node,
             prev_node);
#endif

    cur_node = (lcd_show_chars_node_t*)malloc(sizeof(lcd_show_chars_node_t));
    if (cur_node == NULL) {
      ESP_LOGE(TAG, "malloc lcd_show_chars_node_t failed.");
      break;
    }
    memset(cur_node, 0, sizeof(lcd_show_chars_node_t));
    cur_node->index = prev_node == NULL ? 0 : prev_node->index + 1;
    cur_node->prev = prev_node;
    cur_node->next = NULL;
    cur_node->x =
        prev_node == NULL ? lcd_area->x : prev_node->x + prev_node->font_width;
    cur_node->y = prev_node == NULL ? lcd_area->y : prev_node->y;
    cur_node->font_width =
        utf8_one_len == 1 ? lcd_area->font_size / 2 : lcd_area->font_size;
    cur_node->font_height = lcd_area->font_size;
    if (cur_node->x + cur_node->font_width > lcd_area->x + lcd_area->width) {
      /* 换行 */
      cur_node->x = lcd_area->x;
      cur_node->y += lcd_area->font_size;
      /* 过界则放弃 */
      if (cur_node->y >= (lcd_area->y + lcd_area->height)) {
        ESP_LOGW(TAG, "current node y:%ld, LCD y:%ld height:%ld", cur_node->y,
                 lcd_area->y, lcd_area->height);
        free(cur_node);
        return 0;
      } else {
#ifdef ENABLE_LCD_SHOW_DEBUG
        ESP_LOGI(TAG, "current node x:%ld, y:%ld, width:%ld, height:%ld",
                 cur_node->x, cur_node->y, lcd_area->width, lcd_area->height);
#endif
      }
    } else {
#ifdef ENABLE_LCD_SHOW_DEBUG
      ESP_LOGI(TAG, "current node x:%ld, y:%ld, width:%ld, height:%ld",
               cur_node->x, cur_node->y, lcd_area->width, lcd_area->height);
#endif
    }

    cur_node->utf8_bytes = utf8_one_len;
    if (utf8_one_len == 1) {
      cur_node->gb2312[0] = (uint8_t)unicode;
      cur_node->gb2312_bytes = 1;
      cur_node->utf8[0] = *(utf8_text + utf8_text_offset);
    } else {
      // utf8_one_len > 1
      for (int i = 0; i < utf8_one_len; i++) {
        cur_node->utf8[i] = *(utf8_text + utf8_text_offset + i);
      }
      uint16_t utf16[2] = {0};
      uint8_t utf16_one_len = unicode_to_utf16_one(unicode, utf16);
#ifdef ENABLE_LCD_SHOW_DEBUG
      ESP_LOGI(TAG, "unicode_to_utf16_one -> UTF16:%dbytes 0x%x 0x%x",
               utf16_one_len * 2, utf16[0], utf16[1]);
#endif
      MyShort gb2312_str = search_gb2312_by_utf16(utf16[0], lcd_area->f_table);
#ifdef ENABLE_LCD_SHOW_DEBUG
      ESP_LOGI(TAG, "search_gb2312_by_utf16 -> GB: 0x%x", gb2312_str.value);
#endif
      cur_node->gb2312[0] = gb2312_str.bytes[0];
      cur_node->gb2312[1] = gb2312_str.bytes[1];
      cur_node->gb2312_bytes = 2;
    }

    if (cur_node->prev != NULL) {
      cur_node->prev->next = cur_node;
    }
    if (lcd_area->ori == NULL) {
      lcd_area->ori = cur_node;
    }
    lcd_area->node_count++;

#ifdef ENABLE_LCD_SHOW_DEBUG
    /* show result */
    ESP_LOGI(TAG, " - use %dbytes utf8", utf8_one_len);
    ESP_LOGI(TAG, " - convert to %dbytes GB: 0x%x 0x%x", cur_node->gb2312_bytes,
             cur_node->gb2312[0], cur_node->gb2312[1]);
    ESP_LOGI(TAG, " - current node index is %ld", cur_node->index);
    ESP_LOGI(TAG, " - total nodes count is %ld", lcd_area->node_count);
#endif

    utf8_text_offset += utf8_one_len;
  }  // while

  return 0;
}

static int lcd_show_char_one_by_one(lcd_show_area_t* lcd_area) {
  lcd_show_chars_node_t* cur_node = lcd_area->ori;
  lcd_show_chars_node_t* next_node = NULL;
  while (cur_node != NULL) {
    next_node = cur_node->next;
    if (!cur_node->showed) {
#ifdef ENABLE_LCD_SHOW_DEBUG
      ESP_LOGI(TAG, "node index %ld is showing.", cur_node->index);
#endif
      text_show_char(cur_node->x, cur_node->y, lcd_area->width,
                     lcd_area->height, (char*)cur_node->gb2312,
                     cur_node->gb2312_bytes, lcd_area->font_size, 0,
                     lcd_area->font_color);
      cur_node->showed = true;
    } else {
#ifdef ENABLE_LCD_SHOW_DEBUG
      ESP_LOGI(TAG, "node index %ld is showed.", cur_node->index);
#endif
    }
    cur_node = next_node;
  }  // while
  return 0;
}

int lcd_show_init(lcd_show_area_t* lcd_area, const char* area_name,
                  const char* font_table_path, uint32_t x, uint32_t y,
                  uint32_t width, uint32_t height, uint32_t font_color,
                  uint32_t font_size, uint16_t background_color) {
  if (lcd_area == NULL || area_name == NULL || font_table_path == NULL ||
      width == 0 || height == 0) {
    ESP_LOGE(TAG, "Invalid init params.");
    return -1;
  }

  memset(lcd_area->area_name, 0, 32);
  strncpy(lcd_area->area_name, area_name, 31);

  memset(lcd_area->font_table_path, 0, 32);
  strncpy(lcd_area->font_table_path, font_table_path, 31);

  lcd_area->lcd_width = 320;
  lcd_area->lcd_height = 240;
  lcd_area->x = x;
  lcd_area->y = y;
  lcd_area->width = width;
  lcd_area->height = height;
  lcd_area->font_color = font_color;
  lcd_area->font_size = font_size;
  lcd_area->background_color = background_color;

  lcd_area->node_count = 0;
  lcd_area->ori = NULL;

  lcd_area->f_table = (FIL*)malloc(sizeof(FIL));
  FRESULT result = f_open(lcd_area->f_table,
                          (const TCHAR*)lcd_area->font_table_path, FA_READ);
  if (result != FR_OK) {
    ESP_LOGE(TAG, "%s Open %s failed(%d).", lcd_area->area_name,
             lcd_area->font_table_path, result);
    free(lcd_area->f_table);
    lcd_area->f_table = NULL;
    return -2;
  }

  return 0;
}

int lcd_show_deinit(lcd_show_area_t* lcd_area) {
  if (lcd_area->node_count > 0) {
    lcd_show_chars_node_t* cur = lcd_area->ori;
    while (cur != NULL) {
      lcd_show_chars_node_t* next = cur->next;
      free(cur);
      lcd_area->node_count--;
      cur = next;
    }
  }

  if (lcd_area->f_table) {
    f_close(lcd_area->f_table);
    free(lcd_area->f_table);
    lcd_area->f_table = NULL;
  }
  return 0;
}

void lcd_show_clear(lcd_show_area_t* lcd_area) {
  spilcd_fill(lcd_area->x, lcd_area->y, lcd_area->x + lcd_area->width,
              lcd_area->y + lcd_area->height, WHITE);
}

void lcd_show_color(lcd_show_area_t* lcd_area, uint32_t new_font_color) {
  lcd_area->font_color = new_font_color;
}

int lcd_show_text_string_append(lcd_show_area_t* lcd_area,
                                const char* utf8_text) {
  uint32_t utf8_text_offset = 0;
#ifdef ENABLE_LCD_SHOW_DEBUG2
  uint32_t find_dif_t0 = esp_log_timestamp();
#endif
  uint32_t diff_index =
      lcd_show_find_diff(lcd_area, utf8_text, &utf8_text_offset);
#ifdef ENABLE_LCD_SHOW_DEBUG2
  uint32_t find_dif_t1 = esp_log_timestamp();
#endif
  if (diff_index < lcd_area->node_count) {
    lcd_show_erase(lcd_area, diff_index);
  }
#ifdef ENABLE_LCD_SHOW_DEBUG2
  uint32_t erase_t1 = esp_log_timestamp();
#endif
  if (utf8_text_offset < strlen(utf8_text)) {
    lcd_show_text_str_append(lcd_area, utf8_text + utf8_text_offset);
  }
#ifdef ENABLE_LCD_SHOW_DEBUG2
  uint32_t str_append_t1 = esp_log_timestamp();
#endif
  lcd_show_char_one_by_one(lcd_area);
#ifdef ENABLE_LCD_SHOW_DEBUG2
  uint32_t show_t1 = esp_log_timestamp();

  ESP_LOGI(TAG,
           "find_diff cost:%ldms, erase cost:%ldms, str_append cost:%ldms, "
           "show char cost:%ldms.",
           find_dif_t1 - find_dif_t0, erase_t1 - find_dif_t1,
           str_append_t1 - erase_t1, show_t1 - str_append_t1);
#endif

  return 0;
}
