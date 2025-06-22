cmake_minimum_required(VERSION 3.15)

project(donkey)


add_executable(${PROJECT_NAME} test.c)

target_link_libraries(${PROJECT_NAME} PUBLIC ${FFIRPC_BIN} ffi)
target_link_libraries(${PROJECT_NAME} PUBLIC ${FFIRPC_BIN} jansson)

target_compile_options(${PROJECT_NAME} PRIVATE -fno-omit-frame-pointer -fsanitize=address,undefined)
target_link_options(${PROJECT_NAME} PRIVATE -fno-omit-frame-pointer -fsanitize=address,undefined)

