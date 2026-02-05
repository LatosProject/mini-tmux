cmake_minimum_required(VERSION 3.10)
project(mini-tmux VERSION 0.4.1)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_definitions(_DEFAULT_SOURCE)

# 检测目标架构
if(NOT DEFINED TARGET_ARCH)
    execute_process(COMMAND uname -m OUTPUT_VARIABLE TARGET_ARCH OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
# Debug 构建时启用日志
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DENABLE_LOG)
endif()

# 架构名称映射
if(TARGET_ARCH STREQUAL "x86_64")
    set(ARCH_NAME "amd64")
elseif(TARGET_ARCH STREQUAL "aarch64")
    set(ARCH_NAME "arm64")
else()
    set(ARCH_NAME ${TARGET_ARCH})
endif()

message(STATUS "Building for architecture: ${ARCH_NAME}")
message(STATUS "Version: ${PROJECT_VERSION}")

# libvterm sources
set(VTERM_SOURCES
    vendor/vterm/vterm.c
    vendor/vterm/encoding.c
    vendor/vterm/keyboard.c
    vendor/vterm/mouse.c
    vendor/vterm/parser.c
    vendor/vterm/pen.c
    vendor/vterm/screen.c
    vendor/vterm/state.c
    vendor/vterm/unicode.c
)

set(SOURCES main.c client.c spawn.c util.c server.c log.c window.c render.c input.c keyboard.c)

# 设置输出文件名：mini-tmux-版本-架构[-debug]
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(OUTPUT_NAME "mini-tmux-${PROJECT_VERSION}-${ARCH_NAME}-debug")
else()
    set(OUTPUT_NAME "mini-tmux-${PROJECT_VERSION}-${ARCH_NAME}")
endif()
add_executable(${OUTPUT_NAME} ${SOURCES} ${VTERM_SOURCES})
target_include_directories(${OUTPUT_NAME} PRIVATE vendor/vterm)

# 同时创建一个不带版本的符号链接（可选）
add_custom_command(TARGET ${OUTPUT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${OUTPUT_NAME} mini-tmux
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Creating symlink: mini-tmux -> ${OUTPUT_NAME}"
)
