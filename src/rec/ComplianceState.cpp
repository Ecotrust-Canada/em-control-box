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
#include <fstream>
#include <sstream>
#include <cmath>
#include "ComplianceState.h"
using namespace std;

const double ComplianceSensor::EPS = 1e-8;

ComplianceSensor::ComplianceSensor():Sensor("COMPLIANCE") {
	num_edgepoints_ferry_area = 13;
	num_areas = 0;
}

void ComplianceSensor::SetFishingArea(const char aFishingArea) {
	fishingArea = aFishingArea;
}

int ComplianceSensor::Connect() {
	static bool silenceConnection = false;
	if (!silenceConnection) {
		cout << "INFO:Initializing Compliance sensor" << endl;
		silenceConnection = true;
	}

	UnsetAllErrorStates();

	LoadFerryArea();
	LoadHomePorts();

	return 0;
}

void ComplianceSensor::LoadFerryArea() {
    ifstream in("/opt/em/static/ferry_area.kml");
    if(in.fail()) {
        cerr << "ERROR:Can't open /opt/em/static/ferry_area.kml" << endl;
        SetErrorState(COMPLIANCE_NO_FERRY_AREA_DATA);
        return;
    }

    double x, y;
    char c;
    stringstream buffer;
    stringstream coordinates;
    
    buffer << in.rdbuf();
    string contents = buffer.str();
    
    size_t s_index = contents.find("<coordinates>");
    s_index += 13; //Get rid of <coordinates>
    size_t e_index = contents.find("</coordinates>");

    if (s_index == string::npos || e_index == string::npos || s_index > e_index) {
        cerr << "Invalid ferry areas kml file" << endl;
        SetErrorState(COMPLIANCE_NO_FERRY_AREA_DATA);
        return;
    }

    coordinates << contents.substr(s_index, e_index - s_index);
    
    num_edgepoints_ferry_area = 0;
    while(coordinates>> x>> c>> y) {
        ferry_edgepoints[num_edgepoints_ferry_area].x = x;
        ferry_edgepoints[num_edgepoints_ferry_area].y = y;
        num_edgepoints_ferry_area ++;
    }

    in.close();

    cout << "INFO:Loaded ferry areas" << endl;
    UnsetErrorState(COMPLIANCE_NO_FERRY_AREA_DATA);
}

void ComplianceSensor::LoadHomePorts() {
    ifstream in("/opt/em/static/home_ports.kml");
    if(in.fail()) {
        cerr << "ERROR:Can't open /opt/em/static/home_ports.kml" << endl;
        SetErrorState(COMPLIANCE_NO_HOMEPORT_DATA);
        return;
    }

    double x, y;
    char c;
    stringstream buffer;
    
    buffer << in.rdbuf();
    string contents = buffer.str();
    
    //parse the first homeport.
    size_t s_index = 0, e_index = 0;

    num_areas = 0;

    while(true) {
        s_index = contents.find("<coordinates>", e_index);
        if(s_index == string::npos || s_index < e_index)
            break;
        s_index += 13; //Get rid of <coordinates>
        e_index = contents.find("</coordinates>", s_index);
        num_points[num_areas] = 0;
        stringstream coordinates;
        coordinates << contents.substr(s_index, e_index - s_index);
    
        while(coordinates>> x>> c>> y) {
            home_ports[num_areas][num_points[num_areas]].x = x;
            home_ports[num_areas][num_points[num_areas]].y = y;
            num_points[num_areas] ++;
        }

        num_areas ++;
    }

    if (num_areas == 0) {
        cerr << "Invalid home ports kml file" << endl;
        SetErrorState(COMPLIANCE_NO_HOMEPORT_DATA);
    }

    in.close();

    cout << "INFO:Loaded home ports" << endl;
    UnsetErrorState(COMPLIANCE_NO_HOMEPORT_DATA);
}

int ComplianceSensor::Receive() {
    // do we need this?
    //cout << "MSG:GPS:HEADING="<< heading<< ",SPEED="<< speed<< endl;

    //check if the vessel is inside any of the ferry area.
    /*if(PointInsidePolygon(VESSEL_POS, num_edgepoints_ferry_area, ferry_edgepoints)) {
        SetErrorState(COMPLIANCE_INSIDE_FERRY_LANE);
    } else {
        UnsetErrorState(COMPLIANCE_INSIDE_FERRY_LANE);
    }

    UnsetErrorState(COMPLIANCE_NEAR_HOMEPORT);
    //check if the vessel if near any of the homeports.
    for (unsigned int k = 0; k < num_areas; k++) {
        if (PointInsidePolygon(VESSEL_POS, num_points[k], home_ports[k])) {
            SetErrorState(COMPLIANCE_NEAR_HOMEPORT);
            break;
        }
    }*/

    return 0;
}

/**
 * Checkes whether the given point is inside the given polygon,
 * which is given by edge points in clockwise or counter-clockwise order.
 * onEdge denote the return value when the point is on the edge.
 */
bool ComplianceSensor::PointInsidePolygon(const POINT & q, unsigned int n, const POINT * p, bool onEdge) {
    double ang = 0.0, x1, y1, x2, y2, dprod, cprod, t, tmp;
    for (unsigned int i = 0; i < n-1; ++i) {
        x1 = p[i].x - q.x;
        y1 = p[i].y - q.y;
        x2 = p[i + 1].x - q.x;
        y2 = p[i + 1].y - q.y;
        
        if (fabs(x1) + fabs(y1) < EPS || fabs(x2) + fabs(y2) < EPS) {
            return onEdge;
        }
        
        dprod = x1 * x2 + y1 * y2;
        cprod = x1 * y2 - x2 * y1;
        tmp = sqrt(x1*x1 + y1*y1) * sqrt(x2*x2 + y2*y2);
        
        if (tmp < EPS) return false;
        
        t = acos(dprod / tmp);

        if (fabs(cprod) < EPS && t > EPS) {
            return onEdge;
        }

        if (cprod < -EPS) {
            t = -t;
        }

        ang += t;
    }
    return fabs(fabs(ang) - M_PI * 2.0) < EPS;
}