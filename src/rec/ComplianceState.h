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

#ifndef COMPLIANCE_SENSOR_H
#define COMPLIANCE_SENSOR_H
#include "Sensor.h"

/**
 * @class ComplianceSensor
 * @brief A subclass of the Sensor class for the Compliance "sensor"
 *
 */
class ComplianceSensor: public Sensor {
	protected:
        char fishingArea;
        typedef struct { double x, y; } POINT;
        static const double EPS;

		unsigned int 	num_edgepoints_ferry_area,
						num_areas,
						num_points[10];

		POINT 	ferry_edgepoints[20],
				home_ports[10][10];

		void LoadFerryArea();
        void LoadHomePorts();
        bool PointInsidePolygon(const POINT &, unsigned int, const POINT *, bool = true);

    public:
        ComplianceSensor();
        void SetFishingArea(const char);
        int Connect();
        int Receive();
};

#endif