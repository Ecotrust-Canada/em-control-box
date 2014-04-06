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
#include "em-rec.h"
#include "Sensor.h"
#include "gps.h"

//#include 

#define GPS_STATE_DELAY     3 * POLL_PERIOD / (POLL_PERIOD/5 + POLL_PERIOD/75)
#define GPS_THREAD_CLOSE_DELAY 100
#define MAX_POLYS           10
#define MAX_POINTS          20

struct POINT {
    double x;
    double y;
};

// these are from GPSd code
#define EMIX(x, y) (((x) > (y)) ? (x) : (y))
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
        pthread_t pt_receiveLoop;
        StateMachine smGPSConsumerThread;
        EM_DATA_TYPE *em_data;
        struct gps_data_t GPS_DATA, GPS_DATA_buf, GPS_DATA_empty;
        POINT home_ports[MAX_POLYS][MAX_POINTS];
        POINT ferry_lanes[MAX_POLYS][MAX_POINTS];
        unsigned short num_home_ports,  num_home_port_edges[MAX_POLYS];
        unsigned short num_ferry_lanes, num_ferry_lane_edges[MAX_POLYS];

        pthread_mutex_t mtx_GPS_DATA_buf;

        static void *thr_ReceiveLoopLauncher(void*);
        void thr_ReceiveLoop();
        unsigned short LoadKML(std::string, POINT[MAX_POLYS][MAX_POINTS], unsigned short *);
        short IsPointInsidePoly(const POINT &, const POINT *, unsigned short);

    public:
        GPSSensor(EM_DATA_TYPE*);
        ~GPSSensor();
        bool InSpecialArea();
        int Receive();
        int Connect();
        void Close();
};

#endif
