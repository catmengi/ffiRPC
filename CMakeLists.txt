cmake_minimum_required(VERSION 3.15)

project(ffirpc)

set(SRC_DIR src)
set(FFIRPC_BIN ${PROJECT_NAME})

include_directories(include)

set(SRC
    ${SRC_DIR}/rpc_server.c
    ${SRC_DIR}/rpc_sizedbuf.c 
    ${SRC_DIR}/rpc_struct.c
    ${SRC_DIR}/rpc_thread_context.c
    ${SRC_DIR}/rpc_protocol.c
    ${SRC_DIR}/hashtable.c
)

include_directories(${PROJECT_SOURCE_DIR})

add_library(${PROJECT_NAME} STATIC  ${SRC})


add_subdirectory(tests)
