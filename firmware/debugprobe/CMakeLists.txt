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

cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/freertos)
include(FreeRTOS_Kernel_import.cmake)

project(debugprobe)

pico_sdk_init()

if (${PICO_SDK_VERSION_MAJOR} LESS 2)
	message(SEND_ERROR "Version 2 of the Pico SDK is required to compile this project. Please update your installation at ${PICO_SDK_PATH}")
endif ()

add_executable(debugprobe
        src/probe_config.c
        src/main.c
        src/usb_descriptors.c
        src/probe.c
        src/cdc_uart.c
        src/get_serial.c
        src/sw_dp_pio.c
        src/tusb_edpt_handler.c
        src/status_display.c
        Fonts/font8.c
        Fonts/font12.c
        Fonts/font16.c
        Fonts/font20.c
        Fonts/font24.c
)

target_sources(debugprobe PRIVATE
        CMSIS_DAP/CMSIS/DAP/Firmware/Source/DAP.c
        CMSIS_DAP/CMSIS/DAP/Firmware/Source/JTAG_DP.c
        CMSIS_DAP/CMSIS/DAP/Firmware/Source/DAP_vendor.c
        CMSIS_DAP/CMSIS/DAP/Firmware/Source/SWO.c
        #CMSIS_DAP/CMSIS/DAP/Firmware/Source/SW_DP.c
        )

target_include_directories(debugprobe PRIVATE
        CMSIS_DAP/CMSIS/DAP/Firmware/Include/
        CMSIS_DAP/CMSIS/Core/Include/
        include/
    Fonts/
        )

target_compile_options(debugprobe PRIVATE -Wall)

pico_generate_pio_header(debugprobe ${CMAKE_CURRENT_LIST_DIR}/src/probe.pio)
pico_generate_pio_header(debugprobe ${CMAKE_CURRENT_LIST_DIR}/src/probe_oen.pio)

target_include_directories(debugprobe PRIVATE src)

target_compile_definitions (debugprobe PRIVATE
	PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1
)

option (DEBUG_ON_PICO "Compile firmware for the Pico instead of Debug Probe" 1)
if (DEBUG_ON_PICO)
    target_compile_definitions (debugprobe PRIVATE
	DEBUG_ON_PICO=1
    )
    # if (PICO_BOARD STREQUAL "pico")
    #     set_target_properties(debugprobe PROPERTIES
    #         OUTPUT_NAME "debugprobe_on_pico"
    #     )
    # elseif (PICO_BOARD STREQUAL "pico2")
    #     set_target_properties(debugprobe PROPERTIES
    #         OUTPUT_NAME "debugprobe_on_pico2"
    #     )
    # else ()
    #     message(SEND_ERROR "Unsupported board ${PICO_BOARD}")
    # endif ()
endif ()


target_link_libraries(debugprobe PRIVATE
        pico_multicore
        pico_stdlib
        pico_unique_id
        hardware_watchdog
        tinyusb_device
        tinyusb_board
        hardware_pio
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap1
        hardware_i2c
        hardware_pwm
)

# pico_set_binary_type(debugprobe copy_to_ram)

# 关键：指定自定义链接脚本
pico_set_linker_script(debugprobe ${CMAKE_CURRENT_SOURCE_DIR}/memmap_app_b.ld)

# 禁止 APP 再次初始化 Flash XIP，防止冲突
target_compile_definitions(debugprobe PRIVATE PICO_NO_FLASH_BANK_SELECT=1)
target_compile_definitions(debugprobe PRIVATE PICO_COPY_TO_RAM=0) 

pico_add_extra_outputs(debugprobe)
