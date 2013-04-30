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

#include "serial.h"
#include "iostream"
int handle;

void usage()
{
    cout << "Usage: checkserial <device path> <number of bytes to listen for>" << endl;
}

/**
 * Test the serial device by listening required number of bypes from the device.
 */
int main(int argc, char* argv[])
{
    char buf[1000];
    int bytes = 10;
    if (argc == 1)
    {
        usage();
        return 0;
    }
    else if (argc > 2)
    {
        bytes = atoi(argv[2]);
    }
    if (serialOpen(argv[1], handle, argc > 3))
    {
        cout << "couldn't connect to " << argv[1] << "." << endl;
        exit(0);
    }
    else
    {
        cout << "connected to " << argv[1] << ". Polling..." << endl;
        serialPoll(handle, buf, bytes, 1000, 10000);
        cout << argv[1] << " said " << buf << endl << endl;
    }
    return 0;
};
