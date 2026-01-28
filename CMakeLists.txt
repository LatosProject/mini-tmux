cmake_minimum_required(VERSION 3.10)
project(mini-tmux)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_definitions(_DEFAULT_SOURCE)
set(SOURCES main.c client.c spawn.c util.c server.c log.c window.c render.c input.c)

find_package(PkgConfig REQUIRED)
pkg_check_modules(VTERM REQUIRED vterm)

add_executable(mini-tmux ${SOURCES})
target_include_directories(mini-tmux PRIVATE ${VTERM_INCLUDE_DIRS})
target_link_directories(mini-tmux PRIVATE ${VTERM_LIBRARY_DIRS})
target_link_libraries(mini-tmux ${VTERM_LIBRARIES})