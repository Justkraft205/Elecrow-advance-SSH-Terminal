FILE(GLOB_RECURSE main ${CMAKE_SOURCE_DIR}/main/*.c)

idf_component_register(
    SRCS ${main}
    INCLUDE_DIRS "."
    REQUIRES
        ch405labs_esp_libssh2
        vfs
        bsp_illuminate
        bsp_i2c
        bsp_display
        lvgl
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        spiffs
)
