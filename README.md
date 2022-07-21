# Gyro & fn key mapper for onexplayer AMD Mini 5800U
This tool has been tested with holoiso, it can map IMU(BMI260)'s angular velocity to r-axis, add functions to fn keys(left/right bottom buttons).

Demo:https://youtu.be/gI9XL1J_pYE

## Installation
- Block BMI160 related kernel modules by adding lines to /etc/modprobe.d/blacklist.conf:
```
blacklist bmi160_i2c
blacklist bmi160_spi
blacklist bmi160_core
```
- Auto-load i2c kernel module by create one-line file /etc/modules-load.d/i2c.conf with content `i2c_dev`
- Clone & complie
```
mkdir build
cd build
cmake ..
make
```
- Create service(or ssh/screen/nohup...) so that you can use it in game mode

## Functions
| FN key              | action       | function   |
|:-------------------:|:------------:|:----------:|
| left bottom button  | single click | steam menu |
| left bottom button  | double click | gyro switch|
| right bottom button<br />(not night mode btn) | single click | quick menu|
| right bottom button<br />(not night mode btn) | double click | on-screen keyboard|

## Limits
- Since press the night mode button doesn't send any event, it can not be mapped
- The events only been sent when you release the fn buttons, no events when you press them, so no long press actions
- Rumble not support yet.

## Credit
- [GamepadMotionHelpers](https://github.com/JibbSmart/GamepadMotionHelpers)
- [BMI160_driver](https://github.com/BoschSensortec/BMI160_driver)

