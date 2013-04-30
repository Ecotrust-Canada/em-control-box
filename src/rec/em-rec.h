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

#include <list>
#include "../config.h"

using namespace std; 

#ifndef EM_REC_H
#define EM_REC_H

#define VERSION "2.0.0"

#define POLL_PERIOD 1 * 1000000 // how many microseconds
#define USE_WARNING_HONK 0
#define HONK_SCAN_SUCCESS 1 
#define HONK_SCAN_DUPLICATE 2
#define HONK_SCAN_WARNING 3
#define NOTIFY_SCAN_INTERVAL 6 // # of POLL_PERIODS (which are by default 1s)
#define RECORD_SCAN_INTERVAL 300 // 5 minutes
#define REMOVE_DELAY 0
#define MD5_SALT "1535bc9732a631bb91f113bcf454c7e8"

typedef struct {
	char currentDateTime[32];
	unsigned long int iterationTimer;
	bool dataDrivePresent;
	char *dataDriveLabel;
	
	list<string> SENSOR_DATA_files;
	string SENSOR_DATA_lastHash;

	list<string> RFID_DATA_files;
	string RFID_DATA_lastHash;

	unsigned long int GPS_state;
	float GPS_latitude;
	float GPS_longitude;
	float GPS_heading;
	float GPS_speed;
	unsigned short int GPS_satquality;
	unsigned short int GPS_satsused;
	double GPS_hdop;
	float GPS_epe;

	unsigned long int RFID_state;
	unsigned long long int RFID_lastScannedTag;
	unsigned long long int RFID_lastSavedTag;
	unsigned long int RFID_lastSaveIteration;
	bool RFID_saveFlag;

	unsigned long int AD_state;
	float AD_psi;
	float AD_battery;
	unsigned short int AD_honkSound;
	unsigned long int AD_lastHonkIteration;

	unsigned long int COMPLIANCE_state;
} EM_DATA_TYPE;

extern CONFIG_TYPE CONFIG;
//extern EM_DATA_TYPE EM_DATA;

int main(int, char**);
void recordLoop();
void writeLogs(EM_DATA_TYPE*);
string appendLogWithMD5(string, list<string>, string);
void writeJSONState(EM_DATA_TYPE*);

#endif
