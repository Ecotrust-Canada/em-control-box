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

#include <cstdio>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include "ADSensor.h"

using namespace std;

ADSensor::ADSensor(EM_DATA_TYPE* _em_data):Sensor("AD", &_em_data->AD_state, AD_NO_CONNECTION, AD_NO_DATA) {
    em_data = _em_data;
}

int ADSensor::Connect() {
    if(battery_scale == 0) SetADCType();
    return Sensor::Connect();
}

void ADSensor::SetADCType() {
    char divider = '\0';
    psi_vmin = 0;
    psi_vmax = (double)PSI_VOLT_MAX;

    sscanf(CONFIG.arduino, "%lfV%c", &psi_input_vmax, &divider);
    sscanf(CONFIG.psi_vmin, "%lf", &psi_vmin);

    if(psi_input_vmax == 3.3) {
        battery_scale = 3.3/5.0;
        cout << "AD: Setting 3.3V Arduino ";
    } else if(psi_input_vmax == 5.0) {
        battery_scale = 1.0;
        cout << "AD: Setting 5.0V Arduino ";
    } else {
        cerr << "ERROR: Arduino configuration bad; please check /etc/em.conf" << endl;
        exit(-1);
    }

    if(!divider) {
        cout << "WITHOUT voltage divider" << endl;
    } else if(divider == 'D') {
        psi_vmin /= 2;
        psi_vmax /= 2;
        cout << "WITH voltage divider" << endl;
    }
}

int ADSensor::Receive() {
    char AD_BUF[AD_BUF_SIZE] = { '\0' };

    int bytesRead = Sensor::Receive(AD_BUF, AD_BYTES_MIN, AD_BUF_SIZE);

    if(bytesRead >= AD_BYTES_MIN) {
        char *ch = &AD_BUF[0];
        
        // eat bytes until AD_START_BYTE (:) or no more buffer left
        while(*ch != AD_START_BYTE && ch < &AD_BUF[bytesRead]) ch++;

        if(*ch == AD_START_BYTE) {
            float avgRawPSI = 0.0, avgRawBat = 0.0;
            int p = 0, b = 0;

            // go through buffer and collect total sum of raw PSI and Bat values
            while(ch < &AD_BUF[bytesRead]) {
                int rawVal = 0;

                switch(*++ch) {
                    case 'P': sscanf(++ch, "%d:", &rawVal); avgRawPSI += rawVal; p++; break;
                    case 'B': sscanf(++ch, "%d:", &rawVal); avgRawBat += rawVal; b++; break;
                }

                while (rawVal) {
                    rawVal /= 10;
                    ch++;
                }
            }

            // if we collected any PSI values
            if(p >= 1) {
                if(p > 1) avgRawPSI /= p; // get average

                if(avgRawPSI == 0) {
                    em_data->AD_psi = 0;
                    SetErrorState(AD_PSI_LOW_OR_ZERO);
                } else if(avgRawPSI > 0) {
                    double rawVolts = avgRawPSI / PSI_RAW_MAX * psi_input_vmax;

                    UnsetErrorState(AD_PSI_LOW_OR_ZERO);
                    if(rawVolts < psi_vmin) {
                        em_data->AD_psi = 0;
                        if(rawVolts < psi_vmin * 0.9) SetErrorState(AD_PSI_LOW_OR_ZERO);
                    } else {
                        em_data->AD_psi = (rawVolts - psi_vmin) / (psi_vmax - psi_vmin) * PSI_MAX; // remove the 1V base signal
                    }
                }
            }

            // if we collected any bat values
            if(b >= 1) {
                if(b > 1) avgRawBat /= b; 

                avgRawBat *= battery_scale;

                if(avgRawBat == 0) {
                    em_data->AD_battery = 0;
                } else if(avgRawBat > 0) {
                    em_data->AD_battery = avgRawBat * (BATTERY_MAX / BATTERY_RAW_MAX);
                    if(em_data->AD_battery <= BATTERY_LOW_THRESH) {
                        SetErrorState(AD_BATTERY_LOW);
                        UnsetErrorState(AD_BATTERY_HIGH);
                    } else if(em_data->AD_battery >= BATTERY_HIGH_THRESH) {
                        SetErrorState(AD_BATTERY_HIGH);
                        UnsetErrorState(AD_BATTERY_LOW);
                    } else {
                        UnsetErrorState(AD_BATTERY_LOW);
                        UnsetErrorState(AD_BATTERY_HIGH);
                    }
                }
            }
        }

        return bytesRead;
    }

    return -1;
}

void ADSensor::HonkMyHorn() {
    switch(em_data->AD_honkSound) {
        case HONK_SCAN_SUCCESS:
            Send("F600D500E\n");
            break;

        case HONK_SCAN_DUPLICATE:
            Send("F600D300E\n"); usleep(400000); Send("F600D300E\n");
            break;

        case HONK_SCAN_WARNING:
            Send("F300D500E\n");
            break;
    }

    em_data->AD_honkSound = 0;
    em_data->AD_lastHonkIteration = em_data->iterationTimer;
}