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

#include <string>

typedef struct {
    std::string	vessel,
			vrn,
			arduino_type,
			psi_vmin,
			EM_DIR,
			OS_DISK,
			DATA_DISK,
			JSON_STATE_FILE,
			ARDUINO_DEV,
			GPS_DEV,
			RFID_DEV,
			HOME_PORT_DATA,
			FERRY_DATA;

	unsigned short psi_low_threshold,
				   psi_high_threshold,
				   fps_low_delay;
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
std::string getConfig(const char*, const char*);

#endif
