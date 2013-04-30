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

//#include <cstdio>
//#include <iostream>
//#include <string>
#include "StateMachine.h"

using namespace std;

StateMachine::StateMachine(unsigned long int* aState) {
    state = aState;
    *state = 0;
}

void StateMachine::SetErrorState(long errorFlag) {
    *state = *state | errorFlag;
}

void StateMachine::UnsetErrorState(long errorFlag) {
    *state = *state & (~ errorFlag);
}

void StateMachine::UnsetAllErrorStates() {
    *state = 0;
}

unsigned long int StateMachine::GetErrorState() {
    return *state;
}