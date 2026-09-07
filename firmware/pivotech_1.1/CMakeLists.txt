# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -finput-charset=UTF-8 -fexec-charset=UTF-8") #忽略所有警告
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w -finput-charset=UTF-8 -fexec-charset=UTF-8") #忽略所有警告

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(1.4.2 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(1.4.2
        main.cpp
        CONFIG_FLO.cpp
        menu/core.cpp
        menu/flash.cpp
        menu/license_crypto.cpp
        menu/UI.cpp
        read/functions.cpp
        read/generalread.cpp
        read/pwmoutput.cpp
        read/encoderread.cpp
        read/adcread.cpp
        read/hardware_core1.cpp
        read/serialread.cpp
        read/uartconfig.cpp
        read/select.cpp
        read/aitest.cpp
        read/mp_runtime.cpp
        read/i2ctest.cpp
        read/spitest.cpp
        read/mod_selection.cpp
        read/hc05.cpp
        read/adcxy.cpp
        read/ssd1306_demo.cpp
        read/mpu6050_demo.cpp
        menu/StateLED.cpp
        menu/ssd1306.cpp
        menu/SGateSetting.cpp
        menu/send.cpp
        menu/pio.cpp
        menu/menu.cpp
        menu/load.cpp
        menu/key.cpp
        menu/Curve.cpp
        Fonts/font8.c
        Fonts/font12.c
        Fonts/font16.c
        Fonts/font20.c
        Fonts/font24.c
        Fonts/font12cn.c)

pico_set_program_name(1.4.2 "1.4.2")
pico_set_program_version(1.4.2 "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(1.4.2 1)
pico_enable_stdio_usb(1.4.2 1)

# Add the standard library to the build
target_link_libraries(1.4.2
        pico_stdlib
        hardware_spi
        hardware_pwm
        hardware_adc
        hardware_irq
        hardware_i2c
        hardware_pio
        hardware_timer
        hardware_flash
        hardware_sync
        pico_multicore
        pico_mbedtls
        )

# Add the standard include files to the build
target_include_directories(1.4.2 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)


target_compile_definitions(1.4.2 PRIVATE PICO_STDIO_USB_CUSTOM_DESCRIPTORS=1) # 告诉 pico_stdio_usb 不要包含默认描述符
target_link_options(1.4.2 PRIVATE "-Wl,--allow-multiple-definition")


# 如果ASM文件报错 将下面两组Cmake配置注释掉编译 然后再取消注释编译就会通过

# 关键：指定自定义链接脚本
pico_set_linker_script(1.4.2 ${CMAKE_CURRENT_SOURCE_DIR}/memmap_app_b.ld)

# 禁止 APP 再次初始化 Flash XIP，防止冲突
target_compile_definitions(1.4.2 PRIVATE PICO_NO_FLASH_BANK_SELECT=1)
target_compile_definitions(1.4.2 PRIVATE PICO_COPY_TO_RAM=0) # 禁止 APP 复制到 RAM

pico_add_extra_outputs(1.4.2)
