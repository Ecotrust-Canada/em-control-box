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

#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <cctype>
#include "sys/time.h"
#include <sstream>
#include "util.h"
using namespace std;

void toUpperCase(char* s) {
    for(unsigned int i = 0; i < strlen(s); i++)
    {
        s[i] = toupper(s[i]);
    }
}

void hexStrToIntStr(char* hexStr, char* result) {
    static stringstream ss;
    long long tmp;
    ss.clear();
    ss << hexStr;
    ss >> hex >> tmp;
    ss.clear();
    ss << dec << tmp;
    ss >> dec >>result;
}

unsigned int htoi (const char *ptr) {
    unsigned int value = 0;
    char ch = *ptr;

    while (ch == ' ' || ch == '\t')
        ch = *(++ptr);

    for (;;)
    {

        if (ch >= '0' && ch <= '9')
            value = (value << 4) + (ch - '0');
        else if (ch >= 'A' && ch <= 'F')
            value = (value << 4) + (ch - 'A' + 10);
        else if (ch >= 'a' && ch <= 'f')
            value = (value << 4) + (ch - 'a' + 10);
        else
            return value;
        ch = *(++ptr);
    }
}

double now() {
    struct timeval  tv;
    gettimeofday(&tv, NULL);
    return tv.tv_usec + time(NULL) * 1000000;
}
