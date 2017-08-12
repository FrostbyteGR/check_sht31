/*
 * nagioshelper.h
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

// Sensor library
#include "sht31.h"

// Data structures
struct thresholdRange {
	int min, max;
};

struct threshold {
	struct thresholdRange temperature, humidity;
};

struct execParameters {
	char *bus, *cmd;
	struct threshold warn, crit;
};

// Function prototypes
struct execParameters parseParameters(int argc, char *argv[]);
int outputResults(struct execParameters params, struct sensorOutput output);
