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

#ifndef RFID_SENSOR_H
#define RFID_SENSOR_H
//#include <set>
#include "em-rec.h"
#include "Sensor.h"

/**
 * Buffer for RFID messages.
 */
#define RFID_BUF_SIZE		2048
#define RFID_BYTES_MIN		5
#define RFID_START_BYTE		0x3A
#define RFID_STOP_BYTE		0x0D
#define RFID_DATA_BYTES		10
#define RFID_CHK_BYTES		2
/* Multiple possible start bytes stored together as a string */
#define RFID_START_BYTES    "\x3A\x02"


/**
 * @class RFIDSensor
 * @brief A subclass of the Sensor class for the RFID sensor.
 *
 */
class RFIDSensor: public Sensor {
    private:
        EM_DATA_TYPE *em_data;
        string scanCountsFile;
        //set<unsigned long long int> MY_TAGS;
        int ASCIIToHex(char);
        int DecodeChecksum(char, char);
        bool checkStartByte(char);
        bool primaryChecksumTest(char*);
        bool alternateChecksumTest(char*);

    public:
        RFIDSensor(EM_DATA_TYPE*);
        int Connect();
        void GetScanCounts(string);
        int Receive();
        void resetStringScans();
        void resetTripScans();
        void Close();
        void saveToFile();
};

#endif
