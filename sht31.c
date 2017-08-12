/*
 * sht31.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

// Sensor library
#include "sht31.h"

// Sensor command definitions
#define SHT31_ENV_READ		0x240B
#define SHT31_SOFTRESET		0x30A2
#define SHT31_SERIAL_READ	0x3780
#define SHT31_STATUS_READ	0xF32D
#define SHT31_STATUS_CLEAR	0x3041
#define SHT31_HEATER_ENABLE	0x306D
#define SHT31_HEATER_DISABLE	0x3066

// Sensor definitions
#define QUERYRETRIES 3

// Error code definitions
#define ERRCODE_CRC		0
#define ERRCODE_IOCTL		1
#define ERRCODE_CMD_SEND	2
#define ERRCODE_CMD_RECV	3
#define ERRCODE_CMD_UNKNOWN	4

// Boolean definitions
#ifndef	TRUE
#	define	TRUE	(1==1)
#	define	FALSE	(!TRUE)
#endif

// General purpose delay function
static void delay (unsigned int milliSeconds) {
	nanosleep((struct timespec[]){{milliSeconds/1000,(milliSeconds%1000)*1000000}},NULL);
}

// Error handling function
static void throwSensorError(int sensor, int errorCode) {
	// Close the sensor device file descriptor
	close(sensor);

	// Decide on which error to display
	switch(errorCode) {
		case ERRCODE_CRC:
			fprintf(stderr,"The response received by the sensor was corrupted.\n");
			break;
		case ERRCODE_IOCTL:
			fprintf(stderr,"Could not initialize the I2C bus.\n");
			break;
		case ERRCODE_CMD_SEND:
			fprintf(stderr,"Command could not be sent to the sensor.\n");
			break;
		case ERRCODE_CMD_RECV:
			fprintf(stderr,"Response could not be read from the sensor.\n");
			break;
		case ERRCODE_CMD_UNKNOWN:
			fprintf(stderr,"Unknown sensor command.\n" \
			"Available commands: getEnvData, getSerial, getStatus, clearStatus, softReset, heaterEnable, heaterDisable\n");
	}

	// Flush and exit
	fflush(stderr);
	exit (EXIT_FAILURE);
}

// Function to send a command to the sensor and receive a response if applicable
static void sendCommand(int sensor, uint16_t command, uint8_t *result, int readBytes) {
	// Split the 16-bit command into a pair of flipped 8-bit values
	uint8_t commandBytes[]={(command>>8)&0xFF, command&0xFF};

	// If writing the command to the bus fails
	if (write(sensor, commandBytes, sizeof(commandBytes))!=sizeof(commandBytes)) {
		// Throw the corresponding error
		throwSensorError(sensor, ERRCODE_CMD_SEND);
	}

	// If a response is expected
	if (readBytes>0) {
		// Wait for 10ms
		delay(10);

		// If reading the result from the bus fails
		if (read(sensor, result, readBytes)<readBytes) {
			// Throw the corresponding error
			throwSensorError(sensor, ERRCODE_CMD_RECV);
		}
	}
}

// Function to verify the CRC, taken from page 14 of SHT31 spec pdf
// TODO: Comments with explanation
static uint8_t verifyCRC(const uint8_t *inputData, int dataLength) {
	const uint8_t polynomial=0x31;
	uint8_t result=0xFF;

	for (int length=dataLength; length; --length) {
		result^=*inputData++;

		for (int bits=8; bits; --bits) {
			result=(result&0x80)?(result<<1)^polynomial:(result<<1);
		}
	}

	return result;
}

// Function to retrieve the environment data from the sensor
static int retrieveEnvData(int sensor, uint8_t *result, int resultSize) {
	// Send the corresponding command to the sensor
	sendCommand(sensor, SHT31_ENV_READ, result, resultSize);

	// Return the CRC validation result of the query
	return (result[2]==verifyCRC(result, 2) && result[5]==verifyCRC(result+3, 2));
}

// Function to initialize the sensor parameters
int initializeSensor(char* devFile) {
	int result;

	// If opening the device file fails
	if ((result=open(devFile, O_RDWR))<0) {
		// Throw the corresponding error, flush and exit
		fprintf(stderr, "Could not open device file: %s\n", devFile);
		fflush(stderr);
		exit(EXIT_FAILURE);
	}

	// If initializing the I2C bus fails
	if (ioctl(result, I2C_SLAVE, SENSOR_ADDR)<0) {
		// Throw the corresponding error
		throwSensorError(result, ERRCODE_IOCTL);
	}

	// Return the file descriptor pointing to the sensor
	return result;
}

// Function to execute additional sensor commands
int executeAdditionalCommand(int sensor, char* command) {
	// Define the available commands and initialize the command index
	char additionalCommands[][15]={"getSerial","getStatus","clearStatus","softReset","heaterEnable","heaterDisable"};
	int commandIndex=-1;

	// Iterate through all of the available commands
	for (int index=0; index<sizeof(additionalCommands)/sizeof(additionalCommands[0]); index++) {
		// If a (case-insensitive) match was found
		if (strcasecmp(command, additionalCommands[index])==0) {
			// Assign the command index
			commandIndex=index;
		}
	}

	// Decide on which command to execute
	switch(commandIndex) {
		// getSerial
		case 0: {
			uint8_t sensorData[6];

			// Send the command to the sensor
			sendCommand(sensor, SHT31_SERIAL_READ, sensorData, sizeof(sensorData));

			// If the CRC validation succeeds
			if (sensorData[2]==verifyCRC(sensorData, 2) && sensorData[5]==verifyCRC(sensorData+3,2)) {
				// Report back with the sensor's serial number information
				fprintf(stdout,"S/N: 0x%x\n", ((uint32_t)sensorData[0]<<24)|((uint32_t)sensorData[1]<<16)|((uint32_t)sensorData[3]<<8)|sensorData[4]);
			} else {
				// Throw the corresponding error
				throwSensorError(sensor, ERRCODE_CRC);
			}

			break;
		}

		// getStatus
		case 1:	{
			uint8_t sensorData[3];
			uint16_t result=0x0000;

			// Send the command to the sensor
			sendCommand(sensor, SHT31_STATUS_READ, sensorData, sizeof(sensorData));

			// If the CRC validation succeeds
			if (sensorData[2]==verifyCRC(sensorData, 2)) {
				// Combine the pair of 8-bit received data to a 16-bit piece
				result=((uint16_t)sensorData[0]<<8)|sensorData[1];
			} else {
				// Throw the corresponding error
				throwSensorError(sensor, ERRCODE_CRC);
			}

			// Report back with the sensor's status information
			fprintf(stdout,"Sensor status:\n" \
					"Checksum:\t\t%d\n" \
					"Last Command:\t\t%d\n" \
					"Reset Detected:\t\t%d\n" \
					"Tmp Tracking Alert:\t%d\n" \
					"Hum Tracking Alert:\t%d\n" \
					"Heater:\t\t\t%d\n" \
					"Alert Pending:\t\t%d\n", \
					result&1, result>>1&1, result>>4&1, result>>10&1, result>>11&1, result>>13&1, result>>15&1);
			break;
		}

		// clearStatus
		case 2:
			// Send the command to the sensor and report back
			sendCommand(sensor, SHT31_STATUS_CLEAR, NULL, 0);
			fprintf(stdout,"Sensor status cleared.\n");
			break;

		// softReset
		case 3:
			// Send the command to the sensor and report back
			sendCommand(sensor, SHT31_SOFTRESET, NULL, 0);
			fprintf(stdout,"Sensor was soft reset.\n");
			break;

		// heaterEnable
		case 4:
			// Send the command to the sensor and report back
			sendCommand(sensor, SHT31_HEATER_ENABLE, NULL, 0);
			fprintf(stdout,"Sensor heater unit enabled.\n");
			break;

		// heaterDisable
		case 5:
			// Send the command to the sensor and report back
			sendCommand(sensor, SHT31_HEATER_DISABLE, NULL, 0);
			fprintf(stdout,"Sensor heater unit disabled.\n");
			break;

		// Invalid command
		default:
			// Throw the corresponding error
			throwSensorError(sensor, ERRCODE_CMD_UNKNOWN);
	}

	// Close the sensor device file descriptor
	close(sensor);

	// Exit
	return 0;
}

// Function to parse the environment data queried by the sensor
struct sensorOutput parseEnvData(int sensor) {
	struct sensorOutput result;
	uint8_t sensorData[6];

	// Set the sensor query retry count
	int queryRetries=QUERYRETRIES+1;

	// While there are still retries remaining
	while (queryRetries--) {
		// Clean up any residual sensor data from previous attempts
		memset(sensorData, 0, sizeof(sensorData));

		// If the environment data retrieval was sucessful
		if (retrieveEnvData(sensor, sensorData, sizeof(sensorData))) {
			// Convert the temperature and humidity data into human readable format (°C and % RH)
			result.temperature=-45+(175*((float)(((uint16_t)sensorData[0]<<8)|sensorData[1])/(float)0xFFFF));
			result.humidity=100*((float)(((uint16_t)sensorData[3]<<8)|sensorData[4])/(float)0xFFFF);
		}

		// If the retrieved environment data are within the sensor's documented capabilities
		if (result.temperature>=SENSOR_TMP_MIN && result.temperature<=SENSOR_TMP_MAX && result.humidity>=SENSOR_HUM_MIN && result.humidity<=SENSOR_HUM_MAX) {
			// Close the sensor device file descriptor
			close(sensor);

			// Return the processed output
			return result;
		}

		// Wait for 1 second before retrying
		delay(1000);
	}

	// If this part is reached, no measurement was valid

	// Set the output values to N/A
	result.temperature=SENSOR_NA;
	result.humidity=SENSOR_NA;

	// Close the sensor device file descriptor
	close(sensor);

	// Return the processed output
	return result;
}
