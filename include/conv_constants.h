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

#ifndef CONVSDK_CONSTANTS_H
#define CONVSDK_CONSTANTS_H

#include "conv_event.h"

typedef void *conv_instance_t;

typedef enum {
  CONV_MODE_PUSH_TO_TALK,
  CONV_MODE_TAP_TO_TALK,
  CONV_MODE_DUPLEX,
} conv_mode_t;

typedef enum {
  CONV_CHAIN_MODE_WEBSOCKET,
  CONV_CHAIN_MODE_RTC
} conv_chain_mode_t;

typedef enum {
  STATE_TYPE_DIALOG_STATE,
  STATE_TYPE_CONNECTION_STATE,
  STATE_TYPE_INTERRUPTION_POLICY,
  STATE_TYPE_MIC_STATE,
  STATE_TYPE_PLAYER_STATE,
  STATE_TYPE_APP_ACTION
} state_type_t;

typedef enum {
  CONV_CONNECTION_UNKNOWN = 0,
  CONV_DISCONNECTED,
  CONV_CONNECTED,
} conv_connection_state_t;

typedef enum {
  ACTION_MIC_STARTED = 0,
  ACTION_MIC_STOPPED,
  ACTION_PLAYER_STARTED,
  ACTION_PLAYER_STOPPED,
  ACTION_ENABLE_VOICE_INTERRUPTION = 6,
  ACTION_DISABLE_VOICE_INTERRUPTION,
  ACTION_VOICE_MUTE,
  ACTION_VOICE_UNMUTE,
  ACTION_START_HUMAN_SPEECH,
  ACTION_STOP_HUMAN_SPEECH,
  ACTION_CANCEL_HUMAN_SPEECH,
  ACTION_REQUEST_TO_SPEAK,
  ACTION_REQUEST_TO_RESPOND,
  ACTION_UPDATE_INFO,
} conv_action_t;

typedef enum {
  CONV_LOG_LEVEL_VERBOSE,
  CONV_LOG_LEVEL_DEBUG,
  CONV_LOG_LEVEL_INFO,
  CONV_LOG_LEVEL_WARNING,
  CONV_LOG_LEVEL_ERROR,
  CONV_LOG_LEVEL_NONE
} conv_log_level_t;

// global config entry
typedef enum {
  CONV_CONFIG_KEY_START = 0,
} config_key_t;

/**
 * @brief SDK各事件回调
 * @param conv_event_t*
 * ：回调事件，可通过调用其中具体方法获得事件及相关信息。具体方法见下方conv_event_t
 * @param void* ：用户设置的user_data
 */
typedef void (*conv_event_callback_t)(conv_event_t *, void *);

#endif  // CONVSDK_CONSTANTS_H
