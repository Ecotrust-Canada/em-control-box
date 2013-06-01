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

#define STATE_RUNNING true
#define STATE_NOT_RUNNING false

#define POLL_PERIOD 1000000 // how many microseconds
#define STATS_COLLECTION_INTERVAL 10       // how many POLL_PERIODs before something different happens
#define USE_WARNING_HONK 0
#define HONK_SCAN_SUCCESS 1 
#define HONK_SCAN_DUPLICATE 2
#define HONK_SCAN_WARNING 3
#define NOTIFY_SCAN_INTERVAL 6 // # of POLL_PERIODS (which are by default 1s)
#define RECORD_SCAN_INTERVAL 300 // 5 minutes
#define REMOVE_DELAY 0
#define MD5_SALT "1535bc9732a631bb91f113bcf454c7e8"

#define DEBUG

#ifdef DEBUG
	#define D(s) cerr << "DEBUG: " s << endl;
	#define OVERRIDE_SILENCE true
#else
	#define D(s)
	#define OVERRIDE_SILENCE false
#endif

typedef struct {
	unsigned long iterationTimer;
    unsigned short runState;
    char currentDateTime[32];

	list<string> SENSOR_DATA_files;
	list<string> RFID_DATA_files;
	string SENSOR_DATA_lastHash;
	string RFID_DATA_lastHash;
	
	bool SYS_dataDiskPresent;
	char *SYS_dataDiskLabel;

	char SYS_uptime[16];
	char SYS_load[24];
	double SYS_cpuPercent;
	unsigned long long SYS_ramFreeKB;
	unsigned long long SYS_ramTotalKB;
	unsigned short SYS_tempCore0;
	unsigned short SYS_tempCore1;
	unsigned long SYS_osDiskFreeBlocks;
	unsigned long SYS_osDiskTotalBlocks;
	unsigned long SYS_osDiskMinutesLeft;
	unsigned long SYS_dataDiskFreeBlocks;
	unsigned long SYS_dataDiskTotalBlocks;
	unsigned long SYS_dataDiskMinutesLeft;

	unsigned long GPS_state;
	char GPS_homePortDataFile[48];
	char GPS_ferryDataFile[48];
	unsigned long GPS_time;
	double GPS_latitude;
	double GPS_longitude;
	double GPS_heading;
	double GPS_speed;
	unsigned short GPS_satQuality;
	unsigned short GPS_satsUsed;
	double GPS_hdop;
	double GPS_eph;

	unsigned long RFID_state;
	unsigned long long RFID_lastScannedTag;
	unsigned long long RFID_lastSavedTag;
	unsigned long RFID_lastSaveIteration;
        unsigned long RFID_stringScans;
        unsigned long RFID_tripScans;
	bool RFID_saveFlag;

	unsigned long AD_state;
	float AD_psi;
	float AD_battery;
	unsigned short AD_honkSound;
	unsigned long AD_lastHonkIteration;

	//unsigned long int COMPLIANCE_state;
} EM_DATA_TYPE;

extern CONFIG_TYPE CONFIG;
//extern EM_DATA_TYPE EM_DATA;

int main(int, char**);
void reset_string_scans_handler(int);
void reset_trip_scans_handler(int);
void exit_handler(int);
void recordLoop();
void writeLogs(EM_DATA_TYPE*);
string appendLogWithMD5(string, list<string>, string);
void computeSystemStats(EM_DATA_TYPE*, unsigned short);
void writeJSONState(EM_DATA_TYPE*);

#endif
