/*
 * sht31.h
 *  SHT31-D Sensor nagios plugin for Single Board Computers
 *  Copyright (c) 2017 Frostbyte <frostbytegr@gmail.com>
 *
 *  Check environment temperature and humidity with SHT31-D via I2C
 *
 *  Credits:
 *  <github.com/sfeakes> - adafruit sht31 for raspberry pi code
 *  <devel@nagios-plugins.org> - plugin development guidelines
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>

// Sensor definitions
#define SENSOR_ADDR	0x44
#define SENSOR_NA	130
#define SENSOR_TMP_MIN	-40
#define SENSOR_TMP_MAX	125
#define SENSOR_HUM_MIN	0
#define SENSOR_HUM_MAX	100

// Data structures
struct sensorOutput {
	float temperature, humidity;
};

// Function prototypes
int initializeSensor(char* devFile);
int executeAdditionalCommand(int sensor, char* command);
struct sensorOutput parseEnvData(int sensor);
