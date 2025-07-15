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
#ifndef CONVSDK_INCLUDE_CONV_EVENT_H
#define CONVSDK_INCLUDE_CONV_EVENT_H

typedef enum {
  CONV_EVENT_NONE = -1,

  /**
   * 发生异常或接收到异常消息
   */
  CONV_EVENT_CONVERSATION_FAILED = 0,

  /**
   * 与AI(服务端)建连成功
   */
  CONV_EVENT_CONVERSATION_CONNECTED,

  /**
   * AI(服务端)登场, 开始会话任务
   * "Started"
   */
  CONV_EVENT_CONVERSATION_STARTED,

  /**
   * AI(服务端)退场, 会话任务完成, 链接断开
   * "Stopped"
   */
  CONV_EVENT_CONVERSATION_COMPLETED,

  /**
   * 检测HUMAN开始说话, 由端侧VAD检测到起点触发
   * "SpeechBegin" or "SpeechStarted"
   */
  CONV_EVENT_SPEECH_STARTED,

  /**
   * HUMAN说话结束, 由AI(服务端)判停
   * "SpeechEnded"
   */
  CONV_EVENT_SPEECH_ENDED,

  /**
   * AI(服务端)开始传回TTS数据
   * "RespondingStarted"
   */
  CONV_EVENT_RESPONDING_STARTED,

  /**
   * AI(服务端)传回TTS数据完成
   * "RespondingEnded"
   */
  CONV_EVENT_RESPONDING_ENDED,

  /**
   * 表示此conv_event_t中包含AI(服务端)传回的TTS数据包
   * "Binary"
   */
  CONV_EVENT_BINARY,

  /**
   * 最近一次传入的HUMAN说话声音数据的音量值
   */
  CONV_EVENT_SOUND_LEVEL,

  /**
   * 对话状态发生变化
   * "DialogStateChanged"
   */
  CONV_EVENT_DIALOG_STATE_CHANGED = 11,

  /**
   * 轻触打断被允许
   * "RequestAccepted"
   */
  CONV_EVENT_INTERRUPT_ACCEPTED,

  /**
   * 轻触打断被拒绝
   * "RequestDenied"
   */
  CONV_EVENT_INTERRUPT_DENIED,

  /**
   * 语音打断被允许
   */
  CONV_EVENT_VOICE_INTERRUPT_ACCEPTED,

  /**
   * 语音打断被拒绝
   */
  CONV_EVENT_VOICE_INTERRUPT_DENIED,

  /**
   * SDK与服务端链接成功
   */
  CONV_EVENT_CONNECTION_CONNECTED,

  /**
   * SDK与服务端断开链接
   */
  CONV_EVENT_CONNECTION_DISCONNECTED,

  /**
   * 用户语音识别出的详细信息，比如文本
   * "SpeechContent"
   */
  CONV_EVENT_SPEECH_CONTENT,

  /**
   * 系统对外输出的详细信息，比如文本
   * "RespondingContent"
   */
  CONV_EVENT_RESPONDING_CONTENT,

  /**
   * 网络状态信息, 比如网络延迟
   * "NetworkStatus"
   */
  CONV_EVENT_NETWORK_STATUS = 20,

  /**
   * 未定义的消息信息
   */
  CONV_EVENT_OTHER_MESSAGE = 29,

  /**
   * !!!--- 以下为送给上层RTC SDK的事件 ---!!!
   */
  /**
   * 表示此conv_event_t中包含HUMAN(用户)传回的MIC数据包
   * "UserBinary"
   */
  CONV_EVENT_USER_BINARY = 30,

  /**
   * 表示将事件整理成RTC消息
   * "RTCMessage"
   */
  CONV_EVENT_RTC_MESSAGE,

  /**
   * !!!--- 以下为送给上层RTC SDK的事件请求, 将会转成kRTCMessage ---!!!
   */
  /**
   * "Start" or "StartedReceived"
   */
  CONV_EVENT_CONVERSATION_START,

  /**
   * "Stop"
   */
  CONV_EVENT_CONVERSATION_STOP,

  /**
   * "SendSpeech"
   */
  CONV_EVENT_SEND_SPEECH,

  /**
   * "StopSpeech"
   */
  CONV_EVENT_STOP_SPEECH,

  /**
   * "RequestToSpeak"
   */
  CONV_EVENT_REQUEST_TO_SPEAK,

  /**
   * "RequestToRespond"
   */
  CONV_EVENT_REQUEST_TO_RESPOND,

  /**
   * "LocalRespondingStarted"
   */
  CONV_EVENT_LOCAL_RESPONDING_STARTED,

  /**
   * "LocalRespondingEnded"
   */
  CONV_EVENT_LOCAL_RESPONDING_ENDED,

  /**
   * Valid cut event
   */
  CONV_EVENT_VALID_CUT
} conv_event_type_t;

typedef enum {
  DIALOG_STATE_NONE = -1,
  DIALOG_STATE_IDLE = 0,
  DIALOG_STATE_LISTENING,
  DIALOG_STATE_RESPONDING,
  DIALOG_STATE_THINKING
} dialog_state_changed_t;

typedef enum {
  CONV_STATE_UNKNOWN = -1,
  CONV_STATE_IDLE = 0,  // Disconnected
  CONV_STATE_CONNECTING,
  CONV_STATE_CONNECTED,    // Websocket connected
  CONV_STATE_STARTING,     // Send Start command
  CONV_STATE_STARTED,      // Received Started event
  CONV_STATE_STOPPING,     // Send Stop command
  CONV_STATE_STOPPED = 6,  // Received Stopped event
  CONV_STATE_INTERRUPTING,
  CONV_STATE_RUNNING,
  CONV_STATE_SPEECH_STARTED,
  CONV_STATE_SPEECH_ENDED,
  CONV_STATE_RESPONDING_STARTED,
  CONV_STATE_RESPONDING_ENDED,
  CONV_STATE_FAILED,
} conv_state_t;

typedef struct {
  int status_code;
  char *msg;
  conv_event_type_t msg_type;
  conv_event_type_t sub_msg_type;
  dialog_state_changed_t dialog_state;
  unsigned char *binary_data;
  unsigned int binary_data_bytes;
  char task_id[64];
  char dialog_id[64];
} conv_event_t;

extern void conv_event_set_dialog_state_changed(conv_event_t *src,
                                                dialog_state_changed_t state);
extern void conv_event_copy(conv_event_t *src, conv_event_t **dst);

extern const char *get_conv_event_type_string(conv_event_type_t type);
extern const char *get_conv_state_string(conv_state_t state);
extern const char *get_dialog_state_changed_string(
    dialog_state_changed_t state);

#endif