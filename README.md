# 语音交互SDK说明

## 名词说明
P2T(Push To Talk): 按下按键开始说话，放开按键接收交互结果并播放。   
T2T(Tap To Talk): 轻点按钮，开始说话，停止说话后接收交互结果并播放。  
Duplex: 全双工模式，可语音打断，也支持TapToTalk。  
KWS_Duplex: 带唤醒功能的全双工模式，可语音打断，也支持TapToTalk。
  
## APP_CODE
02C: 纯云端 -> 默认带RTC不带WS的VoiceChat，不含内部AEC、VAD、KWS, 可选RTC、WS  


## FUN_CODE
|FUN_CODE|        /            |          /             |                 |    内部模块    |                 |          /          |         链路模式 |         /            |                     |  云端协议   |            |      /     |
|:------:|:-------------------:|:----------------------:|:---------------:|:--------------:|:--------------:|:-------------------:|:---------------:|:--------------------:|:------------------:|:----------:|:----------:|:----------:|
|        |23-20bit:<br>reserved|19bit:<br>内部Beamforming|18bit:<br>内部VAD|17bit:<br>内部AEC|16bit:<br>内部KWS|14-15bit:<br>reserved|13bit:<br>支持RTC|12bit:<br>支持Websocket|3-11bit:<br>reserved|2bit:<br>百炼|1bit:<br>NLS|0bit:<br>通义|
| 073007 |                     |                        | &#10003;        | &#10003;       | &#10003;       |                     | &#10003;        | &#10003;             |                    | &#10003;   | &#10003;   | &#10003;    |
| 063007 |                     |                        | &#10003;        | &#10003;       |                |                     | &#10003;        | &#10003;             |                    | &#10003;   | &#10003;   | &#10003;    |
| 003007 |                     |                        |                 |                |                |                     | &#10003;        | &#10003;             |                    | &#10003;   | &#10003;   | &#10003;    |
| 003004 |                     |                        |                 |                |                |                     | &#10003;        | &#10003;             |                    | &#10003;   |            |             |
| 001004 |                     |                        |                 |                |                |                     |                 | &#10003;             |                    | &#10003;   |            |             |


## SDK编译指令
### Linux平台编译及说明
#### 编译
用于Linux平台开发和先验功能。
> sh scripts/build_linux.sh  
#### 成果物说明
```
out/pc_linux/  
  │── demo  
  │   └── utestDemo                    单元测试的可执行文件  
  │── include  
  │   │── conv_code.h                  错误码定义文件  
  │   │── conv_constants.h             各枚举定义文件  
  │   │── conversation.h               SDK API接口头文件  
  │   └── conv_event.h                 事件回调相关信息的定义头文件  
  └── lib  
      │── libconversation_shared.so    SDK动态库  
      └── libconversation.a            SDK静态库  
```

### ESP32S3平台编译及说明
#### 编译:
> sh scripts/build_esp32s3.sh  
#### 成果物说明
```
out/esp32s3/install  
  └── V0.0.2-02C-20250611_ESP32S3   SDK包名, 包括版本号、应用号、构建日期和平台
      │── README.md                        此SDK的详细说明  
      │── RELEASE.note                     历史版本信息  
      │── version                          此SDK的版本号  
      │── example                          此SDK运行在esp32s3上的app示例工程  
      │── include  
      │   │── device_config.h              SDK在此平台运行的相关参数, 如栈大小、最大堆大小、默认参数等
      │   │── conv_utest.h                 调用SDK内部单元测试的接口头文件
      │   │── conv_code.h                  错误码定义文件  
      │   │── conv_constants.h             各枚举定义文件  
      │   │── conversation.h               SDK API接口头文件  
      │   └── conv_event.h                 事件回调相关信息的定义头文件  
      └── lib  
          └── libconversation.a            SDK静态库  
```

## SDK跨平台迁移说明
### SDK代码框架
```
video-chat-things
  │── common  
  |   │── event_hash                 SDK运行剧本的hash表
  |   │   └── push2talk.h            SDK运行Push2Talk模式的剧本 
  |   │── include                    对外头文件
  |   │   │── conv_code.h            错误码定义文件  
  |   │   └── conv_constants.h       各枚举定义文件  
  |   │── net                        网络交互协议模块
  |   │── utils                      SDK的底层工具(抽象层), 最终将会调用到具体的platform下的系统相关接口
  |   └── vc_core                    SDK工作流核心代码
  │── entity/ty                      对外实体代码, ty表示通义
  |   |—— api
  |   └── configuration              项目相关的配置
  |       |—— voice_chat_fullcloud   和项目相关的配置信息
  |       |—— build_esp32s3.cmake    运行在esp32s3平台的默认配置信息
  |       |—— build_pc_linux.cmake   运行在linux平台的默认配置信息
  |       |—— compiler.cmake         编译相关的cmake文件
  |       |—— flags.cmake            编译flag相关的cmake文件
  |       |—— device_config.h.in     用于生成配置信息的.in文件
  |       └── device_config.h        编译生成的配置信息文件, 不要编辑
  │── platform    平台和系统相关的接口实现, 可认为是HAL层, 代码迁移新平台请关注并实现此目录
  └── scripts     编译相关
```
### SDK新平台迁移说明
#### 实现系统接口
各类RTOS的接口较为接近，可模仿platform/esp32s3和platform/rtos/freertos_v1_10目录实现semaphore、task、timer等系统调用。
#### 编译链路
参考build_esp32s3.sh和build_esp32s3.cmake，创新新平台的编译参数。
修改platform/CMakeLists.txt来选择新平台的系统调用代码。

## 应用工程编译指令
### ESP32S3平台编译及说明
#### 示例工程编译
> cd example/esp32s3  
> idf.py set-target esp32s3 && idf.py build  
#### 烧录
以设备端口 /dev/ttyACM0 为例
> cd example/esp32s3  
> python -m esptool --chip esp32s3 --port /dev/ttyACM0 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x0 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0x10000 build/voice_chat.bin