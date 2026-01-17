cmake_minimum_required(VERSION 3.10)
project(mini-tmux)
set(SOURCES main.c client.c spawn.c util.c server.c log.c)

add_executable(mini-tmux ${SOURCES})