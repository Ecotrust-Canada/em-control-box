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

#include "RFIDSensor.h"
#include <iostream>
#include <sstream>

using namespace std;

char RFID_BUF[RFID_BUF_SIZE];

RFIDSensor::RFIDSensor(EM_DATA_TYPE* _em_data):Sensor("RFID", &_em_data->RFID_state, RFID_NO_CONNECTION, RFID_NO_DATA) {
    em_data = _em_data;
}

int RFIDSensor::Connect() {
    /* Experiment; we don't use this anymore
    
	unsigned long long int tag;
    ifstream in("/mnt/data/TAG_LIST");

    if (in.is_open()) {
        while(in >> tag) MY_TAGS.insert(tag);
        in.close();
        cout << "RFID: Loaded /mnt/data/TAG_LIST" << endl;
    } */

    return Sensor::Connect();
}

int RFIDSensor::Receive() {
	char RFID_BUF[RFID_BUF_SIZE] = { '\0' };

    int bytesRead = Sensor::Receive(RFID_BUF, RFID_BYTES_MIN, RFID_BUF_SIZE, false);

    if(bytesRead >= RFID_BYTES_MIN) {
        char *ch = &RFID_BUF[0];

        // eat bytes until RFID_START_BYTE (:) or no more buffer left
        while(*ch != RFID_START_BYTE && ch < &RFID_BUF[bytesRead]) ch++;

        if(*ch == RFID_START_BYTE) {
        	char tagData[RFID_DATA_BYTES + RFID_CHK_BYTES] = { '\0' };
            unsigned int currentByte = 0, runningSum = 0;
            unsigned int scannedTotal = 0, scannedCorrupt = 0;

            // go through buffer and process all the tags
            while(++ch < &RFID_BUF[bytesRead]) {
            	if(*ch == RFID_STOP_BYTE || currentByte == RFID_DATA_BYTES + RFID_CHK_BYTES) {
            		if (runningSum % 256 == DecodeChecksum(tagData[RFID_DATA_BYTES], tagData[RFID_DATA_BYTES + 1])) {	
            			tagData[RFID_DATA_BYTES] = '\0';
            			em_data->RFID_lastScannedTag = hexToInt(tagData);
            		} else {
            			scannedCorrupt++;
            		}

            		scannedTotal++;
            		currentByte = 0;
            		runningSum = 0;

            		ch++; // advance to next START_BYTE
            	} else {
            		tagData[currentByte] = *ch;
            		if(currentByte < RFID_DATA_BYTES) runningSum += *ch;

            		currentByte++;
            	}
            }

            if (scannedCorrupt > 0) cerr << "RFID: " << scannedCorrupt << "/" << scannedTotal << " scans were corrupt" << endl;

            if (scannedCorrupt >= scannedTotal) {
            	SetState(RFID_CHECKSUM_FAILED);
            	em_data->RFID_saveFlag = false;
            } else {
            	UnsetState(RFID_CHECKSUM_FAILED);

                // only save if RECORD_SCAN_INTERVAL time has gone by since the last scan, or the tag is different from the last one saved
                if(em_data->iterationTimer - em_data->RFID_lastSaveIteration >= RECORD_SCAN_INTERVAL || em_data->RFID_lastScannedTag != em_data->RFID_lastSavedTag) {
            	   em_data->RFID_saveFlag = true;
                   em_data->RFID_stringScans++;
                   em_data->RFID_tripScans++;
                }

                // only make noise if NOTIFY_SCAN_INTERVAL time has gone by since the last scan, or the tag is different from the last one saved
                if(em_data->iterationTimer - em_data->AD_lastHonkIteration >= NOTIFY_SCAN_INTERVAL || em_data->RFID_lastScannedTag != em_data->RFID_lastSavedTag) {
                    /*if(USE_WARNING_HONK && MY_TAGS.find(em_data->RFID_lastScannedTag) == MY_TAGS.end()) {
                        em_data->AD_honkSound = HONK_SCAN_WARNING;
                    } else*/ if(em_data->RFID_lastScannedTag != em_data->RFID_lastSavedTag) {
                        em_data->AD_honkSound = HONK_SCAN_SUCCESS;
                    } else {
                        em_data->AD_honkSound = HONK_SCAN_DUPLICATE;
                    }
                }
            }
		}

		return bytesRead;
	}

    return 0;
}

// given an ASCII char c that represents a hex (0-f) value, return the value
unsigned int RFIDSensor::ASCIIToHex(char c) {
  if (c >= '0' && c <= '9')      return c - '0';
  else if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  else if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  else return -1;   // getting here is bad: it means the character was invalid
}

// given two ASCII-encoded hex chars, return the full byte
unsigned int RFIDSensor::DecodeChecksum(char a1, char a2) {
  unsigned int a = ASCIIToHex(a1);
  unsigned int b = ASCIIToHex(a2);

  if (a<0 || b<0) return -1;  // an invalid hex character was encountered
  else return (a*16) + b;
}

unsigned long long int RFIDSensor::hexToInt(char* hexStr) {
    stringstream ss;
    unsigned long long int tmp;

    ss << hexStr;
    ss >> hex >> tmp;

    ss.clear();
    ss << dec << tmp;
    ss >> dec >> tmp;

    return tmp;
}

void RFIDSensor::resetStringScans() {
    em_data->RFID_stringScans = 0;
}

void RFIDSensor::resetTripScans() {
    em_data->RFID_tripScans = 0;
}
