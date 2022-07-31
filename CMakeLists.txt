cmake_minimum_required(VERSION 3.0.0)
project(oxp_gyro_key_mapper VERSION 0.1.0)
set (CMAKE_CXX_STANDARD 20)
add_subdirectory(imu)
include_directories(imu)
link_directories(imu)

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET glib-2.0)

add_executable(oxp_gyro_key_mapper main.cpp uinput.cpp uinput.hpp)
target_link_libraries(oxp_gyro_key_mapper imu_lib PkgConfig::deps evdev pthread)
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
