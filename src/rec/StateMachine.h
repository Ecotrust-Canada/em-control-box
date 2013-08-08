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

#define GPS_STATE_DELAY 3 // how many times must NO_DATA/NO_FIX be set before it's exposed when checking for updated GPS info
#define AD_PSI_LOW_OR_ZERO_DELAY 3

#include "em-rec.h"
#include "states.h"

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
