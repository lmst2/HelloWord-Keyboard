set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
cmake_minimum_required(VERSION 3.19)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(SIZE arm-none-eabi-size)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(HelloWord-Keyboard-bootloader C ASM)
set(CMAKE_C_STANDARD 11)

add_compile_options(-mcpu=cortex-m3 -mthumb -mthumb-interwork)
add_compile_options(-ffunction-sections -fdata-sections -fno-common -fmessage-length=0)
add_compile_options(-Os -g)

add_compile_options($<$<COMPILE_LANGUAGE:ASM>:-x$<SEMICOLON>assembler-with-cpp>)

# Reuse CMSIS and device headers from the keyboard firmware project
set(KBD_FW_DIR ${CMAKE_SOURCE_DIR}/../HelloWord-Keyboard-fw)

include_directories(
    ${KBD_FW_DIR}/Drivers/CMSIS/Include
    ${KBD_FW_DIR}/Drivers/CMSIS/Device/ST/STM32F1xx/Include
    ${KBD_FW_DIR}/Core/Inc
)

add_definitions(-DSTM32F103xB)

set(SOURCES
    bootloader.c
    usb_hid.c
    ${KBD_FW_DIR}/startup/startup_stm32f103xb.s
)

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/STM32F103CBTx_BOOTLOADER.ld)

add_link_options(-Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)
add_link_options(-mcpu=cortex-m3 -mthumb -mthumb-interwork)
add_link_options(-T ${LINKER_SCRIPT})
add_link_options(-specs=nosys.specs -specs=nano.specs)

add_executable(${PROJECT_NAME}.elf ${SOURCES} ${LINKER_SCRIPT})

set(HEX_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex)
set(BIN_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${HEX_FILE}
    COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_FILE}
    COMMAND ${SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Building ${HEX_FILE}\nBuilding ${BIN_FILE}")
