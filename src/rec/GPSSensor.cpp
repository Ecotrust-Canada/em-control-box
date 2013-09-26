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
#include <iomanip> // for setw
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <pthread.h>

using namespace std;

GPSSensor::GPSSensor(EM_DATA_TYPE* _em_data):Sensor("GPS", &_em_data->GPS_state, GPS_NO_CONNECTION, GPS_NO_DATA) {
    em_data = _em_data;
    smThread.SetState(STATE_NOT_RUNNING);
    pthread_mutex_init(&mtx_GPS_DATA_buf, NULL);
}

GPSSensor::~GPSSensor() {
    pthread_mutex_destroy(&mtx_GPS_DATA_buf);
}

// lacking mutexes on GPS_DATA but it shouldn't matter as Connect() is only ever called
// in the main thread context, and never when the consumer thread is running
int GPSSensor::Connect() {
    static bool silenceConnectErrors = false;
    
    if (gps_open("localhost", DEFAULT_GPSD_PORT, &GPS_DATA) != 0) {
        if(!silenceConnectErrors || OVERRIDE_SILENCE) {
            cerr << "GPS: Failed to connect to GPSd (is it running?); will keep at it but silencing further Connect() errors" << endl;
            silenceConnectErrors = true;
        }

        SetState(GPS_NO_CONNECTION);
        GPS_DATA = GPS_DATA_buf = GPS_DATA_empty;
        
        return -1;
    }
    
    (void) gps_stream(&GPS_DATA, WATCH_ENABLE | WATCH_JSON, NULL);

    UnsetState(GPS_NO_CONNECTION);
    UnsetState(GPS_NO_DATA);
    UnsetState(GPS_NO_FIX);

    cout << name << ": Connected (GPSd)" << endl;

    // bug: condition of having a KML file with NO polys = reread every time
    if(!(GetState() & GPS_NO_HOME_PORT_DATA) && num_home_ports == 0) {
        if((num_home_ports = LoadKML(em_data->GPS_homePortDataFile, home_ports, num_home_port_edges)) == 0) {
            SetState(GPS_NO_HOME_PORT_DATA);
        }
    }

    if(!(GetState() & GPS_NO_FERRY_DATA) && num_ferry_lanes == 0) {
        if((num_ferry_lanes = LoadKML(em_data->GPS_ferryDataFile, ferry_lanes, num_ferry_lane_edges)) == 0) {
            SetState(GPS_NO_FERRY_DATA);
        }
    }

    return 0;
}

int GPSSensor::Receive() {
    static int retVal;

    // whenever the thread self-closes (sets THREAD_CLOSING), we do a full
    // reconnection to GPSd, because we don't know (or care) why it closed
    // but we know something is wrong
    switch(smThread.GetState()) {
        // thread should always set this when it wants to exit
        case STATE_CLOSING_OR_UNDEFINED:
            // there's no guarantee this will get run if the program is
            // halted (CTRL+C) so it is done again by em-rec after the record loop
            Close();

        // the thread should NOT be running past this point
        ///////////////////////////////////////////////////

        // the thread should NEVER set this itself; Close() does this
        case STATE_NOT_RUNNING:
            if(__EM_RUNNING) {
                if (!isConnected() && Connect() != 0) {
                    D("Tried to GPSSensor::Receive() while not connected and Connect() failed");
                    return -1;
                }

                smThread.SetState(STATE_RUNNING);
                if((retVal = pthread_create(&pt_receiveLoop, NULL, &thr_ReceiveLoopLauncher, (void*)this)) != 0) {
                    smThread.SetState(STATE_NOT_RUNNING);
                }
                
                D("pthread_create(): " << retVal);
            }

        case THREAD_RUNNING:
            if (isConnected() /*&& smThread.GetState() & STATE_RUNNING*/) {
                pthread_mutex_lock(&mtx_GPS_DATA_buf);
                pthread_mutex_lock(&(em_data->mtx));
                    em_data->GPS_time = GPS_DATA_buf.fix.time;
                    em_data->GPS_latitude = isnan(GPS_DATA_buf.fix.latitude) ? 0 : GPS_DATA_buf.fix.latitude;
                    em_data->GPS_longitude = isnan(GPS_DATA_buf.fix.longitude) ? 0 : GPS_DATA_buf.fix.longitude;
                    em_data->GPS_heading = isnan(GPS_DATA_buf.fix.track) ? 0 : GPS_DATA_buf.fix.track;
                    em_data->GPS_speed = isnan(GPS_DATA_buf.fix.speed) ? 0 : GPS_DATA_buf.fix.speed * MPS_TO_KNOTS;
                    em_data->GPS_satQuality = GPS_DATA_buf.status;
                    em_data->GPS_satsUsed = GPS_DATA_buf.satellites_used;
                    em_data->GPS_hdop = isnan(GPS_DATA_buf.dop.hdop) ? 0 : GPS_DATA_buf.dop.hdop;
                    em_data->GPS_eph = isnan(GPS_DATA_buf.fix.epx) && isnan(GPS_DATA_buf.fix.epy) ? 0 : EMIX(GPS_DATA_buf.fix.epx, GPS_DATA_buf.fix.epy);
                pthread_mutex_unlock(&(em_data->mtx));
                pthread_mutex_unlock(&mtx_GPS_DATA_buf);
            }

            break;
    }

    return 0;
}

void *GPSSensor::thr_ReceiveLoopLauncher(void *self) {
    ((GPSSensor*)self)->thr_ReceiveLoop();
    return NULL;
}

// assumes we are Connected()
void GPSSensor::thr_ReceiveLoop() {
    unsigned short threadCloseDelayCounter = 0;

    cout << name << ": Consumer thread running" << endl;

    while(smThread.GetState() & STATE_RUNNING && __EM_RUNNING) {
        if (isConnected()) {
            if(gps_waiting(&GPS_DATA, POLL_PERIOD / 10)) {
                if(!gps_read(&GPS_DATA)) {
                    D("gps_read() didn't succeed, closing connection");
                    SetState(GPS_NO_CONNECTION); // want to force a reconnect

                    smThread.SetState(STATE_CLOSING_OR_UNDEFINED);
                    pthread_exit(NULL);
                } else {                        
                    // this stuff processed with every run of the read loop ONLY when there was data
                    if(GPS_DATA.online != GPS_DATA_buf.online) {
                        D("Unset GPS_NO_DATA");
                        UnsetState(GPS_NO_DATA);
                    } else {
                        D("GPS data read, no change, setting GPS_NO_DATA");
                        SetState(GPS_NO_DATA, GPS_STATE_DELAY);
                    }
                }
            } else {
                D("No GPS data to read, setting delayed GPS_NO_DATA");
                SetState(GPS_NO_DATA, GPS_STATE_DELAY); // after 300000?
            }

            cout << GPS_DATA.status << "\t" << GPS_DATA.fix.mode << "\t" << GPS_DATA.online << "\t" << setw(10) << GPS_DATA.fix.time << "\t" << setw(10) << GPS_DATA.skyview_time << "\t" << GPS_DATA.satellites_used << "\t" << GPS_DATA.satellites_visible << "\t" << GPS_DATA.fix.epx << "\t" << setw(12) << GPS_DATA.fix.latitude << "\t" << setw(12) << GPS_DATA.fix.longitude << "\t" << GPS_DATA.tag << "\t";

            if (GPS_DATA.set & ONLINE_SET) cout << " ONLINE";
            if (GPS_DATA.set & TIME_SET) cout << ",TIME";
            if (GPS_DATA.set & LATLON_SET) cout << ",LATLON";
            if (GPS_DATA.set & ALTITUDE_SET) cout << ",ALTITUDE";
            if (GPS_DATA.set & SPEED_SET) cout << ",SPEED";
            if (GPS_DATA.set & TRACK_SET) cout << ",TRACK";
            if (GPS_DATA.set & CLIMB_SET) cout << ",CLIMB";
            if (GPS_DATA.set & STATUS_SET) cout << ",STATUS";
            if (GPS_DATA.set & MODE_SET) cout << ",MODE";
            if (GPS_DATA.set & DOP_SET) cout << ",DOP";
            if (GPS_DATA.set & HERR_SET) cout << ",HERR";
            if (GPS_DATA.set & VERR_SET) cout << ",VERR";
            if (GPS_DATA.set & VERSION_SET) cout << ",VERSION";
            if (GPS_DATA.set & POLICY_SET) cout << ",POLICY";
            if (GPS_DATA.set & SATELLITE_SET) cout << ",SATELLITE";
            if (GPS_DATA.set & DEVICE_SET) cout << ",DEVICE";
            cout << endl;
            // this stuff processed every run of read loop even when no data

            // check for NO_FIX, using the satellites_used count is the most consistent and easiest
            if(GPS_DATA.satellites_used == 0) {
                // hack for old-style bean-shaped GPSs that are configured to send only GPRMC (Area A)
                // these guys cause GPSd to report 0 satellites_used even though they have a lock, so
                // we unset GPS_NO_FIX if LATLON_SET is set in these cases; this hack is enabled by setting
                // fishing_area = A in em.conf
                // the additional conditions of HERR & VERR NOT being set are to prevent this hack from
                // being triggered when in Area A when we have a PROPERLY configured GPS (or a newer 17x);
                // if properly configured or newer they set VERR and HERR
                if(__GPRMC &&
                   GPS_DATA.set & LATLON_SET &&
                   !(GPS_DATA.set & HERR_SET) && !(GPS_DATA.set & VERR_SET)) {
                    UnsetState(GPS_NO_FIX);
                } else {
                    SetState(GPS_NO_FIX, GPS_STATE_DELAY);
                }
            } else {
                UnsetState(GPS_NO_FIX);
            }

            if(GPS_DATA.status == 6) {
                UnsetState(GPS_NO_FIX);
                SetState(GPS_ESTIMATED, GPS_STATE_DELAY);
            } else {
                UnsetState(GPS_ESTIMATED);
            }

            if(GetState() & GPS_NO_FIX || GetState() & GPS_NO_DATA) {
                gps_clear_fix(&(GPS_DATA.fix));
                
                pthread_mutex_lock(&mtx_GPS_DATA_buf);
                    gps_clear_fix(&(GPS_DATA_buf.fix));
                pthread_mutex_unlock(&mtx_GPS_DATA_buf);

                GPS_DATA.status = 0;
                GPS_DATA.satellites_used = 0;

                threadCloseDelayCounter++;
                D("TCC = " << threadCloseDelayCounter);
            } else {
                threadCloseDelayCounter = 0;
            }

            pthread_mutex_lock(&mtx_GPS_DATA_buf);
                memcpy(&GPS_DATA_buf, &GPS_DATA, sizeof(GPS_DATA));
            pthread_mutex_unlock(&mtx_GPS_DATA_buf);

            if(threadCloseDelayCounter >= GPS_THREAD_CLOSE_DELAY) {
                D("Thread close counter reached");    

                smThread.SetState(STATE_CLOSING_OR_UNDEFINED);
                pthread_exit(NULL);
            }
        } else {
            D("Receive() while NOT connected");
        }

        usleep(POLL_PERIOD / 100); // slow us down a bit

        pthread_mutex_lock(&(em_data->mtx));
            _runState = em_data->runState;
        pthread_mutex_unlock(&(em_data->mtx));
    }

    D("Broke out of thread's receive loop");
    smThread.SetState(STATE_CLOSING_OR_UNDEFINED);
    pthread_exit(NULL);
}

void GPSSensor::Close() {
    D("GPSSensor::Close()");
    
    // only delay if main program shutting down
    if (!(__EM_RUNNING)) {
        usleep(POLL_PERIOD); // wait for a bit to let the consumer thread pick up on main program loop being set THREAD_NOT_RUNNING
    } // by now consumer thread should be waiting for a join


    // by now the thread should be in THREAD_CLOSING
    if(smThread.GetState() & STATE_CLOSING_OR_UNDEFINED) {
        if(pthread_join(pt_receiveLoop, NULL) == 0) {
            cout << name << ": Consumer thread stopped" << endl;
        }

        // thread is gone, no need for mutexes
        smThread.SetState(STATE_NOT_RUNNING);
    }
    
    gps_close(&GPS_DATA);
    GPS_DATA = GPS_DATA_buf = GPS_DATA_empty;

    SetState(GPS_NO_CONNECTION);
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

    UnsetState(GPS_INSIDE_HOME_PORT);
    UnsetState(GPS_INSIDE_FERRY_LANE);

    if(GetState() & GPS_NO_FIX || GetState() & GPS_NO_DATA) {
        D("Not bothering to check poly because GPS data isn't useful");
    } else {
        if(!(GetState() & GPS_NO_HOME_PORT_DATA)) {
            for (unsigned int k = 0; k < num_home_ports; k++) {
                D("Checking if in home port " << k);
                if (IsPointInsidePoly((POINT){em_data->GPS_longitude, em_data->GPS_latitude}, home_ports[k], num_home_port_edges[k] - 1)) {
                    D("IN SPECIAL AREA: home port");
                    SetState(GPS_INSIDE_HOME_PORT);
                    encodingState = STATE_ENCODING_PAUSED;
                    break;
                }
            }
        }

        if(!(GetState() & GPS_NO_FERRY_DATA)) {
            for (unsigned int k = 0; k < num_ferry_lanes; k++) {
                D("Checking if in ferry lane " << k);
                if (IsPointInsidePoly((POINT){em_data->GPS_longitude, em_data->GPS_latitude}, ferry_lanes[k], num_ferry_lane_edges[k] - 1)) {
                    D("IN SPECIAL AREA: ferry lane");
                    SetState(GPS_INSIDE_FERRY_LANE);
                    break;
                }
            }
        }
    }

    return encodingState;
}
