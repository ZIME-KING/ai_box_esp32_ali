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
#ifndef VOICE_CHAT_API_CONVERSATION_H
#define VOICE_CHAT_API_CONVERSATION_H

#include <stddef.h>
#include <stdint.h>

#include "conv_code.h"
#include "conv_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建交互，设置回调。对应销毁接口为destroy_conversation
 * @param on_message : 各事件监听回调，参见下文具体回调
 * @param user_data : 回调返回
 * @return conv_instance_t : Conversation单例指针
 */
conv_instance_t create_conversation(const conv_event_callback_t on_message,
                                    void* user_data);

/**
 * @brief 销毁交互，若交互进行中，则内部完成conversation_disconnect
 * @return conv_ret_code_t : 状态码
 */
conv_ret_code_t destroy_conversation();

/**
 * @brief 与服务端建立链接
 * @param params ：json string形式的初始化参数
 * @return conv_ret_code_t : 状态码
 */
conv_ret_code_t conversation_connect(const char* params);

/**
 * @brief 断开链接
 * @return conv_ret_code_t : 状态码
 */
conv_ret_code_t conversation_disconnect();

/**
 * @brief 按键(Tap)打断。正在播放时，调用此接口请求打断播放。
 * @return conv_ret_code_t : 状态码
 */
conv_ret_code_t conversation_interrupt();

/**
 * @brief 推送音频数据
 * @param data : 音频数据，PCM格式
 * @param data_size : 音频数据字节数
 * @return conv_ret_code_t : 状态码
 */
conv_ret_code_t conversation_send_audio_data(const uint8_t* data,
                                             size_t data_size);

/**
 * @brief 推送参考音频数据，在音频数据送给播放器时推送
 * @param data : 音频数据，PCM格式
 * @param data_size : 音频数据字节数
 * @return conv_ret_code_t : 状态码
 */
conv_ret_code_t conversation_send_ref_data(const uint8_t* data,
                                           size_t data_size);

/**
 * @brief
 * 通知服务端与用户主动交互，可以直接把上传的文本转换为语音下发，也可以上传文本调用大模型，返回的结果再转换为语音下发
 * @param params ：json string形式的初始化参数
 * @return conv_ret_code_t : 状态码
 */
conv_ret_code_t conversation_send_response_data(const char* params);

/**
 * @brief
 * 向SDK送入通过RTC获得的VoiceChat相关response。response将在VideoChat Native
 * SDK内部进行解析，转成交互状态和相关事件给APP。
 * @param params ：json string形式的初始化参数
 * @return conv_ret_code_t : 状态码
 */
conv_ret_code_t conversation_get_response(const char* params);

/**
 * @brief 触发SDK动作，比如音频（播放）开始/结束事件告知到SDK
 * @param action : 触发的动作类型
 * @param data : 此次SDK动作附带的数据, 比如音频数据比如json字符串
 * @param data_size : 此次SDK动作附带的数据的字节数
 * @return conv_ret_code_t : 状态码
 */
conv_ret_code_t conversation_set_action(conv_action_t action,
                                        const uint8_t* data, size_t data_size);

/**
 * @brief 获得当前状态
 * @param type : 状态类型
 * @return conv_ret_code_t : 状态码
 */
int conversation_get_state(state_type_t type);

/**
 * @brief 发送如P2T相关的参数
 * @param params : 参数
 * @return conv_ret_code_t : 状态码
 */
conv_ret_code_t conversation_update_message(const char* params);

/**
 * @brief 获得当前SDK版本号
 * @return const char* : 版本信息
 */
const char* conversation_get_version();

/**
 * @brief 获得当前SDK中参数值
 * @param param : 需要获得参数名
 * @return const char* : 参数值，普通字符串或json字符串
 */
const char* conversation_get_parameter(const char* param);

#ifdef __cplusplus
}
#endif

#endif  // VOICE_CHAT_API_CONVERSATION_H
