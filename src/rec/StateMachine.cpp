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

#include "StateMachine.h"
#include <iostream>

using namespace std;

static signed long futureIterations[sizeof(long int)*8] = { -1 };

StateMachine::StateMachine(unsigned long* aState) {
    state = aState;
    *state = 0;
}

unsigned short StateMachine::bitmaskIndex(unsigned long b) {
    unsigned short i = 0;
    while (b >>= 1) i++;
    return i;
}

void StateMachine::SetErrorState(unsigned long errorFlag, unsigned short futureIteration) {
    unsigned short flagIndex = bitmaskIndex(errorFlag);

    if(futureIteration) {
        if(futureIterations[flagIndex] >= futureIteration) {
            futureIterations[flagIndex] = 0;
            D("Reached delayed error state in state machine, restarting");
        } else {
            futureIterations[flagIndex]++;
            return;
        }
    } else {
        futureIterations[flagIndex] = 0;
    }
    
    *state = *state | errorFlag;
}

void StateMachine::UnsetErrorState(unsigned long errorFlag) {
    *state = *state & (~errorFlag);
    futureIterations[bitmaskIndex(errorFlag)] = 0;
}

void StateMachine::UnsetAllErrorStates() {
    for(unsigned int i = 0; i < sizeof(futureIterations) / sizeof(futureIterations[0]); i++) {
        futureIterations[i] = 0;
    }

    *state = 0;
}

unsigned long int StateMachine::GetErrorState() {
    return *state;
}