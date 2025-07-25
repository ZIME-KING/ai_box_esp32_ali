set(example_srcs
    "voice_chat.c"
    "wifi_sta/wifi_sta.c"
    "audio/audio_controller.c"
    "cache/buffer_pool.c"
    "show/lcd_show.c"
    "display/lcd_display.cc"


    )
set(example_inc
    "."
    "wifi_sta"
    "audio"
    "cache"
    "show"
    "generated"
    "display"
    )
idf_component_register(SRCS ${example_srcs}
                       INCLUDE_DIRS ${example_inc}
                       EMBED_FILES "music-16b-2c-8000hz.mp3")