cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(glitch_pio C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(pio_glitch)

# by default the header is generated into the build dir
pico_generate_pio_header(pio_glitch ${CMAKE_CURRENT_LIST_DIR}/glitch.pio)
# however, alternatively you can choose to generate it somewhere else (in this case in the source tree for check in)
#pico_generate_pio_header(pio_glitch ${CMAKE_CURRENT_LIST_DIR}/glitch.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR})

target_sources(pio_glitch PRIVATE glitch.c)

target_link_libraries(pio_glitch PRIVATE pico_stdlib hardware_pio)

# Pass cmake -DOUT_GPIO=x, where x is the pin you want to use
if(OUT_GPIO)
  target_compile_definitions(pio_glitch PRIVATE OUT_GPIO=${OUT_GPIO})
endif()

if(IN_GPIO)
  target_compile_definitions(pio_glitch PRIVATE IN_GPIO=${IN_GPIO})
endif()

if(SYSTEM_FREQ)
  target_compile_definitions(pio_glitch PRIVATE SYSTEM_FREQ=${SYSTEM_FREQ})
endif()

pico_add_extra_outputs(pio_glitch)
pico_enable_stdio_usb(pio_glitch 1)
pico_enable_stdio_uart(pio_glitch 0)