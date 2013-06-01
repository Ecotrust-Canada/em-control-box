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

#define CONFIG_FILENAME "/etc/em.conf"
#include "util.h"

typedef struct {
    char
#ifdef EM_REC
   			vessel[32],
			vrn[16],
			arduino[16],
			fishing_area[16],
			psi_vmin[16],
			JSON_STATE_FILE[32],
			ARDUINO_DEV[32],
			GPS_DEV[32],
			RFID_DEV[32],
			SENSOR_DATA[32],
			RFID_DATA[32],
#elif defined EM_GRAB
			framerate[4],
#endif
			DATA_MNT[32],
			BACKUP_DATA_MNT[32],
			HOMEPORT_DATA[48],
			FERRY_DATA[48];
} CONFIG_TYPE;

/**
 * Print the configuration to the char array.
 * @param out The char array to print the configuration to.
 */
char* printConfig(char*);

/**
 * Read the configuration from the file defined by CONFIG_FILENAME.
 */
int readConfigFile();

char* getConfig(const char*);

/**
 * For performance - cache the config in a structure.
 */
char* buildConfigCache();

#endif