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

#define STATE_CLOSING_OR_UNDEFINED 0x0001
#define STATE_NOT_RUNNING          0x0002
#define STATE_RUNNING              0x0004

#define SM_EXCLUSIVE_STATES         true
#define SM_MULTIPLE_STATES          false

#include "em-rec.h"
#include "states.h"

#include <pthread.h>

class StateMachine {
    protected:
        unsigned long state; ///< Contains all the error flags of the sensor.
    
    public:
        /**
         * A Constructor. 
         * The serialHandle will be initialized when Connect function is called.
         * 
         * All error flags will be reset.
         */
        StateMachine(unsigned long, bool);
        ~StateMachine();

        /**
         * Set the error flag.
         */
        void SetState(unsigned long, unsigned short = 0);

        /**
         * Reset the error flag.
         */
        void UnsetState(unsigned long);

        /**
         * Reset all the error states.
         */
        void UnsetAllStates();

        void PrintState();

        unsigned long GetState();

    private:
        pthread_mutex_t mtx_state;
        unsigned short bitmaskIndex(unsigned long);
        bool exclusiveStyle;
        signed long futureIterations[sizeof(long int)*8];
};

#endif
