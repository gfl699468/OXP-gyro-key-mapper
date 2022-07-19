# Gyro & fn key mapper for onexplayer AMD Mini 5800U
This tool has been tested with holoiso, it mapping onexplayer AMD Mini 5800U's IMU(BMI260) to r-axis, add functions to fn keys(left/right bottom buttons).
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
