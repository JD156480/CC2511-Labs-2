cmake_minimum_required(VERSION 3.12)
include(pico_sdk_import.cmake)

set(projname "DevBoardLab")

project(${projname} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

#include(example_auto_set_url.cmake)

add_executable(${projname}
        main.c
        )

target_link_libraries(${projname}
    pico_stdlib
    hardware_uart
    hardware_pwm
    hardware_adc
)

pico_enable_stdio_uart(${projname} 1)
pico_enable_stdio_usb(${projname} 0)

pico_add_extra_outputs(${projname})