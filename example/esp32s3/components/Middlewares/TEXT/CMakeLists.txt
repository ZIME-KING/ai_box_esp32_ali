list(APPEND srcs    fonts.c
                    convert.c
                    text.c)

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "."
                    REQUIRES esp_partition
                             spi_flash
                             fatfs
                             BSP)