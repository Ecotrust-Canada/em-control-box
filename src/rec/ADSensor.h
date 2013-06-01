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

#ifndef AD_SENSOR_H
#define AD_SENSOR_H
#include "Sensor.h"
#include "em-rec.h"

/**
 * Buffer for AD microcontroller messages
 */
#define AD_BUF_SIZE		2048
#define AD_BYTES_MIN	5
#define AD_START_BYTE	':'

#define PSI_MAX			2500.0
#define PSI_RAW_MAX		1023
#define PSI_VOLT_MAX	5.0

#define BATTERY_MAX		11.73
#define BATTERY_RAW_MAX	460

#define BATTERY_LOW_THRESH 11
#define BATTERY_HIGH_THRESH 14

/**
 * @class ADSensor
 * @brief A subclass of the Sensor class for the AD sensor.
 *
 */
class ADSensor: public Sensor {
	private:
        EM_DATA_TYPE *em_data;
		void SetADCType();
		double psi_input_vmax;
		double psi_vmin;
		double psi_vmax;
		double battery_scale;

    public:
        ADSensor(EM_DATA_TYPE*);
        int Connect();
        int Receive();
        void HonkMyHorn();
};

#endif