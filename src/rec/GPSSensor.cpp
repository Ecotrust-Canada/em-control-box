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
#include "GPSSensor.h"
#include "libgpsmm.h"
using namespace std;

GPSSensor::GPSSensor(unsigned long int* _state):Sensor("GPS", _state, GPS_NO_CONNECTION, GPS_NO_DATA) {}

int GPSSensor::Connect() {
    if (_gpsmm == NULL || *state & GPS_NO_CONNECTION) {
        if(_gpsmm != NULL) ((gpsmm*)_gpsmm)->~gpsmm();
        _gpsmm = new gpsmm("localhost", DEFAULT_GPSD_PORT);
    }
    
    if (((gpsmm*)_gpsmm)->stream(WATCH_ENABLE|WATCH_JSON) == NULL) {
        cerr << "GPS: Failed to talk to gpsd (not running?)" << endl;
        UnsetAllErrorStates();
        SetErrorState(GPS_NO_CONNECTION);
        return 1;
    }

    UnsetErrorState(GPS_NO_CONNECTION);
    cout << name << ": Connected" << endl;

    return 0;
}

int GPSSensor::Receive(EM_DATA_TYPE* em_data) {
    if (*state & GPS_NO_CONNECTION) {
        if(Connect())
            // couldn't connect
            return 1;
    }
    
    // wait only half a second
    if(((gpsmm*)_gpsmm)->waiting(850000)) {
        if((GPS_DATA = ((gpsmm*)_gpsmm)->read()) == NULL) {
            UnsetAllErrorStates();
            SetErrorState(GPS_NO_CONNECTION);
            //cerr << "nothing to read" << endl;
            return 1;
        }
        //cerr << em_data->iterationTimer << " : READ something" << endl;
        UnsetErrorState(GPS_NO_DATA);
        //if(isConnected()) {
            //cerr << "CONNECTED" << endl;

            if (GPS_DATA->set & STATUS_SET) {
                //cerr << em_data->iterationTimer << " : STATUS_SET true" << endl;
                if(GPS_DATA->status == 0) {
                    SetErrorState(GPS_NO_DATA);
                    UnsetErrorState(GPS_ESTIMATED);
                    //cerr << em_data->iterationTimer << " : No data in STATUS_SET" << endl;
                } else if(GPS_DATA->status == 6) {
                    SetErrorState(GPS_ESTIMATED);
                    UnsetErrorState(GPS_NO_DATA);
                } else { // 
                    UnsetErrorState(GPS_NO_DATA);
                    UnsetErrorState(GPS_ESTIMATED);
                }

                //cerr << em_data->iterationTimer << " : Setting status: " << GPS_DATA->status << endl;
                em_data->GPS_satquality = GPS_DATA->status;
            }

            if (GPS_DATA->set & SATELLITE_SET) {
                if (GPS_DATA->satellites_visible == 0) {
                    SetErrorState(GPS_NO_SAT_IN_VIEW);
                } else {
                    UnsetErrorState(GPS_NO_SAT_IN_VIEW);
                }

                if (GPS_DATA->satellites_used == 0) {
                    SetErrorState(GPS_NO_SAT_IN_USE);
                } else { 
                    UnsetErrorState(GPS_NO_SAT_IN_USE);
                }

                em_data->GPS_satsused = GPS_DATA->satellites_used;
                //cerr << "SATS: " << GPS_DATA->satellites_visible << "/" << GPS_DATA->satellites_used << endl;
            }

            if(GPS_DATA->set & LATLON_SET) {
                em_data->GPS_latitude = GPS_DATA->fix.latitude;
                em_data->GPS_longitude = GPS_DATA->fix.longitude;
            }

            if(GPS_DATA->set & SPEED_SET) {
                em_data->GPS_speed = GPS_DATA->fix.speed * MPS_TO_KNOTS;
            }

            if(GPS_DATA->set & TRACK_SET) {
                em_data->GPS_heading = GPS_DATA->fix.track;
            }

            if(GPS_DATA->set & DOP_SET) {
                //cerr << "HDOP: " << GPS_DATA->dop.hdop << endl;
                em_data->GPS_hdop = GPS_DATA->dop.hdop;
            }

            if(GPS_DATA->set & HERR_SET) {
                //cerr << "EPX: " << GPS_DATA->fix.epx << "  EPY: " << GPS_DATA->fix.epy << "  EPE: " << GPS_DATA->epe << endl;
                em_data->GPS_epe = GPS_DATA->fix.epx;
            }
        //} else { cerr << "NOT CONNECTED" << endl; }
    } else {
        SetErrorState(GPS_NO_DATA);
        cerr << em_data->iterationTimer << " : No data in main loop" << endl;
        return 1;
    }

    return 0;
}