cmake_minimum_required(VERSION 3.15)

project(ffirpc)

set(SRC_DIR src)
set(FFIRPC_BIN ${PROJECT_NAME})

include_directories(include)

set(SRC
    ${SRC_DIR}/ptracker.c
    ${SRC_DIR}/C-Thread-Pool/thpool.c
    ${SRC_DIR}/rpc_init.c
    ${SRC_DIR}/poll_network.c
    ${SRC_DIR}/rpc_server.c
    ${SRC_DIR}/rpc_network.c
    ${SRC_DIR}/rpc_object.c
    ${SRC_DIR}/rpc_sizedbuf.c 
    ${SRC_DIR}/rpc_function.c
    ${SRC_DIR}/rpc_struct.c
    ${SRC_DIR}/rpc_thread_context.c
    ${SRC_DIR}/rpc_client.c
    ${SRC_DIR}/hashtable.c
)

include_directories(${PROJECT_SOURCE_DIR})

add_library(${PROJECT_NAME} STATIC  ${SRC})

target_compile_options(${PROJECT_NAME} PRIVATE -fno-omit-frame-pointer -fsanitize=address,undefined)
target_link_options(${PROJECT_NAME} PRIVATE -fno-omit-frame-pointer -fsanitize=address,undefined)

add_subdirectory(tests)
