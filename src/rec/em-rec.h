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

using namespace std; 

#ifndef EM_REC_H
#define EM_REC_H

#include "settings.h"

#define VERSION "2.2.15"

// YOU PROBABLY SHOULDN'T CHANGE THESE WITHOUT A GOOD REASON
////////////////////////////////////////////////////////////
#define POLL_PERIOD 				1000000 // how many microseconds (default 1 sec) DO NOT ALTER
#define AUX_INTERVAL				5       // how many POLL_PERIODs before something different happens (all intervals in units of POLL_PERIOD) 5 seconds
#define PROC_STAT_VALS          	7
#define DISK_USAGE_START_VAL        30240
#define HONK_SCAN_SUCCESS			1
#define HONK_SCAN_DUPLICATE			2
#define HONK_SCAN_WARNING			3

#define MD5_SALT "1535bc9732a631bb91f113bcf454c7e8"

#define _EM_RUNNING   	smRecorder.GetState() & STATE_RUNNING
#define _OS_DISK_FULL 	smSystem.GetState() & SYS_OS_DISK_FULL
#define _DATA_DISK_FULL smSystem.GetState() & SYS_DATA_DISK_FULL
#define _AD   	  		smOptions.GetState() & OPTIONS_USING_AD
#define _RFID   		smOptions.GetState() & OPTIONS_USING_RFID
#define _GPS    		smOptions.GetState() & OPTIONS_USING_GPS
#define _GPRMC 			smOptions.GetState() & OPTIONS_GPRMC_ONLY_HACK
#define _ANALOG 		smOptions.GetState() & OPTIONS_ANALOG_CAMERAS
#define _IP     		smOptions.GetState() & OPTIONS_DIGITAL_CAMERAS

#define __EM_RUNNING 		((StateMachine *)em_data->sm_recorder)->GetState() & STATE_RUNNING
#define __OS_DISK_FULL 		((StateMachine *)em_data->sm_system)->GetState() & SYS_OS_DISK_FULL
#define __DATA_DISK_FULL 	((StateMachine *)em_data->sm_system)->GetState() & SYS_DATA_DISK_FULL
#define __AD     			((StateMachine *)em_data->sm_options)->GetState() & OPTIONS_USING_AD
#define __RFID   			((StateMachine *)em_data->sm_options)->GetState() & OPTIONS_USING_RFID
#define __GPS    			((StateMachine *)em_data->sm_options)->GetState() & OPTIONS_USING_GPS
#define __GPRMC  			((StateMachine *)em_data->sm_options)->GetState() & OPTIONS_GPRMC_ONLY_HACK
#define __ANALOG 			((StateMachine *)em_data->sm_options)->GetState() & OPTIONS_USING_ANALOG_CAMERAS
#define __IP     			((StateMachine *)em_data->sm_options)->GetState() & OPTIONS_USING_DIGITAL_CAMERAS
#define __SYS_GET_STATE     ((StateMachine *)em_data->sm_system)->GetState()
#define __SYS_SET_STATE(s)  ((StateMachine *)em_data->sm_system)->SetState(s)
#define __SYS_UNSET_STATE(s)  ((StateMachine *)em_data->sm_system)->UnsetState(s)

#include <string>

typedef struct {
	// program state data
	unsigned long runIterations;
	char currentDateTime[32];
	void *sm_recorder;
	void *sm_options;
	void *sm_system;
	void *sm_aux;

	// System
	string SYS_targetDisk;
	string SYS_dataDiskLabel;
	string SYS_fishingArea;
	unsigned short SYS_numCams;
	string SYS_uptime;
	string SYS_load;
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
	unsigned long SYS_dataDiskMinutesLeftFake;
	unsigned long SYS_videoSecondsRecorded;

	// GPS
	string GPS_homePortDataFile;
	string GPS_ferryDataFile;
	unsigned long GPS_time;
	double GPS_latitude;
	double GPS_longitude;
	double GPS_heading;
	double GPS_speed;
	unsigned short GPS_satQuality;
	unsigned short GPS_satsUsed;
	double GPS_hdop;
	double GPS_eph;

	// RFID
	unsigned long long RFID_lastScannedTag;
	unsigned long long RFID_lastSavedTag;
	unsigned long RFID_lastSaveIteration;
        unsigned long RFID_stringScans;
        unsigned long RFID_tripScans;
	bool RFID_saveFlag;

	// AD (Analog-Digital converter)
	float AD_psi;
	float AD_battery;
	unsigned short AD_honkSound;
	unsigned long AD_lastHonkIteration;

	pthread_mutex_t mtx;
} EM_DATA_TYPE;

int main(int, char**);
void *thr_auxiliaryLoop(void*);
void writeLog(string, string);
void writeLog(string, string, bool);
string writeLog(string, string, string);
void writeJSONState();
string updateSystemStats();
void reset_string_scans_handler(int);
void reset_trip_scans_handler(int);
void exit_handler(int);

#endif
