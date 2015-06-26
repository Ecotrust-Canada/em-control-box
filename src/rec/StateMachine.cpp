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
#include "output.h"

#include <iostream>

using namespace std;

StateMachine::StateMachine(unsigned long initialState, bool _exclusiveStyle) {
    pthread_mutex_init(&mtx_state, NULL);
    state = initialState;
    exclusiveStyle = _exclusiveStyle;

    for(unsigned int i = 0; i < sizeof(long int) * 8; i++) {
        futureIterations[i] = -1;
    }
}

StateMachine::~StateMachine() {
    pthread_mutex_destroy(&mtx_state);
}

unsigned short StateMachine::bitmaskIndex(unsigned long b) {
    unsigned short i = 0;
    while (b >>= 1) i++;
    return i;
}

void StateMachine::SetState(unsigned long errorFlag, unsigned short futureIteration) {
    string moduleName = "SM";
    unsigned short flagIndex = bitmaskIndex(errorFlag);
    signed long savedFutureIteration = futureIterations[flagIndex];

    if(exclusiveStyle) UnsetAllStates();
    futureIterations[flagIndex] = savedFutureIteration;

    pthread_mutex_lock(&mtx_state);
        if(futureIteration) {
            if(futureIterations[flagIndex] >= futureIteration - 1) {
                D("Reached delayed error state, restarting counter (" + to_string(errorFlag) + ": " + to_string(futureIterations[flagIndex]) + ")");
                futureIterations[flagIndex] = 0;
            } else {
                futureIterations[flagIndex]++;
                pthread_mutex_unlock(&mtx_state);
                return;
            }
        } else {
            futureIterations[flagIndex] = 0;
        }
        
        state = state | errorFlag;
    pthread_mutex_unlock(&mtx_state);
}

void StateMachine::UnsetState(unsigned long errorFlag) {
    pthread_mutex_lock(&mtx_state);
        state = state & (~errorFlag);
        futureIterations[bitmaskIndex(errorFlag)] = 0;
    pthread_mutex_unlock(&mtx_state);
}

void StateMachine::UnsetAllStates() {
    pthread_mutex_lock(&mtx_state);
        for(unsigned int i = 0; i < sizeof(futureIterations) / sizeof(futureIterations[0]); i++) {
            futureIterations[i] = 0;
        }

        state = 0;
    pthread_mutex_unlock(&mtx_state);
}

unsigned long StateMachine::GetState() {
    pthread_mutex_lock(&mtx_state);
        unsigned long int _state = state;
    pthread_mutex_unlock(&mtx_state);

    return _state;
}
