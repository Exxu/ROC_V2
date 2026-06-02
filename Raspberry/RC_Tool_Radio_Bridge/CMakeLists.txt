cmake_minimum_required(VERSION 3.10)
project(roc_rc_tool_radio_bridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(roc_rc_tool_radio_bridge
  src/rc_tool_radio_bridge.cpp
  src/hil_frame.cpp
  src/serial_port.cpp
  src/mavlink_minimal.cpp
)

target_include_directories(roc_rc_tool_radio_bridge
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(roc_rc_tool_radio_bridge
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
