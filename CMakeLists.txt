cmake_minimum_required(VERSION 3.8.1)
project(tcp_client_server)

find_package (Threads)

set(CMAKE_CXX_STANDARD 11)
# set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++11")
set(CMAKE_VERBOSE_MAKEFILE ON)

add_library(${PROJECT_NAME} SHARED 
        src/tcp_client.cpp
        src/tcp_server.cpp
        src/client.cpp
        src/pipe_ret_t.cpp
        src/common.cpp)

# Install the library
install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)

# Install the header files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include)

option(SERVER_EXAMPLE "Build SERVER" OFF)

if(SERVER_EXAMPLE)

    add_definitions(
            -DSERVER_EXAMPLE
    )

    add_executable(tcp_server examples/server_example.cpp)

    target_link_libraries (tcp_server ${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})

endif()

option(CLIENT_EXAMPLE "Build CLIENT" OFF)

if(CLIENT_EXAMPLE)

    add_definitions(
            -DCLIENT_EXAMPLE
    )

    add_executable(tcp_client examples/client_example.cpp)

    target_link_libraries (tcp_client ${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})

endif()
