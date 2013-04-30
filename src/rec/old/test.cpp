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


#include "test.h"
#include <iostream>
#include "stdio.h"
#include "../config.h"

using namespace std;

int FAILS;
extern CONFIG_TYPE CONFIG;
extern char GPS_BUF[8192];

void emAssert(bool cond, const char* msg)
{
    if (!cond)
    {
        cerr << "FAIL: " << msg << endl;
        FAILS++;
    }
}


int loadFile(const char* filename, char* buf)
{
    long len;
    FILE* fp = fopen(filename, "r");
    fseek(fp,0,SEEK_END);        //go to end
    len=ftell(fp);               //get position at end (length)
    fseek(fp,0,SEEK_SET);        //go to beg.
    fread(buf, len, sizeof(char), fp);
    buf[len] = '\0';             //terminate string.
    fclose(fp);
    return 0;
}


void testGPS()
{
    double lat = 0, lng = 0, head = -1, speed = 0;

    cout << "packet 1" << endl;
    loadFile("test/gps-test-output.txt", GPS_BUF);
    parseGPS(GPS_BUF, lat, lng, head, speed);
    cout << lat << endl;
    emAssert(lat == 2945.6775, "latitude was parsed.");

    cout << "packet 2" << endl;
    loadFile("test/gps-test-output2.txt", GPS_BUF);
    parseGPS(GPS_BUF, lat, lng, head, speed);

    cout << "packet 3" << endl;
    loadFile("test/gps-test-output3.txt", GPS_BUF);
    parseGPS(GPS_BUF, lat, lng, head, speed);

    cout << "packet 4" << endl;
    loadFile("test/gps-test-output4.txt", GPS_BUF);
    parseGPS(GPS_BUF, lat, lng, head, speed);
}


// config can't be imported.
void testConfig() {
/*
    cout << "TESTING CONFIGURATION PARSER" << endl
        << "============================" << endl << endl
        << "vessel: " << CONFIG.vessel << endl
        << "vrn: " << CONFIG.vrn << endl;
*/
}


int runTests()
{
    cout << "Running Tests." << endl;
    FAILS = 0;
    testConfig();
    testGPS();
    if (FAILS)
    {
        cerr << "Tests failed: " << FAILS << endl;
    }
    else
    {
        cout << "All tests passed. " << endl;
    }
    return FAILS;
}
