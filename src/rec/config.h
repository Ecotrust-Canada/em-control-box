/*
EM: Electronic Monitor - Control Box Software
Copyright (C) 2012 Ecotrust Canada
Knowledge Systems and Planning

This file is part of EM.

EM is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

EM is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with EM. If not, see <http://www.gnu.org/licenses/>.

You may contact Ecotrust Canada via our website http://ecotrust.ca
*/

#ifndef CONFIG_H
#define CONFIG_H

#define DEFAULT_fishing_area		"A"
#define DEFAULT_vessel 				"NOT_CONFIGURED"
#define DEFAULT_vrn					"00000"
#define DEFAULT_arduino 			"5V"
#define DEFAULT_psi_vmin			"0.90"
#define DEFAULT_cam 				"1"

#define DEFAULT_EM_DIR				"/var/em"
#define DEFAULT_OS_DISK				"/var/em/data"
#define DEFAULT_DATA_DISK 			"/mnt/data"
#define DEFAULT_JSON_STATE_FILE 	"/tmp/em_state.json"
#define DEFAULT_ARDUINO_DEV 		"/dev/arduino"
#define DEFAULT_GPS_DEV 			"/dev/ttyS0"
#define DEFAULT_RFID_DEV 			"/dev/ttyS1"
#define DEFAULT_HOME_PORT_DATA 		"/opt/em/public/a_home_ports.kml"
#define DEFAULT_FERRY_DATA			"/opt/em/public/a_ferry_lanes.kml"

typedef struct {
    char	vessel[32],
			vrn[16],
			arduino_type[16],
			//fishing_area[16],
			psi_vmin[16],
			//cam[16],
			EM_DIR[32],
			OS_DISK[32],
			DATA_DISK[32],
			JSON_STATE_FILE[32],
			//VIDEOS_DIR[32],
			ARDUINO_DEV[32],
			GPS_DEV[32],
			RFID_DEV[32],
			HOME_PORT_DATA[48],
			FERRY_DATA[48];
			//PAUSE_MARKER[48],
			//SCREENSHOT_MARKER[48];
} CONFIG_TYPE;

/**
 * Print the configuration to the char array.
 * @param out The char array to print the configuration to.
 */
char* printConfig(char*);

/**
 * Read the configuration from the file defined by CONFIG_FILENAME.
 */
int readConfigFile(const char *);

const char* getConfig(const char*, const char*);

#endif
