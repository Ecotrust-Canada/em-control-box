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

#ifndef GEOLIB_H_
#define GEOLIB_H_

#include <math.h>
#include <sys/time.h>
#include <time.h>
#define LL_UNDEF 999

#define GEO_NO_SAT_IN_VIEW 1
#define GEO_NO_SAT_IN_USE 2
#define GEO_NO_CONNECTION 4
#define GEO_ESTIMATED 8
#define GEO_BLANK_OUTPUT 16



int parseGPS(char* buf, double &lat, double &lng, double &heading, double &speed);

/**
 * Find distance in meters between two points on the earth.
 * @param p1 first coordinate pair.
 * @param p2 second coordinate pair.
 * @return meters distance.
 */
double ll2distance(POINT p1, POINT p2);

/**
 * Find angle in degrees between two points on the earth.
 * @param p1 first coordinate pair.
 * @param p2 second coordinate pair.
 * @return angle in degrees.
 */
double ll2heading(POINT p1, POINT p2);

#endif