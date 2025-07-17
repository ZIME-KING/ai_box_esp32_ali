/* VoiceChat Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>

#include "audio_controller.h"
#include "cJSON.h"
#include "conv_utest.h"
#include "conversation.h"
#include "device_config.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "exfuns.h"
#include "ff.h"
#include "fonts.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "key.h"
#include "lcd_show.h"
#include "es8388.h"
#include "myi2c.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "sd.h"
#include "lcd.h"
#include "text.h"
#include "wifi_sta.h"

#include "es8311.h"

// #define KEY_NONE    0
// #define KEY0_PRES   BOOT_PRES
// #define KEY0_RELES  2

#define KEY_NONE 0
#define KEY0_PRES 1   /* KEY0按下 */
#define KEY1_PRES 2   /* KEY1按下 */
#define KEY2_PRES 3   /* KEY1按下 */
#define KEY3_PRES 4   /* KEY1按下 */
#define KEY0_RELES 10 /* KEY0释放 */

static const char *TAG = "VoiceChatDemo";
static const char *SDCARD_RECORDER_DIR = "0:/RECORDER";
static const char *SDCARD_PLAYER_DIR = "0:/PLAYER";
static const char *SDCARD_PLAYER_BASE = "TTS";
static const char *SDCARD_DECODER_DIR = "0:/DECODER";
static const char *SDCARD_DECODER_BASE = "DECODE";
static const char *TTS_AUDIO_FORMAT = "mp3";
static const bool ENABLE_USING_SDCARD_CACHE = false;

static const char *GB_TABLE = "0:/SYSTEM/FONT/UTFTOGB.BIN";

static conv_event_type_t g_event_type = CONV_EVENT_NONE;
static dialog_state_changed_t g_dialog_state = DIALOG_STATE_NONE;
static int default_recorder_sample_rate = CONFIG_DEFAULT_RECORDER_SAMPLE_RATE;
static int default_player_sample_rate = CONFIG_DEFAULT_PLAYER_SAMPLE_RATE;
static bool g_running = true;
static bool g_key_scanning = false;
static uint8_t g_recorder_sta = 0; /**
                                    * 录音状态
                                    * [7]:0,没有开启录音;1,已经开启录音;
                                    * [6]:读文件模拟录音;
                                    * [5]:模拟录音和模拟播放;
                                    * [4:1]:保留
                                    * [0]:
                                    */

static QueueSetHandle_t xQueueSet;               /* 定义队列集 */
static SemaphoreHandle_t key0_pres_xSemaphore;   /* KEY0按下信号量 */
static SemaphoreHandle_t key0_releas_xSemaphore; /* KEY0抬起信号量 */
static SemaphoreHandle_t key1_pres_xSemaphore;   /* KEY1按下信号量 */
static SemaphoreHandle_t key2_pres_xSemaphore;   /* KEY2按下信号量 */
static SemaphoreHandle_t key3_pres_xSemaphore;   /* KEY3按下信号量 */

static audio_ctrl_t audio_ctrl;

/* show LCD */
static lcd_show_area_t title_show;
static lcd_show_area_t sub_title_show;
static lcd_show_area_t hw_show;
static lcd_show_area_t latency_title_show;
static lcd_show_area_t content_show;

static uint32_t speech_end_timestamp = 0;
static bool tts_first_frame_flag = true;
static bool first_speech_content_flag = true;
static bool first_responding_content_flag = true;

void audio_ctrl_callback(audio_ctrl_type_t type, audio_event_t event)
{
  if (type == AUDIO_CTRL_PLAYER)
  {
    lcd_show_clear(&hw_show);
    if (event == AUDIO_PLAYER_STOPPED)
    {
      ESP_LOGI(TAG, "== AUDIO_PLAYER_STOPPED ==");
      vTaskDelay(pdMS_TO_TICKS(100));
      player_stop();
      conversation_set_action(ACTION_PLAYER_STOPPED, NULL, 0);
      lcd_show_text_string_append(&hw_show, "播放完成");
    }
    else if (event == AUDIO_PLAYER_STARTED)
    {
      ESP_LOGI(TAG, "== AUDIO_PLAYER_STARTED ==");
      conversation_set_action(ACTION_PLAYER_STARTED, NULL, 0);
      lcd_show_text_string_append(&hw_show, "播放中 ...");
    }
  }
}

void event_callback(conv_event_t *event, void *param)
{
  if (event->msg_type == CONV_EVENT_BINARY)
  {
    player_insert_data(event->binary_data, event->binary_data_bytes);

    if (tts_first_frame_flag)
    {
      uint32_t tts_latency = esp_log_timestamp() - speech_end_timestamp;
      tts_first_frame_flag = false;
      lcd_show_clear(&latency_title_show);
      char latency_info[64] = {0};
      snprintf(latency_info, 63, "首包延迟 %ldms", tts_latency);
      lcd_show_text_string_append(&latency_title_show, latency_info);
    }
  }
  else
  {
    ESP_LOGI(TAG, "== %s ==", get_conv_event_type_string(event->msg_type));
    if (event->msg_type == CONV_EVENT_DIALOG_STATE_CHANGED)
    {
      g_dialog_state = event->dialog_state;
      ESP_LOGI(TAG, "- DIALOG_STATE_CHANGED: %s -",
               get_dialog_state_changed_string(event->dialog_state));
      lcd_show_clear(&sub_title_show);
      if (event->dialog_state == DIALOG_STATE_LISTENING)
      {
        lcd_show_color(&content_show, RED);
        lcd_show_text_string_append(&sub_title_show, "听 ...");
        recorder_start();
      }
      else if (event->dialog_state == DIALOG_STATE_IDLE)
      {
        first_speech_content_flag = true;
        first_responding_content_flag = true;
        lcd_show_text_string_append(&sub_title_show, "空闲 ...");
      }
      else if (event->dialog_state == DIALOG_STATE_RESPONDING)
      {
        lcd_show_text_string_append(&sub_title_show, "说 ...");
      }
      else if (event->dialog_state == DIALOG_STATE_THINKING)
      {
        lcd_show_text_string_append(&sub_title_show, "思考 ...");
      }
    }
    else if (event->msg_type == CONV_EVENT_SPEECH_ENDED)
    {
      recorder_stop();
    }
    else if (event->msg_type == CONV_EVENT_RESPONDING_STARTED)
    {
      player_start();
    }
    else if (event->msg_type == CONV_EVENT_RESPONDING_ENDED)
    {
      player_drain();
    }
    else if (event->msg_type == CONV_EVENT_SPEECH_CONTENT ||
             event->msg_type == CONV_EVENT_RESPONDING_CONTENT)
    {
      if (event->msg_type == CONV_EVENT_SPEECH_CONTENT &&
          first_speech_content_flag)
      {
        first_speech_content_flag = false;
        lcd_show_color(&content_show, RED);
      }
      if (event->msg_type == CONV_EVENT_RESPONDING_CONTENT &&
          first_responding_content_flag)
      {
        first_responding_content_flag = false;
        lcd_show_color(&content_show, GREEN);
      }

      cJSON *response_json = cJSON_Parse(event->msg);
      if (response_json != NULL)
      {
        cJSON *payload = cJSON_GetObjectItem(response_json, "payload");
        if (payload == NULL)
        {
          goto OUT_CONTENT;
        }
        cJSON *output = cJSON_GetObjectItem(payload, "output");
        if (output == NULL)
        {
          goto OUT_CONTENT;
        }
        cJSON *text = cJSON_GetObjectItem(output, "text");
        if (text == NULL)
        {
          goto OUT_CONTENT;
        }
        ESP_LOGI(TAG, "%s: %s",
                 event->msg_type == CONV_EVENT_SPEECH_CONTENT ? "HUMAN" : "AI",
                 text->valuestring);

        lcd_show_text_string_append(&content_show, text->valuestring);

      OUT_CONTENT:
        cJSON_Delete(response_json);
      }
    }
    else if (event->msg_type == CONV_EVENT_INTERRUPT_ACCEPTED)
    {
      player_stop();
    }
    else if (event->msg_type == CONV_EVENT_CONVERSATION_FAILED ||
             event->msg_type == CONV_EVENT_CONNECTION_DISCONNECTED ||
             event->msg_type == CONV_EVENT_CONVERSATION_COMPLETED)
    {
      ESP_LOGI(TAG, "All response: %s", event->msg);
      g_running = false;
      player_stop();
    }

    g_event_type = event->msg_type;
  }
}

void recorder_task(void *arg)
{
  ESP_LOGI(TAG, "recorder_task begin ...");
  QueueSetMemberHandle_t activate_member = NULL;
  conv_ret_code_t conv_ret = CONV_SUCCESS;
  size_t bytes_read = 0;                   /* 读取录音大小 */
  uint8_t rval = 0xFE;                     /* 获取TF状态 */
  FF_DIR recdir;                           /* 目录 */
  FIL *f_rec = (FIL *)malloc(sizeof(FIL)); /* 文件指针 */
  uint8_t *pname = malloc(255);            /* 文件名称存储buf */
  uint8_t *current_buffer = malloc(REC_RX_BUF_SIZE);

  if (ENABLE_USING_SDCARD_CACHE)
  {
    /* 打开文件夹，若没有，则自动创建 */
    while (f_opendir(&recdir, SDCARD_RECORDER_DIR))
    {
      f_mkdir(SDCARD_RECORDER_DIR); /* 创建该目录 */
    }
  }

  while (!g_key_scanning)
  {
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  while (g_running)
  {
    activate_member =
        xQueueSelectFromSet(xQueueSet, 4); /* 等待队列集中的队列接收到消息 */

    if (activate_member == key0_pres_xSemaphore)
    {
      ESP_LOGI(TAG, "take key0_pres_xSemaphore ...");
      xSemaphoreTake(activate_member, portMAX_DELAY);
      ESP_LOGI(TAG, "take key0_pres_xSemaphore done, g_recorder_sta:%x",
               g_recorder_sta);

      conv_ret = conversation_set_action(ACTION_START_HUMAN_SPEECH, NULL, 0);
      if (conv_ret != CONV_SUCCESS)
      {
        ESP_LOGE(TAG, "request SEND_SPEECH failed.");
        break;
      }

      ESP_LOGI(TAG, "enter rec mode ...");
      player_stop();
      recorder_start();
      g_recorder_sta |= 0x80;

      if (ENABLE_USING_SDCARD_CACHE)
      {
        pname[0] = 0; /* pname没有任何文件名 */
        recorder_new_pathname(SDCARD_RECORDER_DIR, pname);
        rval = f_open(f_rec, (const TCHAR *)pname, FA_CREATE_ALWAYS | FA_WRITE);
        /* 打开文件失败 */
        if (rval != FR_OK)
        {
          rval = 0xFE; /* 提示SD卡问题 */
          ESP_LOGE(TAG, "打开文件失败");
        }
      }
    }
    else if (activate_member == key0_releas_xSemaphore)
    {
      ESP_LOGI(TAG, "take key0_releas_xSemaphore ...");
      xSemaphoreTake(activate_member, portMAX_DELAY);
      ESP_LOGI(TAG, "exit rec mode ...");
      recorder_stop();
      g_recorder_sta = 0;
      rval = 0xFE;
      if (ENABLE_USING_SDCARD_CACHE)
      {
        f_close(f_rec);
      }

      conv_ret = conversation_set_action(ACTION_STOP_HUMAN_SPEECH, NULL, 0);
      if (conv_ret != CONV_SUCCESS)
      {
        ESP_LOGE(TAG, "request STOP_SPEECH failed.");
        break;
      }
      speech_end_timestamp = esp_log_timestamp();
      tts_first_frame_flag = true;
    }
    else if (activate_member == key1_pres_xSemaphore)
    {
      ESP_LOGI(TAG, "take key1_pres_xSemaphore ...");
      xSemaphoreTake(activate_member, portMAX_DELAY);
      if (!ENABLE_USING_SDCARD_CACHE)
      {
        ESP_LOGE(TAG, "ignore this function ......");
        continue;
      }
      ESP_LOGI(TAG, "take key1_pres_xSemaphore done, g_recorder_sta:%x",
               g_recorder_sta);

      g_recorder_sta |= 0x40;
      memset(pname, 0, 255);
      sprintf((char *)pname, "%s/TEST0.WAV", SDCARD_RECORDER_DIR);
      rval = f_open(f_rec, (const TCHAR *)pname, FA_READ);
      /* 打开文件失败 */
      if (rval != FR_OK)
      {
        ESP_LOGE(TAG, "打开文件 %s 失败 %d", pname, rval);
        g_recorder_sta = 0;
        rval = 0xFE; /* 提示SD卡问题 */
      }
    }
    else if (activate_member == key2_pres_xSemaphore)
    {
      ESP_LOGI(TAG, "take key2_pres_xSemaphore ...");
      xSemaphoreTake(activate_member, portMAX_DELAY);
      if (!ENABLE_USING_SDCARD_CACHE)
      {
        ESP_LOGE(TAG, "ignore this function ......");
        continue;
      }
      ESP_LOGI(TAG, "take key2_pres_xSemaphore done, g_recorder_sta:%x",
               g_recorder_sta);

      recorder_start();
      player_start();
      g_recorder_sta |= 0x20;
    }

    /* 处理录音中数据写入与状态显示 */
    if (g_recorder_sta & 0x80)
    {
      ESP_LOGD(TAG, "ready to recording %dbytes ...", REC_RX_BUF_SIZE);
      memset(current_buffer, 0, REC_RX_BUF_SIZE);
      bytes_read = recorder_fetch_data(current_buffer, REC_RX_BUF_SIZE);
      ESP_LOGD(TAG, "recording %dbytes done", bytes_read);

      if (g_dialog_state == DIALOG_STATE_LISTENING)
      {
        if (bytes_read == REC_RX_BUF_SIZE)
        {
          if (ENABLE_USING_SDCARD_CACHE && rval == FR_OK)
          {
            UINT bw;
            f_write(f_rec, current_buffer, bytes_read, (UINT *)&bw);
          }

          if (conversation_send_audio_data(
                  current_buffer, bytes_read * sizeof(unsigned char)) !=
              CONV_SUCCESS)
          {
            ESP_LOGE(TAG, "request send audio failed.");
          }
          else
          {
            ESP_LOGV(TAG, "request send %dbytes audio success.", bytes_read);
          }
        }
        else
        {
          ESP_LOGW(TAG, "skip with %dbytes data ....", bytes_read);
        }
      }
      else
      {
        ESP_LOGW(TAG, "current dialog state is %s, skip %dbytes.",
                 get_dialog_state_changed_string(g_dialog_state), bytes_read);
      }

      vTaskDelay(pdMS_TO_TICKS(20));
    }
    else if (g_recorder_sta & 0x40)
    {
      g_recorder_sta = 0;
      if (rval == FR_OK)
      {
        conv_ret = conversation_set_action(ACTION_START_HUMAN_SPEECH, NULL, 0);
        if (conv_ret != CONV_SUCCESS)
        {
          ESP_LOGE(TAG, "request SEND_SPEECH failed.");
          break;
        }
        player_start();

        int send_total_bytes = 0;
        do
        {
          f_read(f_rec, current_buffer, REC_RX_BUF_SIZE, (UINT *)&bytes_read);
          ESP_LOGD(TAG, "ready to send %dbytes data.", bytes_read);
          if (conversation_send_audio_data(
                  current_buffer, bytes_read * sizeof(unsigned char)) !=
              CONV_SUCCESS)
          {
            ESP_LOGE(TAG, "request send audio failed.");
            break;
          }
          else
          {
            player_insert_data(current_buffer, bytes_read);

            send_total_bytes += bytes_read;
            ESP_LOGI(TAG, "request send %dbytes(total:%dbytes) audio success.",
                     bytes_read, send_total_bytes);
            vTaskDelay(pdMS_TO_TICKS(60));
          }
        } while (bytes_read > 0); // while

        conv_ret = conversation_set_action(ACTION_STOP_HUMAN_SPEECH, NULL, 0);
        if (conv_ret != CONV_SUCCESS)
        {
          ESP_LOGE(TAG, "request STOP_SPEECH failed.");
          break;
        }

        player_stop();

        f_close(f_rec);
      }
      rval = 0xFE;
    }
    else if (g_recorder_sta & 0x20)
    {
      ESP_LOGD(TAG, "ready to recording %dbytes ...", REC_RX_BUF_SIZE);
      memset(current_buffer, 0, REC_RX_BUF_SIZE);
      bytes_read = recorder_fetch_data(current_buffer, REC_RX_BUF_SIZE);
      ESP_LOGI(TAG, "recording %dbytes done", bytes_read);

      if (bytes_read > 0)
      {
        player_insert_data(current_buffer, bytes_read);
      }

      vTaskDelay(pdMS_TO_TICKS(20));
    }
  } // while

  vTaskDelay(pdMS_TO_TICKS(100));

  free(f_rec);
  free(pname);
  free(current_buffer);

  ESP_LOGI(TAG, "thread recorder_task exit ...");
  vTaskDelete(NULL);
}

void key_task(void *arg)
{
  ESP_LOGI(TAG, "key_task begin ...");
  // QueueSetMemberHandle_t activate_member = NULL;
  uint8_t key = KEY_NONE;
  uint8_t last_round_key = KEY_NONE;
  uint8_t key_action = KEY_NONE;

  /* 创建队列集 */
  xQueueSet = xQueueCreateSet(4);
  /* 创建信号量 */
  key0_pres_xSemaphore = xSemaphoreCreateBinary();
  key0_releas_xSemaphore = xSemaphoreCreateBinary();
  key1_pres_xSemaphore = xSemaphoreCreateBinary();
  key2_pres_xSemaphore = xSemaphoreCreateBinary();
  /* 把信号量加入到队列集 */
  xQueueAddToSet(key0_pres_xSemaphore, xQueueSet);
  xQueueAddToSet(key0_releas_xSemaphore, xQueueSet);
  xQueueAddToSet(key1_pres_xSemaphore, xQueueSet);
  xQueueAddToSet(key2_pres_xSemaphore, xQueueSet);

  g_key_scanning = true;

  while (g_running)
  {
    key = key_scan(1);

    // ESP_LOGV(TAG, "key scan:%d, last:%d", key, last_round_key);

    key_action = KEY_NONE;
    if (key == 0 && last_round_key == BOOT_PRES)
    {
      key_action = KEY0_RELES;
    }
    if (key == BOOT_PRES && last_round_key == 0)
    {
      key_action = KEY0_PRES;
    }

    switch (key_action)
    {
    case KEY0_PRES:
      xSemaphoreGive(key0_pres_xSemaphore);
      ESP_LOGI(TAG, "KEY0 button press ...");
      break;
    case KEY0_RELES:
      xSemaphoreGive(key0_releas_xSemaphore);
      ESP_LOGI(TAG, "KEY0 button release ...");
      break;
    case KEY1_PRES:
      xSemaphoreGive(key1_pres_xSemaphore);
      ESP_LOGI(TAG, "KEY1 button press ...");
      break;
    case KEY2_PRES:
      xSemaphoreGive(key2_pres_xSemaphore);
      ESP_LOGI(TAG, "KEY2 button press ...");
      break;
    default:
      break;
    }

    last_round_key = key;

    vTaskDelay(pdMS_TO_TICKS(20));
  } // while

  ESP_LOGI(TAG, "thread key_task exit ...");
  vTaskDelete(NULL);
}

/*
  {
    "apikey": "xxxxx",
    "url": "wss://dashscope.aliyuncs.com/api-ws/v1/inference",
    "chain_mode": 0,
    "ws_protocol_ver": 3,
    "app_id": "xxxxx",
    "workspace_id": "xxxxx",
    "mode": "push2talk",
    "upstream": {
      "type": "AudioOnly",
      "mode": "push2talk",
      "audio_format": "pcm"
    },
    "downstream": {
      "type": "Audio",
      "audio_format": "pcm",
      "intermediate_text": "transcript,dialog"
    },
    "client_info": {
      "user_id": "xxxxx"
    },
    "dialog_attributes": {
      "prompt": "你是个有用的语音助手"
    }
  }
*/
const char *genInitParams()
{
  cJSON *root = cJSON_CreateObject();

#include "usr_access.h"

  cJSON_AddNumberToObject(root, "chain_mode", CONFIG_DEFAULT_CONV_CHAIN_MODE);
  cJSON_AddNumberToObject(root, "ws_protocol_ver",
                          CONFIG_DEFAULT_WS_PROTOCOL_VER);
  cJSON_AddStringToObject(root, "mode", CONFIG_DEFAULT_CONV_SERVICE_MODE);
  cJSON_AddStringToObject(root, "task_group",
                          CONFIG_DEFAULT_PROTOCOL_TASK_GROUP);
  cJSON_AddStringToObject(root, "task", CONFIG_DEFAULT_PROTOCOL_TASK);
  cJSON_AddStringToObject(root, "function", CONFIG_DEFAULT_PROTOCOL_FUNCTION);
  cJSON_AddNumberToObject(root, "log_level", CONV_LOG_LEVEL_INFO);

  cJSON *upstream = cJSON_CreateObject();
  cJSON_AddStringToObject(upstream, "type", CONFIG_DEFAULT_UPSTREAM_TYPE);
  cJSON_AddStringToObject(upstream, "mode", CONFIG_DEFAULT_CONV_SERVICE_MODE);
  cJSON_AddStringToObject(upstream, "audio_format",
                          CONFIG_DEFAULT_RECORDER_FORMAT);
  cJSON_AddItemToObject(root, "upstream", upstream);

  cJSON *downstream = cJSON_CreateObject();
  cJSON_AddStringToObject(downstream, "type", CONFIG_DEFAULT_DOWNSTREAM_TYPE);
  cJSON_AddStringToObject(downstream, "audio_format", TTS_AUDIO_FORMAT);
  cJSON_AddNumberToObject(downstream, "sample_rate",
                          default_player_sample_rate);
  cJSON_AddStringToObject(downstream, "voice", "longcheng_v2");
  cJSON_AddStringToObject(downstream, "intermediate_text",
                          CONFIG_DEFAULT_DOWNSTREAM_INTERMEDIATE_TEXT);
  cJSON_AddItemToObject(root, "downstream", downstream);

  cJSON *client_info = cJSON_CreateObject();
  cJSON_AddStringToObject(client_info, "user_id", "empty_user_id");
  cJSON_AddItemToObject(root, "client_info", client_info);

  cJSON *dialog_attributes = cJSON_CreateObject();
  cJSON_AddStringToObject(dialog_attributes, "prompt",
                          CONFIG_DEFAULT_DIALOG_ATTRIBUTES_PROMPT);
  cJSON_AddItemToObject(root, "dialog_attributes", dialog_attributes);

  char *result_json = cJSON_Print(root);
  cJSON_Delete(root);

  return result_json;
}

void board_dnesp32s3_init()
{
  esp_err_t sys_ret = nvs_flash_init(); /* 初始化NVS */
  if (sys_ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      sys_ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
  }

  // my_spi_init(); /* SPI初始化 */
  key_init(); /* KEY初始化 */
  myiic_init();  /* MYIIC初始化 */
  user_es8311_init(); /* ES8311初始化 */


  // xl9555_init(); /* XL9555初始化 */
  // spilcd_init(); /* SPILCD初始化 */

  // /* ES8388初始化 */
  // while (es8388_init()) {
  //   //spilcd_show_string(30, 110, 200, 16, 16, "ES8388 Error", RED);
  //   vTaskDelay(pdMS_TO_TICKS(200));
  //   //spilcd_fill(30, 110, 239, 126, WHITE);
  //   vTaskDelay(pdMS_TO_TICKS(200));
  // }

  //   xl9555_pin_write(SPK_EN_IO, 0); /* 打开喇叭 */

  //   /* 检测不到SD卡 */
  //   while (sd_spi_init()) {
  //  //   spilcd_show_string(30, 110, 200, 16, 16, "SD Card Error!", RED);
  //     vTaskDelay(pdMS_TO_TICKS(500));
  // //    spilcd_show_string(30, 130, 200, 16, 16, "Please Check! ", RED);
  //     vTaskDelay(pdMS_TO_TICKS(500));
  //   }

  sys_ret = exfuns_init(); /* 为fatfs相关变量申请内存 */

  // uint8_t key = 0;
  // char show_buf[32];
  // while (fonts_init()) {
  //   /* 检查字库 */
  //   spilcd_clear(WHITE);
  //   spilcd_show_string(30, 30, 200, 16, 16, "ESP32-S3", RED);

  //   key = fonts_update_font(30, 50, 16, (uint8_t*)"0:", RED); /* 更新字库 */

  //   while (key) {
  //     /* 更新失败 */
  //     memset(show_buf, 0, 32);
  //     snprintf(show_buf, 32, "Font Update Failed! %d", key);
  //     spilcd_show_string(30, 50, 200, 16, 16, show_buf, RED);
  //     vTaskDelay(pdMS_TO_TICKS(200));
  //     spilcd_fill(20, 50, 200 + 20, 90 + 16, WHITE);
  //     vTaskDelay(pdMS_TO_TICKS(200));
  //   }

  //   spilcd_show_string(30, 50, 200, 16, 16, "Font Update Success!   ", RED);
  //   vTaskDelay(pdMS_TO_TICKS(1000));
  //   spilcd_clear(WHITE);
  // }

  // lcd_show_init(&title_show, "TITLE", GB_TABLE, 10, 10, 300, 24, BLACK, 24,
  //               WHITE);
  // lcd_show_init(&sub_title_show, "SUB_TITLE", GB_TABLE, 10, 35, 140, 16, BLUE,
  //               16, WHITE);
  // lcd_show_init(&hw_show, "HW_TITLE", GB_TABLE, 170, 35, 150, 16, BLUE, 16,
  //               WHITE);
  // lcd_show_init(&latency_title_show, "LATENCY", GB_TABLE, 10, 52, 300, 48, BLUE,
  //               16, WHITE);
  // lcd_show_init(&content_show, "CONTENT", GB_TABLE, 10, 110, 300, 130, RED, 16,
  //               WHITE);

  // lcd_show_text_string_append(&title_show, "ESP32-S3 开发板");

  vTaskDelay(pdMS_TO_TICKS(2000));
}

static esp_err_t es8311_board_test(void)
{
  // 读取Chip ID (寄存器地址 0x00)
  uint8_t chip_id;
  uint8_t ret = es8311_read_reg(0x00, &chip_id);
  if (ret == ESP_OK)
  {
    ESP_LOGI(TAG, "ES8311 Chip ID: 0x%02x", chip_id);
  }
  else
  {
    ESP_LOGE(TAG, "Failed to read ES8311 Chip ID");
  }
  return ret;
}
void app_main(void)
{
  esp_log_level_set("*", ESP_LOG_VERBOSE);

  board_dnesp32s3_init(); /* 硬件初始化 */
  es8311_register_dump();
  while (1){
    vTaskDelay(1000);
    ESP_LOGI(TAG, "ES8311 codec initialized successfully.");
      es8311_register_dump();    /* ES8311测试 */
  }
  //  lcd_show_clear(&title_show);
  //  lcd_show_text_string_append(&title_show, "WIFI链接 ...");
  wifi_sta_init(); /* 链接WIFI */

  ESP_LOGI(TAG, "app_main ->");
  ESP_LOGI(TAG, "test voice-chat %s ->", conversation_get_version());

  //  lcd_show_clear(&title_show);
  //  lcd_show_text_string_append(&title_show, "交互启动中 ...");
  create_conversation(event_callback, NULL);

  conv_ret_code_t conv_ret = conversation_connect(genInitParams());
  ESP_LOGI(TAG, "conversation_connect done(%d).", conv_ret);
  if (conv_ret != CONV_SUCCESS)
  {
    return;
  }

  memset(&audio_ctrl, 0, sizeof(audio_ctrl));
  audio_ctrl.recorder_sr = default_recorder_sample_rate;
  recorder_init(&audio_ctrl);
  audio_ctrl.player_sr = default_player_sample_rate;
  strncpy(audio_ctrl.decoder_format, TTS_AUDIO_FORMAT, 8);
  strncpy(audio_ctrl.sdcard_decoding_dir, SDCARD_DECODER_DIR, 64);
  strncpy(audio_ctrl.sdcard_decoding_base, SDCARD_DECODER_BASE, 64);
  strncpy(audio_ctrl.sdcard_player_dir, SDCARD_PLAYER_DIR, 64);
  strncpy(audio_ctrl.sdcard_player_base, SDCARD_PLAYER_BASE, 64);
  audio_ctrl.audio_ctrl_callback = audio_ctrl_callback;
  audio_ctrl.enable_using_sdcard_cache = ENABLE_USING_SDCARD_CACHE;
  player_init(&audio_ctrl);

  g_running = true;

  /* 按键监控 */
  TaskHandle_t key_task_handler;
  xTaskCreate(key_task, "key_task_work", CONFIG_DEFAULT_CONV_STACK_SIZE,
              &key_task_handler, 5, NULL);

  /* MIC录音处理 */
  TaskHandle_t rec_task_handler;
  xTaskCreate(recorder_task, "recorder_task_work",
              CONFIG_DEFAULT_CONV_STACK_SIZE, &rec_task_handler, 15, NULL);

  ESP_LOGI(TAG, "Working ......");

  //  lcd_show_clear(&title_show);
  //  lcd_show_text_string_append(&title_show, "开始交互吧！");

  /* forever */
  while (g_running)
  {
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  conv_ret = conversation_disconnect();
  if (conv_ret != CONV_SUCCESS)
  {
    ESP_LOGE(TAG, "conversation_disconnect failed(%d).", conv_ret);
  }

  /* 等待交互结束 */
  while (g_event_type != CONV_EVENT_CONVERSATION_COMPLETED &&
         g_event_type != CONV_EVENT_CONVERSATION_FAILED)
  {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  conv_ret = destroy_conversation();
  if (conv_ret != CONV_SUCCESS)
  {
    ESP_LOGE(TAG, "destroy_conversation failed(%d).", conv_ret);
  }
  recorder_deinit();
  player_deinit();

  ESP_LOGI(TAG, "test voice-chat done.");

}
