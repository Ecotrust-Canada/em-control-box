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

#ifndef SENSOR_H
#define SENSOR_H
//#include <cstring>
//#include <iostream>
//#include <sys/ioctl.h>
//#include <fcntl.h>
//#include <termios.h>
#include <termios.h>
#include "StateMachine.h"

#define SENSOR_RECONNECT_DELAY 10000 // in usec; if connection failed, this is the wait time until next reconnection
#define MAX_TRY_COUNT 10 ///< Maximum number of times to try to reconnect to the sensor if connection keeps failing.

/**
 * @class Sensor
 * @brief A class for all the sensors: GPS, AD, RFID
 * This is a wrapper class of the serial operations defined in serial.h and serial.cpp
 * , together with some error state trackings.
 *
 */
class Sensor: public StateMachine {
    private:
        unsigned long int NO_CONNECTION;
        unsigned long int NO_DATA;
        const char* serialPort; ///< The port of the serial device is plugged in.
        int baudRate; ///< Whether to use 4800 or 9600 or 19200 as the speed when listening to the serial port.
        int serialHandle; ///< The serial file handle.
        
        int serialFlush(int);

        /**
         * Opens a serial port for sending and receiving messages.
         *
         * @param port index of port to open.
         * @param hSerial the handle of an open serial port.
         * @param nmea whether to use nmea serial standard settings.
         * @return 0 if the call succeeded.
         */
        int serialOpen(const char*, int&, int);

        /**
         * Send a string to a serial port.
         *
         * @param cmd the string to send
         * @param hSerial the handle of an open serial port.
         * @return 0 if the call succeeded.
         */
        int serialSend(const char*, int);
    
    public:
        /**
         * A Constructor. 
         * The serialHandle will be initialized when Connect function is called.
         * 
         * All error flags will be reset.
         */
        Sensor(const char*, unsigned long int*, unsigned long int, unsigned long int);

        const char* name; ///< The name of the sensor, i.e., GPS, AD, RFID

        void SetPort(const char*);
        void SetBaudRate(int);

        /**
         * Close the handle.
         */
        virtual void Close();

        /**
         * Connect to the serial port and set the serial handle.
         * @return 0 if succeeded, positive integer otherwise.
         */
        virtual int Connect();

        /**
         * Send a message to the serial port.
         * If fails, it tries to reconnect to the port by calling the Connect function.
         * @param cmd The message to be sent.
         * @return 0 if succeeded, positive integer otherwise.
         */
        virtual int Send(const char*);

        /**
         * Receive bytes from the serial ports and store it to a buffer.
         * If fails, it tries to reconnect to the port by calling the Connect function.
         * @param buf The buffer to store the data from the sensor.
         * @param min The minimum bytes to read.
         * @param max The maximum bytes to read.
         * @param bool Set NO_DATA if Receive doesn't see data
         * @return 0 if succeeded, positive integer otherwise.
         */
        virtual int Receive(char[], int, int, bool = true);

        bool isConnected();
};

#endif