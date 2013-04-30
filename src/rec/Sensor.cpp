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

#include <iostream>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <cstring>
#include "Sensor.h"
using namespace std;

Sensor::Sensor(const char* aName, unsigned long int* _state, unsigned long int _NO_CONNECTION, unsigned long int _NO_DATA):StateMachine(_state) {
    name = aName;
    *state = _NO_CONNECTION;
    NO_CONNECTION = _NO_CONNECTION;
    NO_DATA = _NO_DATA;
}

void Sensor::SetPort(const char* aPort) {
    serialPort = aPort;
}

void Sensor::SetBaudRate(int aBaudRate) {
    baudRate = aBaudRate;
}

int Sensor::Connect() {
    static bool silenceConnectErrors = false;
    termios options;

    SetErrorState(NO_CONNECTION);

    for(int i = 0; *state & NO_CONNECTION && i < MAX_TRY_COUNT; i++) {
        if(i > 0) usleep(SENSOR_RECONNECT_DELAY);

        serialHandle = open(serialPort, O_RDWR | O_NOCTTY | O_NDELAY); // opens port

        if (serialHandle == -1) {
            if(!silenceConnectErrors) cerr << name << ": Failed to open port '" << serialPort << "'" << endl;
            continue;
        }

        // enable blocking I/O (read() blocks until bytes available)
        if(fcntl(serialHandle, F_SETFL, 0) == -1) {
            if(!silenceConnectErrors) cerr << name << ": Failed SETFL" << endl;
            continue;
        }

        // fill options struct
        if(tcgetattr(serialHandle, &options) == -1) {
            if(!silenceConnectErrors) cerr << name << ": Failed to get serial config" << endl;
            continue;
        }

        if(cfsetispeed(&options, baudRate) == -1) { // sets baud rates
            if(!silenceConnectErrors) cerr << name << ": Failed to set input baud rate" << endl;
            continue;
        }

        if(cfsetospeed(&options, baudRate) == -1) { // sets baud rates
            if(!silenceConnectErrors) cerr << name << ": Failed to set output baud rate" << endl;
            continue;
        }

        // no parity 8N1 mode
        options.c_cflag &= ~PARENB;  // sets some default communication flags
        options.c_cflag &= ~CSTOPB;
        options.c_cflag &= ~CSIZE;
        options.c_cflag |= CS8;

        // disable hardware flow control
        options.c_cflag &= ~CRTSCTS;

        // disable hang-up on close to avoid reset
        options.c_cflag &= ~HUPCL;
        //options.c_cflag |= HUPCL;

        // turn on READ & ignore ctrl lines
        options.c_cflag |= CREAD | CLOCAL;
        
        options.c_lflag &= ~ISIG; // disable signals
        options.c_lflag &= ~ICANON; // disable canonical input
        options.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL | ECHOCTL | ECHOPRT | ECHOKE); // disable all echoing

        // disable input parity checking, marking, and character mapping
        options.c_iflag &= ~(INPCK | PARMRK | INLCR | ICRNL | IUCLC);

        // turn off s/w flow ctrl; maybe comment this out for non-GPS sensors
        options.c_iflag &= ~(IXON | IXOFF | IXANY);
        
        // raw output
        options.c_oflag &= ~OPOST;
        
        options.c_cc[VMIN]  = 0;
        options.c_cc[VTIME] = 20;

        if(tcsetattr(serialHandle, TCSANOW, &options) == -1) {
            if(!silenceConnectErrors) cerr << name << ": Failed to set config" << endl;
            continue;
        }

        UnsetErrorState(NO_CONNECTION);
        cout << name << ": Connected" << endl;
    }

    if(*state & NO_CONNECTION && !silenceConnectErrors) {
        cerr << name << ": Failed to connect after " << MAX_TRY_COUNT << " tries; will keep at it but silencing further errors" << endl;
        silenceConnectErrors = true;
        return -1;
    }

    return 0;
}

int Sensor::Send(const char* cmd) {
    int bytesWritten = write(serialHandle, cmd, strlen(cmd));
    
    if(bytesWritten == -1) {
        SetErrorState(NO_CONNECTION);
    } else {
        UnsetErrorState(NO_CONNECTION);
    }

    return bytesWritten;
}

int Sensor::Receive(char buf[], int min, int max, bool set_no_data) {
    if(*state & NO_CONNECTION) Connect();

    int bytesToRead = 0, bytesRead = 0;
    int status = ioctl(serialHandle, FIONREAD, &bytesToRead); // check how many bytes are waiting to be read

    if (status == -1) {
        SetErrorState(NO_CONNECTION);
        return status;
    } else {
        UnsetErrorState(NO_CONNECTION);
    }

    if(bytesToRead > max) bytesToRead = max;

    if(bytesToRead >= min) {
        bytesRead = read(serialHandle, buf, bytesToRead);
        buf[bytesRead] = '\0'; // null terminate the data.
        UnsetErrorState(NO_DATA);
    } else {
        if (set_no_data) SetErrorState(NO_DATA);
    }

    return bytesRead;
}

void Sensor::Close() {
    close(serialHandle);
}

bool Sensor::isConnected() {
    if (*state & NO_CONNECTION) return false;

    return true;
}