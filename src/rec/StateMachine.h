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

#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#define GPS_NO_CONNECTION 1
#define GPS_NO_DATA 2
#define GPS_NO_SAT_IN_VIEW 4
#define GPS_NO_SAT_IN_USE 8
#define GPS_ESTIMATED 16

#define AD_NO_CONNECTION 32
#define AD_NO_DATA 64
#define AD_PSI_LOW_OR_ZERO 128 // pressure is too low or reading 0
#define AD_BATTERY_LOW 256
#define AD_BATTERY_HIGH 512

#define RFID_NO_CONNECTION 1024
#define RFID_NO_DATA 2048
#define RFID_CHECKSUM_FAILED 4096

/*
#define COMPLIANCE_NO_FERRY_AREA_DATA 512
#define COMPLIANCE_NO_HOMEPORT_DATA 1024
#define COMPLIANCE_INSIDE_FERRY_LANE 2048
#define COMPLIANCE_NEAR_HOMEPORT 4096
*/

/**
 */
class StateMachine {
    protected:
        unsigned long int *state; ///< Contains all the error flags of the sensor.
    
    public:
        /**
         * A Constructor. 
         * The serialHandle will be initialized when Connect function is called.
         * 
         * All error flags will be reset.
         */
        StateMachine(unsigned long int*);

        /**
         * Set the error flag.
         */
        void SetErrorState(long);

        /**
         * Reset the error flag.
         */
        void UnsetErrorState(long);

        /**
         * Reset all the error states.
         */
        void UnsetAllErrorStates();

        void PrintState();

        unsigned long int GetErrorState();
};

#endif