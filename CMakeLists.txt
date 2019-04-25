cmake_minimum_required(VERSION 2.8)
project(lowdis)

set(CMAKE_C_FLAGS "${CMAKE_FLAGS} -Wall -g")

include_directories(src)

aux_source_directory(src SRC_LIST)

add_library(lowdis ${SRC_LIST})

add_executable(sock-svr ./samples/sock-svr.c)
target_link_libraries(sock-svr lowdis)

add_executable(threadpool ./samples/threadpool.c)
target_link_libraries(threadpool lowdis)

add_executable(sock-cli ./samples/sock-cli.c)
target_link_libraries(sock-cli lowdis)

add_executable(snip ./samples/snip.c)
target_link_libraries(snip lowdis)
