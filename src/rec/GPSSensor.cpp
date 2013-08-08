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

#include "GPSSensor.h"
#include <iostream>
#include <sstream>
#include <fstream>
//#include <cstring>

using namespace std;

GPSSensor::GPSSensor(EM_DATA_TYPE* _em_data):Sensor("GPS", &_em_data->GPS_state, GPS_NO_CONNECTION, GPS_NO_DATA) {
    em_data = _em_data;
}

int GPSSensor::Connect() {
    static bool silenceConnectErrors = false;

    if(GPS_DATA.privdata != NULL && PRIVATE(GPS_DATA)->shmseg != NULL) {
        D("In GPSSensor::Connect() but shmseg set, doing gps_close()");
        gps_close(&GPS_DATA);
    }
    
    if (gps_open(GPSD_SHARED_MEMORY, 0, &GPS_DATA) != 0) {
        if(!silenceConnectErrors || OVERRIDE_SILENCE) {
            cerr << "GPS: Failed to attach to GPSd shared memory segment (is GPSd running?); will keep at it but silencing further Connect() errors" << endl;
            silenceConnectErrors = true;
        }
        
        //UnsetAllErrorStates();
        SetErrorState(GPS_NO_CONNECTION);
        return -1;
    }
    
    UnsetErrorState(GPS_NO_CONNECTION);
    cout << name << ": Connected" << endl;

    if(!(*state & GPS_NO_HOME_PORT_DATA) && num_home_ports == 0) {
        if((num_home_ports = LoadKML(em_data->GPS_homePortDataFile, home_ports, num_home_port_edges)) == 0) {
            SetErrorState(GPS_NO_HOME_PORT_DATA);
        }
    }

    if(!(*state & GPS_NO_FERRY_DATA) && num_ferry_lanes == 0) {
        if((num_ferry_lanes = LoadKML(em_data->GPS_ferryDataFile, ferry_lanes, num_ferry_lane_edges)) == 0) {
            SetErrorState(GPS_NO_FERRY_DATA);
        }
    }

    return 0;
}

int GPSSensor::Receive() {
    static timestamp_t last_online;
    static double /*last_latitude, last_longitude, last_speed,*/ last_time;
 
    if (em_data->iterationTimer < GPS_WARMUP_PERIOD) {
        return 0;
    } else if ((*state & GPS_NO_CONNECTION /*|| *state & GPS_NO_DATA*/) && Connect() != 0) {
        D("Tried to GPSSensor::Receive() but not connected and Connect() failed");
        return -1;
    }

    if (isConnected()) {
        if(!gps_read(&GPS_DATA)) {
            D("gps_read() didn't succeed");
            //UnsetAllErrorStates();
            SetErrorState(GPS_NO_CONNECTION);
            return -1;
        }

        if(GPS_DATA.online == last_online) {
            D("GPS data hasn't been updated, setting GPS_NO_DATA with delay");
            SetErrorState(GPS_NO_DATA, GPS_STATE_DELAY);
        } else {
            UnsetErrorState(GPS_NO_DATA);
        }

        if(GPS_DATA.status == 0 || 
            /*GPS_DATA.satellites_visible == 0 || GPS_DATA.satellites_used == 0*/ // we don't do it this way anymore because in case the GPS is configured to send only GPRMC we get no satellite info but still have a fix
            (/*GPS_DATA.fix.latitude == last_latitude &&
            GPS_DATA.fix.longitude == last_longitude &&
            GPS_DATA.fix.speed == last_speed &&*/
            GPS_DATA.fix.time == last_time &&
            GPS_DATA.satellites_used == 0)) {
            SetErrorState(GPS_NO_FIX, GPS_STATE_DELAY);

            if (GetErrorState() & GPS_NO_FIX) {
                em_data->GPS_satQuality = 0;
            }
        } else if(GPS_DATA.status == 6) {
            SetErrorState(GPS_ESTIMATED);
            em_data->GPS_satQuality = GPS_DATA.status;
        } else {
            UnsetErrorState(GPS_NO_FIX);
            UnsetErrorState(GPS_ESTIMATED);
            em_data->GPS_satQuality = GPS_DATA.status;
        }

        em_data->GPS_time = GPS_DATA.fix.time;
        em_data->GPS_satsUsed = GPS_DATA.satellites_used;
        em_data->GPS_latitude = GPS_DATA.fix.latitude;
        em_data->GPS_longitude = GPS_DATA.fix.longitude;
        em_data->GPS_speed = GPS_DATA.fix.speed * MPS_TO_KNOTS;
        em_data->GPS_heading = GPS_DATA.fix.track;
        em_data->GPS_hdop = GPS_DATA.dop.hdop;
        em_data->GPS_eph = EMIX(GPS_DATA.fix.epx, GPS_DATA.fix.epy);

        last_online = GPS_DATA.online;
        /*last_latitude = GPS_DATA.fix.latitude; // HACKS to handle GPRMC-only GPSs and detecting fix status given that GPSD doesn't behave nicely via SHM
        last_longitude = GPS_DATA.fix.longitude;
        last_speed = GPS_DATA.fix.speed;*/
        last_time = GPS_DATA.fix.time;
    }

    //D("GPS_DATA.set          = " << GPS_DATA.set);
    D("GPS_DATA.online       = " << GPS_DATA.online);
    D("GPS_DATA.fix.time     = " << GPS_DATA.fix.time);
    D("GPS_DATA.fix.mode     = " << GPS_DATA.fix.mode);
    D("GPS_DATA.status       = " << GPS_DATA.status);
    D("GPS_DATA.fix.latitude = " << GPS_DATA.fix.latitude);
    D("GPS_DATA.fix.longitude = " << GPS_DATA.fix.longitude);
    //D("GPS_DATA.sats_visible = " << GPS_DATA.satellites_visible);
    //D("GPS_DATA.sats_used    = " << GPS_DATA.satellites_used);
    //D("GPS_DATA.tag          = " << GPS_DATA.tag);
    //D("GPS_DATA.dop.hdop = " << GPS_DATA.dop.hdop);
    //D("GPS_DATA.eph = " << EMIX(GPS_DATA.fix.epx, GPS_DATA.fix.epy));
    //D("GPS_DATA.fix.epx = " << GPS_DATA.fix.epx);
    //D("GPS_DATA.fix.epy = " << GPS_DATA.fix.epy);
    return 0;
}

void GPSSensor::Close() {
    if(GPS_DATA.privdata != NULL && PRIVATE(GPS_DATA)->shmseg != NULL) {
        D("gps_close() on shutdown");
        gps_close(&GPS_DATA);
    }
}

unsigned short GPSSensor::LoadKML(char *file, POINT polygons[MAX_POLYS][MAX_POINTS], unsigned short *num_edges) {
    unsigned short num_polys_found = 0;
    size_t s_index = 0, e_index = 0;
    char tmp_c;
    double x, y;
    stringstream buffer;
    string contents;

    D("Loading KML file " << file);
    
    ifstream in(file);
    if(in.fail()) {
        cerr << "ERROR: Couldn't load KML file " << file << endl;
        return 0;
    }

    buffer << in.rdbuf();
    in.close();
    contents = buffer.str();

    while(true) {
        stringstream coordinates;

        if((s_index = contents.find("<coordinates>", s_index)) == string::npos) {
            D("KML - didn't find any more <coordinates>, breaking loop");
            break;
        }

        s_index += 13; // get rid of <coordinates>

        if((e_index = contents.find("</coordinates>", s_index)) == string::npos) {
            D("KML - didn't find matching </coordinates>");
            break;
        }

        if(s_index > e_index) {
            D("KML - malformed data, closing </coordinates> with no matching opening");
            break;
        }

        coordinates << contents.substr(s_index, e_index - s_index);
        //D("Working with this data:" << endl << contents.substr(s_index, e_index - s_index));

        num_edges[num_polys_found] = 0;
        while(coordinates >> x >> tmp_c >> y) {
            polygons[num_polys_found][num_edges[num_polys_found]].x = x;
            polygons[num_polys_found][num_edges[num_polys_found]].y = y;
            num_edges[num_polys_found]++;
            D("Found x,y = " << x << ", " << y << " -- now have " << num_edges[num_polys_found] << " edges");

            // there can be an optional ,altitude value in <coordinates>; eat it if necessary
            if(coordinates.peek() == ',') {
                coordinates >> tmp_c >> x;
            }
        }

        num_polys_found++;
    }

    cout << "GPS: Loaded " << num_polys_found << " polygons from " << file << endl;

    return num_polys_found;
}

// Copyright 2000 softSurfer, 2012 Dan Sunday
// This code may be freely used and modified for any purpose
// providing that this copyright notice is included with it.
// SoftSurfer makes no warranty for this code, and cannot be held
// liable for any real or imagined damage resulting from its use.
// Users of this code must verify correctness for their application.

// tests if a point is Left|On|Right of an infinite line.
//    Input:  three points P0, P1, and P2
//    Return: >0 for P2 left of the line through P0 and P1
//            =0 for P2  on the line
//            <0 for P2  right of the line
//    See: Algorithm 1 "Area of Triangles and Polygons"
inline double IsLeft(const POINT &P0, const POINT &P1, const POINT &P2) {
    return ((P1.x - P0.x) * (P2.y - P0.y) - (P2.x -  P0.x) * (P1.y - P0.y));
}

// winding number test for a point in a polygon
//      Input:   P = a point,
//               V[] = vertex points of a polygon V[n+1] with V[n]=V[0]
//      Return:  wn = the winding number (=0 only when P is outside)
short GPSSensor::IsPointInsidePoly(const POINT &P, const POINT *V, unsigned short n) {
    short wn = 0; // the  winding number counter

    // loop through all edges of the polygon
    for(int i = 0; i < n; i++) {   // edge from V[i] to  V[i+1]
        if(V[i].y <= P.y) {          // start y <= P.y
            if(V[i + 1].y > P.y) {      // an upward crossing
                if (IsLeft(V[i], V[i + 1], P) > 0) { // P left of  edge
                    ++wn;            // have  a valid up intersect
                }
            }
        } else {                        // start y > P.y (no test needed)
            if(V[i + 1].y <= P.y) {     // a downward crossing
                if(IsLeft(V[i], V[i + 1], P) < 0) { // P right of edge
                    --wn; //have a valid down intersect
                }
            }
        }
    }

    return wn;
}

int GPSSensor::CheckSpecialAreas() {
    int encodingState = STATE_ENCODING_RUNNING;

    UnsetErrorState(GPS_INSIDE_HOME_PORT);
    UnsetErrorState(GPS_INSIDE_FERRY_LANE);

    if(*state & GPS_NO_FIX || *state & GPS_NO_DATA) {
        D("Not bothering to check poly because GPS data isn't useful");
    } else {
        if(!(*state & GPS_NO_HOME_PORT_DATA)) {
            for (unsigned int k = 0; k < num_home_ports; k++) {
                D("Checking if in home port " << k);
                if (IsPointInsidePoly((POINT){em_data->GPS_longitude, em_data->GPS_latitude}, home_ports[k], num_home_port_edges[k] - 1)) {
                    D("IN SPECIAL AREA: home port");
                    SetErrorState(GPS_INSIDE_HOME_PORT);
                    encodingState = STATE_ENCODING_PAUSED;
                    break;
                }
            }
        }

        if(!(*state & GPS_NO_FERRY_DATA)) {
            for (unsigned int k = 0; k < num_ferry_lanes; k++) {
                D("Checking if in ferry lane " << k);
                if (IsPointInsidePoly((POINT){em_data->GPS_longitude, em_data->GPS_latitude}, ferry_lanes[k], num_ferry_lane_edges[k] - 1)) {
                    D("IN SPECIAL AREA: ferry lane");
                    SetErrorState(GPS_INSIDE_FERRY_LANE);
                    break;
                }
            }
        }
    }

    return encodingState;
}
