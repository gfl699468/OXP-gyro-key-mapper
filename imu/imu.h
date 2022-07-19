#include "GamepadMotion.hpp"
#include "bmi160/bmi160.h"
extern "C" {
    #include <linux/i2c-dev.h>
    #include <i2c/smbus.h>
}
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <math.h>
#include <algorithm>
#define GRAVITY_EARTH (9.80665f)
struct Velocity
{
    double yaw;
    double pitch;
};


class IMU
{
private:

    float g_ratio_table[13] = {0, 0, 0, 16384, 0, 8192, 0, 0, 2096, 0, 0, 0, 2048};
    float dps_ratio_table[5] = {16.4, 32.8, 65.6, 131.2, 262.4};
    double timestamp = 0;
    float gyro_x = 0;
    float gyro_y = 0;
    float gyro_z = 0;
    double delta = 0;

    // sensitivity
    float slow_factor = 1;
    float fast_factor = 2;
    float speed_min_thres = 0;
    float speed_max_thres = 75;
    bmi160_dev* sensor;
    GamepadMotion* filter;

    bmi160_offsets offsets;

    float g_ratio, dps_ratio;
public:
    IMU();
    Velocity getMotion();
    float getSensitivity();
    // std::vector<float> getOrient();
    ~IMU();
};

