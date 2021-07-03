cmake_minimum_required(VERSION 3.19.5)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/CMake)
find_package(Tools MODULE)

# FIXME: The SDK doesn't seem to expose this to CMake, ugh...
add_compile_definitions(PICO_HEAP_SIZE=0x28000)

include(CMake/pico_sdk_import.cmake)

project(Pico C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

pico_sdk_init()

# FIXME: This is mostly necessary for the SDK
set(DISABLE_WARNINGS -Wno-unused-parameter -Wno-type-limits)
set(DISABLE_CXX_WARNINGS -Wno-reorder -Wno-ignored-qualifiers)

add_library(project_options INTERFACE)
target_compile_options(project_options INTERFACE -fdiagnostics-color=always -Wall ${DISABLE_WARNINGS} -Wextra -Werror -O0)
target_compile_options(project_options INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-frtti ${DISABLE_CXX_WARNINGS}>)
target_include_directories(project_options INTERFACE ${CMAKE_SOURCE_DIR})

add_subdirectory(Userland)

file(GLOB_RECURSE Kernel_SOURCES CONFIGURE_DEPENDS Kernel/*.cpp Kernel/*.S Std/*.cpp)

add_executable(Kernel.1 ${Kernel_SOURCES})
target_link_libraries(Kernel.1 pico_stdlib pico_bootrom hardware_dma project_options LibEmbeddedFiles)
target_compile_definitions(Kernel.1 PRIVATE KERNEL)
pico_add_extra_outputs(Kernel.1)

add_custom_target(Kernel.elf ALL
    COMMAND arm-none-eabi-objcopy
        --strip-symbol=__flash_data_2
        --strip-symbol=__flash_data_3
        --strip-symbol=__flash_data_4
        --strip-symbol=__flash_data_5
        --strip-symbol=__flash_base
    Kernel.1.elf Kernel.elf
    DEPENDS Kernel.1)
