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
#include "em-rec.h"

#define GPS_NO_CONNECTION       0x0001
#define GPS_NO_DATA             0x0002 // shared memory area is not being updated (GPSd not running?) -OR- GPS is not feeding data
#define GPS_NO_FIX              0x0004 // you can still be getting data but have no fix
#define GPS_ESTIMATED           0x0008
#define GPS_NO_FERRY_DATA       0x0010
#define GPS_NO_HOME_PORT_DATA   0x0020
#define GPS_INSIDE_FERRY_LANE   0x0040
#define GPS_INSIDE_HOME_PORT    0x0080

#define AD_NO_CONNECTION        0x0100
#define AD_NO_DATA              0x0200
#define AD_PSI_LOW_OR_ZERO      0x0400 // pressure is too low or reading 0
#define AD_BATTERY_LOW          0x0800
#define AD_BATTERY_HIGH         0x1000

#define RFID_NO_CONNECTION      0x2000
#define RFID_NO_DATA            0x4000
#define RFID_CHECKSUM_FAILED    0x8000

#define GPS_NO_DATA_DELAY       3      // how many times must NO_DATA be set before it's exposed
                                       // when checking for updated GPS info

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
        void SetErrorState(unsigned long, unsigned short = 0);

        /**
         * Reset the error flag.
         */
        void UnsetErrorState(unsigned long);

        /**
         * Reset all the error states.
         */
        void UnsetAllErrorStates();

        void PrintState();

        unsigned long int GetErrorState();

    private:
        unsigned short bitmaskIndex(unsigned long);

};

#endif