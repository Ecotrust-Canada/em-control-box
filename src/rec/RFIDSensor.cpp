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
#include "output.h"

#include <iostream>
#include <sstream>
#include <fstream>

using namespace std;

char RFID_BUF[RFID_BUF_SIZE];

RFIDSensor::RFIDSensor(EM_DATA_TYPE* _em_data):Sensor("RFID", RFID_NO_CONNECTION, RFID_NO_DATA) {
    em_data = _em_data;
}

int RFIDSensor::Connect() {
    /* Experiment; we don't use this anymore
    
    unsigned long long int tag;
    ifstream in("/mnt/data/TAG_LIST");

    if (in.is_open()) {
        while(in >> tag) MY_TAGS.insert(tag);
        in.close();
        O("Loaded /mnt/data/TAG_LIST");
    } */

    return Sensor::Connect();
}

void RFIDSensor::GetScanCounts(string file) {
    scanCountsFile = file;

    ifstream fin(scanCountsFile.c_str());
    if(!fin.fail()) {
        fin >> em_data->RFID_stringScans >> em_data->RFID_tripScans;
        fin.close();
        I("Got previous scan counts from " + scanCountsFile);
    }
}

int RFIDSensor::Receive() {
    char RFID_BUF[RFID_BUF_SIZE] = { '\0' };

    int bytesRead = Sensor::Receive(RFID_BUF, RFID_BYTES_MIN, RFID_BUF_SIZE, false);

    if(bytesRead >= RFID_BYTES_MIN) {
        char *ch = &RFID_BUF[0];

        // eat bytes until RFID_START_BYTES (: or \x02) or no more buffer left
        while(!checkStartByte(*ch) && ch < &RFID_BUF[bytesRead]) ch++;

        if(checkStartByte(*ch)) {
            char tagData[RFID_DATA_BYTES + RFID_CHK_BYTES] = { '\0' };
            unsigned int currentByte = 0;
            unsigned int scannedTotal = 0, scannedCorrupt = 0;
            // go through buffer and process all the tags
            while(++ch < &RFID_BUF[bytesRead]) {
                if(*ch == RFID_STOP_BYTE || currentByte == RFID_DATA_BYTES + RFID_CHK_BYTES) {
                    if (primaryChecksumTest(tagData) || alternateChecksumTest(tagData)) {    
                        tagData[RFID_DATA_BYTES] = '\0'; // terminate string after last data character
                        // store an unsigned long long representation of the tag data
                        em_data->RFID_lastScannedTag = strtoull(tagData, NULL, 16);
                    } else {
                        scannedCorrupt++;
                    }

                    scannedTotal++;
                    currentByte = 0;

                    while(!checkStartByte(*ch) && ch < &RFID_BUF[bytesRead]) ch++; // advance to next START_BYTE
                } else {
                    tagData[currentByte] = *ch;

                    currentByte++;
                }
            }

            if (scannedCorrupt > 0) {
                E(std::to_string(scannedCorrupt) + "/" + std::to_string(scannedTotal) + " scans were corrupt");
            }

            if (scannedCorrupt >= scannedTotal) {
                SetState(RFID_CHECKSUM_FAILED);
                em_data->RFID_saveFlag = false;
            } else {
                UnsetState(RFID_CHECKSUM_FAILED);

                // only save if RECORD_SCAN_INTERVAL time has gone by since the last scan, or the tag is different from the last one saved
                if(em_data->runIterations - em_data->RFID_lastSaveIteration >= RECORD_SCAN_INTERVAL || em_data->RFID_lastScannedTag != em_data->RFID_lastSavedTag) {
                   em_data->RFID_saveFlag = true;
                   em_data->RFID_stringScans++;
                   em_data->RFID_tripScans++;
                }

                // only make noise if NOTIFY_SCAN_INTERVAL time has gone by since the last scan, or the tag is different from the last one saved
                if(em_data->runIterations - em_data->AD_lastHonkIteration >= NOTIFY_SCAN_INTERVAL || em_data->RFID_lastScannedTag != em_data->RFID_lastSavedTag) {
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
int RFIDSensor::ASCIIToHex(char c) {
  if (c >= '0' && c <= '9')      return c - '0';
  else if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  else if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  else return -1;   // getting here is bad: it means the character was invalid
}

// given two ASCII-encoded hex chars, return the full byte
int RFIDSensor::DecodeChecksum(char a1, char a2) {
  int a = ASCIIToHex(a1);
  int b = ASCIIToHex(a2);

  if (a<0 || b<0) return -1;  // an invalid hex character was encountered
  else return (a*16) + b;
}

// Returns true if byte is a valid rfid start byte
bool RFIDSensor::checkStartByte(char byte) {
    // loop through the RFID_START_BYTES string return true if it includes byte
    for (unsigned int i = 0; RFID_START_BYTES[i]; ++i) {
        if (byte == RFID_START_BYTES[i]) {
            return true;
        }
    }
    return false;
}


// Returns true if tagdata contains a properly encoded rfid tag with a valid checksum, otherwise returns false
// The checksum calculation for this function is just the sum of each byte of the message
bool RFIDSensor::primaryChecksumTest(char* tagData) {
    int runningSum = 0; // stores running total for checksum
    int checksum; // stores the decoded checksum

    // loop through tagdata to compute checksum
    for (unsigned int i = 0; i < (RFID_DATA_BYTES); i++) {
        runningSum += tagData[i];
    }

    // calculate numeric value of checksum
    checksum = DecodeChecksum(tagData[RFID_DATA_BYTES], tagData[RFID_DATA_BYTES+1]);

    // if checksum is a bad value return false otherwise test if equal to running total
    if (checksum<0) {
        return false;
    } else {
        return runningSum%256 == checksum;
    }

}

// Returns true if tagdata contains a properly encoded rfid tag with a valid checksum, otherwise returns false
// The checksum calculation for this function is sum of the hexidecimal interpretation of each pair of bytes in the message
bool RFIDSensor::alternateChecksumTest(char* tagData) {
    int runningSum = 0; // stores running total for checksum
    int checksum; // stores the decoded checksum

    // if length is odd bad checksum return false (this should should compile away to nothing)
    if ((RFID_DATA_BYTES + RFID_CHK_BYTES)%2 == 1) {
        return false;
    }

    // loop through tagdata to compute checksum
    for (unsigned int i = 0; i < (RFID_DATA_BYTES); i+=2) {
        int b = DecodeChecksum(tagData[i], tagData[i+1]); //decoded byte
        // If we got a bad decode value, the checksum has failed so we return false.
        if (b<0) {
            return false;
        } else {
            runningSum += b;
        }
    }

    // calculate numeric value of checksum
    checksum = DecodeChecksum(tagData[RFID_DATA_BYTES], tagData[RFID_DATA_BYTES+1]);
    // if checksum is a bad value return false otherwise test if equal to running total
    if (checksum<0) {
        return false;
    } else {
        return runningSum%256 == checksum;
    }

}

void RFIDSensor::resetStringScans() {
    em_data->RFID_stringScans = 0;
}

void RFIDSensor::resetTripScans() {
    em_data->RFID_tripScans = 0;
}

void RFIDSensor::Close() {
    if(!scanCountsFile.empty() && !(__OS_DISK_FULL)) {
        ofstream fout(scanCountsFile.c_str());

        if(!fout.fail()) {
            fout << em_data->RFID_stringScans << ' ' << em_data->RFID_tripScans;
            fout.close();
            I("Wrote out scan counts to " + scanCountsFile);
        }
    }

    Sensor::Close();
}

void RFIDSensor::saveToFile() {
    if(!scanCountsFile.empty() && !(__OS_DISK_FULL)) {
        ofstream fout(scanCountsFile.c_str());

        if(!fout.fail()) {
            fout << em_data->RFID_stringScans << ' ' << em_data->RFID_tripScans;
            fout.close();
            I("Wrote out scan counts to " + scanCountsFile);
        }
    }
}