project(test_socket_ops)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} ${ZeroMQ_LIBRARIES})

add_catch_test(${PROJECT_NAME})
