cmake_minimum_required(VERSION 2.8)
project(lowdis)

set(CMAKE_C_FLAGS "${CMAKE_FLAGS} -Wall -g")

include_directories(src)

aux_source_directory(src SRC_LIST)

add_executable(lowdis ${SRC_LIST})

add_executable(threadpool ./samples/threadpool.c ./src/threadpool.c ./src/logger.c)

add_executable(sock-client ./samples/sock.c)

add_executable(snip ./samples/snip.c)
