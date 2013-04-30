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

#define INIT_REC

#include <cstdlib>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <list>
#include <fstream>
#include <blkid/blkid.h>

#include "em-rec.h"
#include "libgpsmm.h"
#include "../config.h"
#include "../util.h"
#include "md5.h"
#include "GPSSensor.h"
#include "ADSensor.h"
#include "RFIDSensor.h"
//#include "ComplianceSensor.h"

using namespace std;

CONFIG_TYPE CONFIG = {};
EM_DATA_TYPE EM_DATA = {};

GPSSensor iGPSSensor(&EM_DATA.GPS_state);
RFIDSensor iRFIDSensor(&EM_DATA.RFID_state);
ADSensor iADSensor(&EM_DATA.AD_state);
//ComplianceSensor iComplianceSensor;

struct stat st, parent_st;

/**
 * Program entry point.
 */
int main(int argc, char *argv[]) {
    struct blkid_struct_cache *blkid_cache;

    cout << "INFO: Ecotrust EM Recorder v" << VERSION << " is starting" << endl;

    if (readConfigFile()) {
        cerr << "ERROR: Failed to get configuration" << endl;
        exit(-1);
    }

    strcpy(CONFIG.vessel, getConfig("vessel"));
    strcpy(CONFIG.vrn, getConfig("vrn"));
    strcpy(CONFIG.arduino, getConfig("arduino"));
    strcpy(CONFIG.fishing_area, getConfig("fishing_area"));
    strcpy(CONFIG.psi_vmin, getConfig("psi_vmin"));
    strcpy(CONFIG.DATA_MNT, getConfig("DATA_MNT"));
    strcpy(CONFIG.BACKUP_DATA_MNT, getConfig("BACKUP_DATA_MNT"));
    strcpy(CONFIG.JSON_STATE_FILE, getConfig("JSON_STATE_FILE"));
    strcpy(CONFIG.ARDUINO_DEV, getConfig("ARDUINO_DEV"));
    strcpy(CONFIG.GPS_DEV, getConfig("GPS_DEV"));
    strcpy(CONFIG.RFID_DEV, getConfig("RFID_DEV"));
    strcpy(CONFIG.SENSOR_DATA, getConfig("SENSOR_DATA"));
    strcpy(CONFIG.RFID_DATA, getConfig("RFID_DATA"));

    EM_DATA.SENSOR_DATA_files.push_back(string(CONFIG.BACKUP_DATA_MNT) + "/" + CONFIG.SENSOR_DATA);
    EM_DATA.RFID_DATA_files.push_back(string(CONFIG.BACKUP_DATA_MNT) + "/" + CONFIG.RFID_DATA);

    // check /mnt/data's device ID and compare it to its parent /mnt
    stat((string(CONFIG.DATA_MNT) + "/../").c_str(), &parent_st);
    if(stat(CONFIG.DATA_MNT, &st) == 0 && st.st_dev != parent_st.st_dev) {
        blkid_get_cache(&blkid_cache, NULL);
        EM_DATA.dataDriveLabel = blkid_get_tag_value(blkid_cache, "LABEL", blkid_devno_to_devname(st.st_dev));

        cout << "INFO: Data drive PRESENT, label = " << EM_DATA.dataDriveLabel << endl;
        EM_DATA.dataDrivePresent = true;
        
        EM_DATA.SENSOR_DATA_files.push_back(string(CONFIG.DATA_MNT) + "/" + CONFIG.SENSOR_DATA);
        EM_DATA.RFID_DATA_files.push_back(string(CONFIG.DATA_MNT) + "/" + CONFIG.RFID_DATA);
    } else {
        cout << "INFO: Data drive NOT PRESENT" << endl;
        EM_DATA.dataDrivePresent = false;
    }

    iADSensor.SetPort(CONFIG.ARDUINO_DEV);
    iADSensor.SetBaudRate(B9600);

    iRFIDSensor.SetPort(CONFIG.RFID_DEV);
    iRFIDSensor.SetBaudRate(B9600);

    iADSensor.Connect();
    iRFIDSensor.Connect();
    iGPSSensor.Connect();
    //iComplianceSensor.Connect();

    recordLoop();

    // never reached
    return 0;
}

/**
 * Recording loop.
 */
void recordLoop() {
    struct timeval tv;
    unsigned long long int tstart = 0, tdiff = 0;
    char DATE_BUF[24] = { '\0' };
    time_t rawtime;
    struct tm *timeinfo;

    cout << "INFO: Entered recordLoop()" << endl;

    while(1) {
        gettimeofday(&tv, NULL);
        tstart = tv.tv_sec * 1000000 + tv.tv_usec;

        // get date
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(DATE_BUF, 24, "%Y-%m-%d %H:%M:%S", timeinfo); // writes null-terminated string so okay not to clear DATE_BUF every time

        snprintf(EM_DATA.currentDateTime, sizeof(EM_DATA.currentDateTime), "%s.%06d", DATE_BUF, (unsigned int)tv.tv_usec);

        iGPSSensor.Receive(&EM_DATA);
        iRFIDSensor.Receive(&EM_DATA);
        iADSensor.Receive(&EM_DATA);
        
        if(EM_DATA.AD_honkSound != 0) iADSensor.HonkMyHorn(&EM_DATA);
        
        //iComplianceSensor.Receive();

        writeLogs(&EM_DATA);
        writeJSONState(&EM_DATA);

        EM_DATA.iterationTimer++;

        if (EM_DATA.iterationTimer % 60 == 0 &&
            stat(CONFIG.DATA_MNT, &st) == 0 &&
            st.st_dev == parent_st.st_dev) {
            cerr << "ERROR: Lost data drive somehow, continuing to write to backup data location" << endl;
            EM_DATA.dataDrivePresent = false;
            EM_DATA.SENSOR_DATA_files.pop_back();
            EM_DATA.RFID_DATA_files.pop_back();
        }

        // try to make "exactly" 1 second elapse each loop
        gettimeofday(&tv, NULL);
        tdiff = tv.tv_sec * 1000000 + tv.tv_usec - tstart;
        
        if (tdiff > POLL_PERIOD) {
            tdiff = POLL_PERIOD;
            cerr << "WARN: Took longer than 1 second to get data; check sensors" << endl;
        }
        
        if(!REMOVE_DELAY) usleep(POLL_PERIOD - tdiff);
    }
}

void writeLogs(EM_DATA_TYPE* em_data) {
    char SENSOR_DATA_buf[256] = { '\0' };

    // SENSOR_DATA.CSV
    snprintf(SENSOR_DATA_buf, sizeof(SENSOR_DATA_buf), "%s,%s,%s,%.1f,%f,%f,%f,%f,%d,%d,%.2f,%.3f,%lu",
        CONFIG.fishing_area,
        CONFIG.vrn,
        em_data->currentDateTime,
        em_data->AD_psi,
        em_data->GPS_latitude,
        em_data->GPS_longitude,
        em_data->GPS_heading,
        em_data->GPS_speed,
        em_data->GPS_satsused,
        em_data->GPS_satquality,
        em_data->GPS_hdop,
        em_data->GPS_epe,
        em_data->AD_state | em_data->RFID_state | em_data->GPS_state);

    em_data->SENSOR_DATA_lastHash = appendLogWithMD5(string(SENSOR_DATA_buf), em_data->SENSOR_DATA_files, em_data->SENSOR_DATA_lastHash);

    // RFID_DATA.CSV
    if(em_data->RFID_saveFlag) {
        char RFID_DATA_buf[256] = { '\0' };

        snprintf(RFID_DATA_buf, sizeof(RFID_DATA_buf), "%s,%s,%s,%llu,%f,%f,%f,%f,%d,%d,%.2f,%.3f,%lu",
            CONFIG.fishing_area,
            CONFIG.vrn,
            em_data->currentDateTime,
            em_data->RFID_lastScannedTag,
            em_data->GPS_latitude,
            em_data->GPS_longitude,
            em_data->GPS_heading,
            em_data->GPS_speed,
            em_data->GPS_satsused,
            em_data->GPS_satquality,
            em_data->GPS_hdop,
            em_data->GPS_epe,
            em_data->AD_state | em_data->RFID_state | em_data->GPS_state);

        em_data->RFID_DATA_lastHash = appendLogWithMD5(string(RFID_DATA_buf), em_data->RFID_DATA_files, em_data->RFID_DATA_lastHash);

        em_data->RFID_lastSavedTag = em_data->RFID_lastScannedTag;
        em_data->RFID_lastSaveIteration = em_data->iterationTimer;
        em_data->RFID_saveFlag = false;
    }
}

string appendLogWithMD5(string buf, list<string> files, string lastHash) {
    if(lastHash.empty()) {
        string& dataFile = files.front();
        string lastHashFile = dataFile + ".lh";

        cout << "INFO: Seeding last hash from " << lastHashFile << endl;

        ifstream fin(lastHashFile.c_str());
        if(!fin.fail()) {
            fin >> lastHash;
            fin.close();
        }
    }

    MD5 md5 = MD5(MD5_SALT + buf + lastHash);
    string newHash = md5.hexdigest();

    buf += "*" + newHash + "\n";

    FILE *fp;
    list<string>::iterator i;
    for(i = files.begin(); i != files.end(); i++) {
        if(!(fp = fopen((*i).c_str(), "a"))) {
            cerr << "ERROR: Can't write to data file " << *i << endl;
        } else {
            fwrite(buf.c_str(), buf.length(), 1, fp);
            fclose(fp);

            ofstream fout(string(*i + ".lh").c_str());
            if (!fout.fail()) {
                fout << newHash << endl;
                fout.close();
            }
        }
    }

    return newHash;
}

void writeJSONState(EM_DATA_TYPE* em_data) {
    char buf[1024] = { '\0' };

    snprintf(buf, sizeof(buf),
    "{ "
        "\"recorderVersion\": \"%s\", "
        "\"currentDateTime\": \"%s\", "
        "\"iterationTimer\": %lu, "
        "\"dataDrivePresent\": %s, "
        "\"dataDriveLabel\": \"%s\", "
        "\"GPS\": { "
            "\"state\": %lu, "
            "\"latitude\": %f, "
            "\"longitude\": %f, "
            "\"heading\": %f, "
            "\"speed\": %f, "
            "\"satquality\": %d, "
            "\"satsused\": %d, "
            "\"hdop\": %f, "
            "\"epe\": %f "
        "}, "
        "\"RFID\": { "
            "\"state\": %lu, "
            "\"lastScannedTag\": %llu "
        "}, "
        "\"AD\": { "
            "\"state\": %lu, "
            "\"psi\": %f, "
            "\"battery\": %f "
        "} "
    "}",
        VERSION, em_data->currentDateTime, em_data->iterationTimer, em_data->dataDrivePresent ? "true" : "false", em_data->dataDriveLabel,
        em_data->GPS_state, em_data->GPS_latitude, em_data->GPS_longitude, em_data->GPS_heading, em_data->GPS_speed, em_data->GPS_satquality, em_data->GPS_satsused, em_data->GPS_hdop, em_data->GPS_epe,
        em_data->RFID_state, em_data->RFID_lastScannedTag,
        em_data->AD_state, em_data->AD_psi, em_data->AD_battery);

    ofstream fout(CONFIG.JSON_STATE_FILE);
    if (!fout.fail()) {
        fout << buf << endl;
        fout.close();
    }
}
