cmake_minimum_required(VERSION 3.10)
project(mini-tmux)
set(SOURCES main.c client.c spawn.c util.c)

add_executable(mini-tmux ${SOURCES})