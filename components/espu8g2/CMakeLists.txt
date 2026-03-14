# Collect all u8g2 C source files from the csrc subdirectory
file(GLOB U8G2_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/csrc/*.c")

idf_component_register(
    SRCS
        "u8g2_esp32_hal.c"
        ${U8G2_SRCS}
    INCLUDE_DIRS
        "include"
        "csrc"
    REQUIRES
        driver
        esp_timer
        log
        freertos
)
