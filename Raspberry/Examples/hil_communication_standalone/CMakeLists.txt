cmake_minimum_required(VERSION 3.10)
project(hil_communication_standalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)

add_library(hil_communication
  src/frame.cpp
  src/serial.cpp
  src/udp.cpp
  src/hil_serial.cpp
  src/hil_udp.cpp
)

target_include_directories(hil_communication
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(hil_communication
  PUBLIC
    Boost::system
    Boost::thread
    Threads::Threads
)

add_executable(hil_serial_test
  examples/main.cpp
)

target_link_libraries(hil_serial_test
  PRIVATE
    hil_communication
)
