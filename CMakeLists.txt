cmake_minimum_required(VERSION 3.15)

project(FFIRPC)

set(SRC_LIBS_DIR libs)
set(SRC_DIR src)
set(FFIRPC_BIN ${PROJECT_NAME})

add_subdirectory(${SRC_LIBS_DIR}/hashtable)

include_directories(include)
include_directories(${SRC_LIBS_DIR}/hashtable/include)

set(SRC 
    ${SRC_DIR}/rpc_sizedbuf.c 
    ${SRC_DIR}/rpc_struct.c
    ${SRC_DIR}/rpc_thread_context.c
)

include_directories(${PROJECT_SOURCE_DIR})

add_library(${PROJECT_NAME} STATIC  ${SRC} ${HASHTABLE_BIN})

add_subdirectory(tests)