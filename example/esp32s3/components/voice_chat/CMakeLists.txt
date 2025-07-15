set (APPLIBS_REQUIRES mbedtls)
idf_component_register(SRCS ""
                       INCLUDE_DIRS "include"
                       REQUIRES ${APPLIBS_REQUIRES})
add_prebuilt_library(applibs_source "${CMAKE_CURRENT_LIST_DIR}/libconversation.a"
                     REQUIRES ${APPLIBS_REQUIRES})
                     target_link_libraries(${COMPONENT_LIB} INTERFACE applibs_source)