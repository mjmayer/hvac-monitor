# HVAC-Monitor

## WiFi Setup

Create `src/secrets.h` file with the following contents

```cpp
#pragma once

const char *ssid = "myssid";
const char *password = "mywifipassword";
```

## Temperature Sensor Setup

Change the following lines in main.cpp to reflect the addresses of your temperature sensors.
If you do not know the addresses of the sensor, they will be printed when you first run
this program.

```cpp
DeviceAddress sensorBeforeCoil = {0x28, 0x58, 0x34, 0x50, 0x00, 0x00, 0x00, 0xD2};
DeviceAddress sensorAfterCoil = {0x28, 0x32, 0x5A, 0x51, 0x00, 0x00, 0x00, 0xE2};
DeviceAddress sensorAttic = {0x28, 0x0D, 0x70, 0x54, 0x00, 0x00, 0x00, 0xEF};
```
