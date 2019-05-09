cmake_minimum_required(VERSION 2.8)
project(lowdis)

set(CMAKE_C_FLAGS "${CMAKE_FLAGS} -Wall -g")

include_directories(src)

aux_source_directory(src SRC_LIST)

add_library(lowdis ${SRC_LIST})

add_executable(sock-svr-thread ./samples/sock-svr-thread.c)
target_link_libraries(sock-svr-thread lowdis)

add_executable(sock-svr-reactor ./samples/sock-svr-reactor.c)
target_link_libraries(sock-svr-reactor lowdis)

add_executable(threadpool ./samples/threadpool.c)
target_link_libraries(threadpool lowdis)

add_executable(sock-cli ./samples/sock-cli.c)
target_link_libraries(sock-cli lowdis)

add_executable(snip ./samples/snip.c)
target_link_libraries(snip lowdis)
