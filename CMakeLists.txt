cmake_minimum_required(VERSION 3.10)
project(mini-tmux)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_definitions(_DEFAULT_SOURCE)

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

set(SOURCES main.c client.c spawn.c util.c server.c log.c window.c render.c input.c)

add_executable(mini-tmux ${SOURCES} ${VTERM_SOURCES})
target_include_directories(mini-tmux PRIVATE vendor/vterm)
