cmake_minimum_required(VERSION 3.5)
project(hil_communication_standalone_timed_v4)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Force the classic FindBoost module. This avoids requiring BoostConfig
# packages such as boost_systemConfig.cmake on some Ubuntu/CMake setups.
set(Boost_NO_BOOST_CMAKE ON)
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
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(hil_communication
  PUBLIC
    ${Boost_LIBRARIES}
    Threads::Threads
)

add_executable(keyboard_tool_controller_timed
  src/keyboard_tool_main.cpp
)

target_link_libraries(keyboard_tool_controller_timed
  PRIVATE
    hil_communication
)
