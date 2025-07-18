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
#ifndef CONFIGURATION_DEVICE_CONFIG_H
#define CONFIGURATION_DEVICE_CONFIG_H

//arch
#ifdef _BOARD_ASR3601_
#include <arch/asr3601_atomic.h>
#elif defined _BOARD_SC5654A_
#include <arch/sc5654a_atomic.h>
#elif defined _BOARD_ATS3607D_
#include <arch/ats3607d_atomic.h>
#elif defined _BOARD_ATS3609D_
#include <arch/ats3609d_atomic.h>
#else
#endif

#ifndef CONV_STRINGIZE_
#define CONV_STRINGIZE_(a) #a
#define CONV_STRINGIZE(a) CONV_STRINGIZE_(a)
#endif

// #define TARGET_PLATFORM_CONFIG_FILE esp32s3
// #include CONV_STRINGIZE(TARGET_PLATFORM_CONFIG_FILE.h)

/******** conv_config 配置信息 ********/
/* CONV_SERVICE_MODE
 * push2talk
 */
#ifndef CONFIG_DEFAULT_CONV_SERVICE_MODE
#define CONFIG_DEFAULT_CONV_SERVICE_MODE "push2talk"
#endif
#ifndef CONFIG_DEFAULT_CONV_SERVICE_PRIORITY
#define CONFIG_DEFAULT_CONV_SERVICE_PRIORITY 20
#endif
#ifndef CONFIG_DEFAULT_CONV_STACK_SIZE
#define CONFIG_DEFAULT_CONV_STACK_SIZE 12288
#endif
#ifndef CONFIG_DEFAULT_CONV_CHAIN_MODE
#define CONFIG_DEFAULT_CONV_CHAIN_MODE 0
#endif
#ifndef CONFIG_DEFAULT_WS_PROTOCOL_VER
#define CONFIG_DEFAULT_WS_PROTOCOL_VER 3
#endif

// VFS FAT 文件路径名最大长度
#ifndef CONV_FILENAME_MAX
#define CONV_FILENAME_MAX 64
#endif

// 可供nui使用的内存总大小，单位 字节
#ifndef CONV_MEM_SIZE
#define CONV_MEM_SIZE 49152
#endif

// 可供se使用的内存总大小， 单位 字节
#ifndef SE_MEM_SIZE
#define SE_MEM_SIZE 0
#endif

// 每次分配的最小内存大小，单位 字节
#ifndef CONV_MEM_MIN_SIZE
#define CONV_MEM_MIN_SIZE 16
#endif

/**
 * MEM_ALIGNMENT: should be set to the alignment of the CPU
 *    4 byte alignment -> #define MEM_ALIGNMENT 4
 *    2 byte alignment -> #define MEM_ALIGNMENT 2
 */
#ifndef CONV_MEM_ALIGNMENT
#define CONV_MEM_ALIGNMENT 4
#endif

// conv_log的打印级别
// 0 - none ; 1 - error ; 2 - warn ; 3 - info ; 4 - debug ; 5 - verbose
#ifndef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL 5
#endif

// recorder 配置信息
#ifndef CONFIG_DEFAULT_RECORDER_SAMPLE_RATE
#define CONFIG_DEFAULT_RECORDER_SAMPLE_RATE 16000
#endif
#ifndef CONFIG_DEFAULT_RECORDER_BITS_PER_SAMPLE
#define CONFIG_DEFAULT_RECORDER_BITS_PER_SAMPLE 16
#endif
#ifndef CONFIG_DEFAULT_RECORDER_CHANNELS
#define CONFIG_DEFAULT_RECORDER_CHANNELS 1
#endif
#ifndef CONFIG_DEFAULT_RECORDER_FORMAT
#define CONFIG_DEFAULT_RECORDER_FORMAT "pcm"
#endif
#ifndef CONFIG_DEFAULT_RECORDER_RINGBUF_SIZE
#define CONFIG_DEFAULT_RECORDER_RINGBUF_SIZE 0
#endif

// player 配置信息
#ifndef CONFIG_DEFAULT_PLAYER_SAMPLE_RATE
#define CONFIG_DEFAULT_PLAYER_SAMPLE_RATE 16000
#endif
#ifndef CONFIG_DEFAULT_PLAYER_BITS_PER_SAMPLE
#define CONFIG_DEFAULT_PLAYER_BITS_PER_SAMPLE 16
#endif
#ifndef CONFIG_DEFAULT_PLAYER_CHANNELS
#define CONFIG_DEFAULT_PLAYER_CHANNELS 1
#endif
#ifndef CONFIG_DEFAULT_PLAYER_FORMAT
#define CONFIG_DEFAULT_PLAYER_FORMAT "pcm"
#endif
#ifndef CONFIG_DEFAULT_PLAYER_RINGBUF_SIZE
#define CONFIG_DEFAULT_PLAYER_RINGBUF_SIZE 0
#endif

/******** net config 配置文件 ********/
#ifndef CONFIG_DEFAULT_APIKEY
#define CONFIG_DEFAULT_APIKEY "sk-85d1601205d246a2b541c794d3dd3561"
#endif
#ifndef CONFIG_DEFAULT_URL
#define CONFIG_DEFAULT_URL "wss://dashscope.aliyuncs.com/api-ws/v1/inference"
#endif
#ifndef CONFIG_DEFAULT_APP_ID
#define CONFIG_DEFAULT_APP_ID "18c468a404004c9c95ebb6b2d0b06e15"
#endif
#ifndef CONFIG_DEFAULT_WORKSPACE_ID
#define CONFIG_DEFAULT_WORKSPACE_ID "llm-4wlqoa100jpykcdz"
#endif

/******** model config 配置文件 ********/
#ifndef CONFIG_DEFAULT_PROTOCOL_TASK_GROUP
#define CONFIG_DEFAULT_PROTOCOL_TASK_GROUP "aigc"
#endif
#ifndef CONFIG_DEFAULT_PROTOCOL_TASK
#define CONFIG_DEFAULT_PROTOCOL_TASK "multimodal-generation"
#endif
#ifndef CONFIG_DEFAULT_PROTOCOL_FUNCTION
#define CONFIG_DEFAULT_PROTOCOL_FUNCTION "generation"
#endif
#ifndef CONFIG_DEFAULT_PROTOCOL_MODEL
#define CONFIG_DEFAULT_PROTOCOL_MODEL "multimodal-dialog"
#endif
#ifndef CONFIG_DEFAULT_UPSTREAM_TYPE
#define CONFIG_DEFAULT_UPSTREAM_TYPE "AudioOnly"
#endif
#ifndef CONFIG_DEFAULT_DOWNSTREAM_TYPE
#define CONFIG_DEFAULT_DOWNSTREAM_TYPE "Audio"
#endif
#ifndef CONFIG_DEFAULT_DOWNSTREAM_INTERMEDIATE_TEXT
#define CONFIG_DEFAULT_DOWNSTREAM_INTERMEDIATE_TEXT "transcript,dialog"
#endif
#ifndef CONFIG_DEFAULT_DIALOG_ATTRIBUTES_PROMPT
#define CONFIG_DEFAULT_DIALOG_ATTRIBUTES_PROMPT "你是个有用的语音助手"
#endif

#endif // CONFIGURATION_DEVICE_CONFIG_H

