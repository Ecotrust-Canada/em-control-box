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

#ifndef GPS_SENSOR_H
#define GPS_SENSOR_H
#include "Sensor.h"
#include "em-rec.h"

#define MPS_TO_KNOTS 1.9438445

/**
 * @class GPSSensor
 * @brief A subclass of the Sensor class for the GPS sensor.
 *
 * The only difference with the parent class is that when the GPS sensor keeps outputing garbage,
 * we disconnect the sensor and reconnect to it.
 *
 */
class GPSSensor: public Sensor {
    private:
        void *_gpsmm;
        struct gps_data_t *GPS_DATA;

    public:
        /**
         * A Constructor. 
         * The serialHandle will be initialized when Connect function is called.
         * 
         * All error flags will be reset.
         */
        GPSSensor(unsigned long int*);

        /**
         * Connect to the serial port and set the serial handle.
         * @return 0 if succeeded, positive integer otherwise.
         */
        int Connect();
        int Receive(EM_DATA_TYPE*);
};

#endif