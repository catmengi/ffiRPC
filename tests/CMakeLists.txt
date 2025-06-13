cmake_minimum_required(VERSION 3.15)

project(donkey)


add_executable(${PROJECT_NAME} test.c)

target_link_libraries(${PROJECT_NAME} PUBLIC ${FFIRPC_BIN} ffi)
target_link_libraries(${PROJECT_NAME} PUBLIC ${FFIRPC_BIN} jansson)

