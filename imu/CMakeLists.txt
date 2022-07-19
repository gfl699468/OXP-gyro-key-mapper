include_directories(bmi160)
link_directories(bmi160)
file(GLOB SRC "bmi160/*")
add_library(bmi160 SHARED ${SRC})
add_library(imu_lib SHARED imu.cpp imu.h)

target_link_libraries(imu_lib bmi160 i2c)
