#include "imu.h"

float lsb_to_ms2(int16_t val, float g_range, uint8_t bit_width)
{
    float half_scale = (float)(1 << bit_width) / 2.0f;

    return GRAVITY_EARTH * val * g_range / half_scale;
}
float lsb_to_mg(int16_t val, float g_range, uint8_t bit_width)
{
    float half_scale = (float)(1 << bit_width) / 2.0f;

    return 1000.0f * val * g_range / half_scale;
}

float lsb_to_dps(int16_t val, float g_range, uint8_t bit_width)
{
    float half_scale = (float)(1 << bit_width) / 2.0f;

    return val * g_range / half_scale;
}

void read_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    // std::cout << "read dev_addr " << std::hex << +dev_addr << " reg_addr " << std::hex << +reg_addr << " len " << len << std::endl;
    char filename[40];
    sprintf(filename, "/dev/i2c-1");
    auto file = open(filename, O_RDWR);
    ioctl(file, I2C_SLAVE, dev_addr);
    signed int raw_data = 0;
    for (size_t i = 0; i < len; i++)
    {
        raw_data = i2c_smbus_read_byte_data(file, reg_addr + i);
        data[i] = (uint8_t)raw_data;
        // std::cout << "read " << i << " data: " << std::bitset<8>(+raw_data) << std::endl;
    }
    close(file);
};

void write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *read_data, uint16_t len)
{
    // std::cout << "write reg_addr " << std::hex << +reg_addr << " data " << std::bitset<8>(*read_data) << " len " << len << std::endl;
    char filename[40];
    sprintf(filename, "/dev/i2c-1");
    auto file = open(filename, O_RDWR);
    ioctl(file, I2C_SLAVE, dev_addr);
    for (size_t i = 0; i < len; i++)
    {
        i2c_smbus_write_byte_data(file, reg_addr + i, read_data[i]);
    }

    close(file);
};

void delay_ms(uint32_t period)
{
    // sleep(0.01*period);
    usleep(1000 * period);
}

IMU::IMU()
{
    sensor = new bmi160_dev();
    filter = new GamepadMotion();

    // init IMU
    sensor->id = 0x68;
    sensor->intf = BMI160_I2C_INTF;
    sensor->read = (bmi160_read_fptr_t)read_reg;
    sensor->write = (bmi160_write_fptr_t)write_reg;
    sensor->delay_ms = (bmi160_delay_fptr_t)delay_ms;
    auto ret = bmi160_init(sensor);

    // power on
    sensor->accel_cfg.power = BMI160_ACCEL_NORMAL_MODE;
    sensor->gyro_cfg.power = BMI160_GYRO_NORMAL_MODE;
    bmi160_set_power_mode(sensor);
    sleep(1);

    bmi160_get_power_mode(sensor);
    
    // Fast offset compensation
    bmi160_foc_conf conf;
    conf.acc_off_en = BMI160_ENABLE;
    conf.gyro_off_en = BMI160_ENABLE;
    conf.foc_acc_x = BMI160_FOC_ACCEL_NEGATIVE_G;
    conf.foc_acc_y = BMI160_FOC_ACCEL_0G;
    conf.foc_acc_x = BMI160_FOC_ACCEL_0G;
    conf.foc_gyr_en = BMI160_ENABLE;

    auto foc_status = bmi160_start_foc(&conf, &offsets, sensor);

    bmi160_set_offsets(&conf, &offsets, sensor);

    // g_ratio = g_ratio_table[sensor->accel_cfg.range];
    // dps_ratio = dps_ratio_table[sensor->gyro_cfg.range];

    // std::cout << "g ratio: " << g_ratio
    //           << " dps ratio: " << dps_ratio << std::endl;

    // set device
    // /* Select the Output data rate, range of accelerometer sensor */
    // sensor->accel_cfg.odr = BMI160_ACCEL_ODR_1600HZ;
    // sensor->accel_cfg.range = BMI160_ACCEL_RANGE_16G;
    // sensor->accel_cfg.bw = BMI160_ACCEL_BW_NORMAL_AVG4;

    // /* Select the power mode of accelerometer sensor */
    // sensor->accel_cfg.power = BMI160_ACCEL_NORMAL_MODE;

    // /* Select the Output data rate, range of Gyroscope sensor */
    // sensor->gyro_cfg.odr = BMI160_GYRO_ODR_3200HZ;
    // sensor->gyro_cfg.range = BMI160_GYRO_RANGE_2000_DPS;
    // sensor->gyro_cfg.bw = BMI160_GYRO_BW_NORMAL_MODE;

    // /* Select the power mode of Gyroscope sensor */
    // sensor->gyro_cfg.power = BMI160_GYRO_NORMAL_MODE;
    // /* Set the sensor configuration */
    // auto rslt = bmi160_set_sens_conf(sensor);
    // std::cout << "set conf ret: " << +rslt << std::endl;
    g_ratio = g_ratio_table[sensor->accel_cfg.range];
    dps_ratio = dps_ratio_table[sensor->gyro_cfg.range];

    std::cout << "g ratio: " << g_ratio
              << " dps ratio: " << dps_ratio << std::endl;
};

IMU::~IMU(){};

Velocity IMU::getMotion()
{
    bmi160_sensor_data *tmp_acc = new bmi160_sensor_data();
    bmi160_sensor_data *tmp_gyro = new bmi160_sensor_data();
    auto ret = bmi160_get_sensor_data(BMI160_BOTH_ACCEL_AND_GYRO_WITH_TIME, tmp_acc, tmp_gyro, sensor);
    if (timestamp == 0)
    {
        timestamp = tmp_gyro->sensortime * 39 / 1e6;
        filter->Reset();
        filter->SetCalibrationMode(
            GamepadMotionHelpers::CalibrationMode::Stillness |
            GamepadMotionHelpers::CalibrationMode::SensorFusion);
    }
    else
    {
        auto new_timestamp = tmp_gyro->sensortime * 39 / 1e6;
        delta = new_timestamp - timestamp;
        filter->ProcessMotion(tmp_gyro->x / dps_ratio,
                              tmp_gyro->y / dps_ratio,
                              tmp_gyro->z / dps_ratio,
                              tmp_acc->x / g_ratio,
                              tmp_acc->y / g_ratio,
                              tmp_acc->z / g_ratio,
                              delta);
        
        filter->GetCalibratedGyro(gyro_x, gyro_y, gyro_z);
        timestamp = new_timestamp;

    }
    auto sensitivity = getSensitivity();
    auto x = delta * sensitivity * gyro_x;
    auto y = delta * sensitivity * gyro_y;
    auto z = delta * sensitivity * gyro_z;

    // printf("delta: %7.2f gyro: %7.2f %7.2f %7.2f\n", delta, x, y, z);
    
    return Velocity{x,y};
};

float IMU::getSensitivity()
{
    auto speed = sqrt(gyro_x * gyro_x + gyro_y * gyro_y);
    auto slow_fast_factor = (speed - speed_min_thres) /
      (speed_max_thres - speed_min_thres);
    slow_fast_factor = std::clamp(slow_fast_factor, 0.0f, 1.0f);
    auto sensitivity = slow_factor * (1.0 - slow_fast_factor) + fast_factor * slow_fast_factor;
    return sensitivity;
}