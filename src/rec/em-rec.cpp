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
#include <signal.h>
#include <sys/stat.h>
#include <list>
#include <fstream>
#include <blkid/blkid.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <limits>
#include <iomanip>

#include "em-rec.h"
#include "libgpsmm.h"
#include "../config.h"
#include "../util.h"
#include "md5.h"
#include "GPSSensor.h"
#include "ADSensor.h"
#include "RFIDSensor.h"

using namespace std;

CONFIG_TYPE CONFIG = {};
EM_DATA_TYPE EM_DATA = {};

GPSSensor iGPSSensor(&EM_DATA);
RFIDSensor iRFIDSensor(&EM_DATA);
ADSensor iADSensor(&EM_DATA);

struct stat st, parent_st;

int main(int argc, char *argv[]) {
    struct blkid_struct_cache *blkid_cache;

    cout << "INFO: Ecotrust EM Recorder v" << VERSION << " is starting" << endl;
    cout << setprecision(numeric_limits<double>::digits10 + 1);
    cerr << setprecision(numeric_limits<double>::digits10 + 1);

    signal(SIGINT, exit_handler);
    signal(SIGHUP, exit_handler);
    signal(SIGTERM, exit_handler);
    signal(SIGUSR1, reset_string_scans_handler);
    signal(SIGUSR2, reset_trip_scans_handler);

    EM_DATA.runState = STATE_RUNNING;

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
    strcpy(EM_DATA.GPS_homePortDataFile, getConfig("HOME_PORT_DATA"));
    strcpy(EM_DATA.GPS_ferryDataFile, getConfig("FERRY_DATA"));

    EM_DATA.SENSOR_DATA_files.push_back(string(CONFIG.BACKUP_DATA_MNT) + "/" + CONFIG.SENSOR_DATA);
    EM_DATA.RFID_DATA_files.push_back(string(CONFIG.BACKUP_DATA_MNT) + "/" + CONFIG.RFID_DATA);

    // check /mnt/data's device ID and compare it to its parent /mnt
    stat((string(CONFIG.DATA_MNT) + "/../").c_str(), &parent_st);
    if(stat(CONFIG.DATA_MNT, &st) == 0 && st.st_dev != parent_st.st_dev) {
        blkid_get_cache(&blkid_cache, NULL);
        EM_DATA.SYS_dataDiskLabel = blkid_get_tag_value(blkid_cache, "LABEL", blkid_devno_to_devname(st.st_dev));

        cout << "INFO: Data disk PRESENT, label = " << EM_DATA.SYS_dataDiskLabel << endl;
        EM_DATA.SYS_dataDiskPresent = true;
        
        EM_DATA.SENSOR_DATA_files.push_back(string(CONFIG.DATA_MNT) + "/" + CONFIG.SENSOR_DATA);
        EM_DATA.RFID_DATA_files.push_back(string(CONFIG.DATA_MNT) + "/" + CONFIG.RFID_DATA);
    } else {
        cout << "INFO: Data disk NOT PRESENT" << endl;
        EM_DATA.SYS_dataDiskPresent = false;
    }

    iADSensor.SetPort(CONFIG.ARDUINO_DEV);
    iADSensor.SetBaudRate(B9600);

    iRFIDSensor.SetPort(CONFIG.RFID_DEV);
    iRFIDSensor.SetBaudRate(B9600);

    iADSensor.Connect();
    iRFIDSensor.Connect();
    iGPSSensor.Connect();

    recordLoop();

    iGPSSensor.Close();
    iRFIDSensor.Close();
    iADSensor.Close();

    cout << "INFO: Stopped" << endl;

    return 0;
}

void reset_string_scans_handler(int s) {
    D("Resetting stringScans");
    iRFIDSensor.resetStringScans();
}

void reset_trip_scans_handler(int s) {
    D("Resetting tripScans");
    iRFIDSensor.resetTripScans();
}

void exit_handler(int s) {
    cout << "INFO: Ecotrust EM Recorder v" << VERSION << " is stopping" << endl;
    EM_DATA.runState = STATE_NOT_RUNNING;
}

void recordLoop() {
    struct timeval tv;
    unsigned long long tstart = 0, tdiff = 0;
    char DATE_BUF[24] = { '\0' };
    time_t rawtime;
    struct tm *timeinfo;
    unsigned short specialPeriodIndex = 0;

    cout << "INFO: Entered recordLoop()" << endl;

    while(EM_DATA.runState == STATE_RUNNING) {
        gettimeofday(&tv, NULL);
        tstart = tv.tv_sec * 1000000 + tv.tv_usec;

        // get date
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(DATE_BUF, 24, "%Y-%m-%d %H:%M:%S", timeinfo); // writes null-terminated string so okay not to clear DATE_BUF every time
        snprintf(EM_DATA.currentDateTime, sizeof(EM_DATA.currentDateTime), "%s.%06d", DATE_BUF, (unsigned int)tv.tv_usec);

        iGPSSensor.Receive();
        iRFIDSensor.Receive();
        iADSensor.Receive();
        
        if(EM_DATA.AD_honkSound != 0) {
            D("Honking horn");
            iADSensor.HonkMyHorn();
        }

        if ((specialPeriodIndex = EM_DATA.iterationTimer % STATS_COLLECTION_INTERVAL) == 0) {
            D("Checking for data disk and computing system stats");

            // check if data disk disappeared
            if(EM_DATA.SYS_dataDiskPresent && stat(CONFIG.DATA_MNT, &st) == 0 && st.st_dev == parent_st.st_dev) {
                cerr << "ERROR: Lost data disk somehow, continuing to write to backup data location" << endl;
                EM_DATA.SYS_dataDiskPresent = false;
                EM_DATA.SENSOR_DATA_files.pop_back();
                EM_DATA.RFID_DATA_files.pop_back();
            }

            iGPSSensor.CheckSpecialAreas();

            computeSystemStats(&EM_DATA, specialPeriodIndex);
        }

        writeLogs(&EM_DATA);
        writeJSONState(&EM_DATA);

        EM_DATA.iterationTimer++;

        // try to make "exactly" 1 second elapse each loop
        gettimeofday(&tv, NULL);
        tdiff = tv.tv_sec * 1000000 + tv.tv_usec - tstart;
        D("Loop run time was " << tdiff / 1000 << " ms");

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
    snprintf(SENSOR_DATA_buf, sizeof(SENSOR_DATA_buf), "%s,%s,%s,%.1f,%lf,%lf,%lf,%lf,%d,%d,%.2lf,%.3lf,%lu",
        CONFIG.fishing_area,
        CONFIG.vrn,
        em_data->currentDateTime,
        em_data->AD_psi,
        em_data->GPS_latitude,
        em_data->GPS_longitude,
        em_data->GPS_heading,
        em_data->GPS_speed,
        em_data->GPS_satsUsed,
        em_data->GPS_satQuality,
        em_data->GPS_hdop,
        em_data->GPS_eph,
        em_data->AD_state | em_data->RFID_state | em_data->GPS_state);

    em_data->SENSOR_DATA_lastHash = appendLogWithMD5(string(SENSOR_DATA_buf), em_data->SENSOR_DATA_files, em_data->SENSOR_DATA_lastHash);

    // RFID_DATA.CSV
    if(em_data->RFID_saveFlag) {
        char RFID_DATA_buf[256] = { '\0' };

        snprintf(RFID_DATA_buf, sizeof(RFID_DATA_buf), "%s,%s,%s,%llu,%lf,%lf,%lf,%lf,%d,%d,%.2lf,%.3lf,%lu",
            CONFIG.fishing_area,
            CONFIG.vrn,
            em_data->currentDateTime,
            em_data->RFID_lastScannedTag,
            em_data->GPS_latitude,
            em_data->GPS_longitude,
            em_data->GPS_heading,
            em_data->GPS_speed,
            em_data->GPS_satsUsed,
            em_data->GPS_satQuality,
            em_data->GPS_hdop,
            em_data->GPS_eph,
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

#define PROC_STAT_VALS 7

void computeSystemStats(EM_DATA_TYPE* em_data, unsigned short index) {
    struct sysinfo si;
    int days, hours, mins;
    FILE *ps, *ct;
    unsigned long long jiffies[PROC_STAT_VALS] = {0}, total_jiffies = 0, temp;
    struct statvfs vfs;

    static unsigned long long last_jiffies[PROC_STAT_VALS];
    static unsigned long osdisk_lastfree, datadisk_lastfree;

    if(sysinfo(&si) == 0) {
        // SYS_uptime
        days = si.uptime / 86400;
        hours = (si.uptime / 3600) - (days * 24);
        mins = (si.uptime / 60) - (days * 1440) - (hours * 60);
        snprintf(em_data->SYS_uptime, sizeof(em_data->SYS_uptime), "%dd%dh%dm", days, hours, mins);
        
        // SYS_ramFree/Total
        em_data->SYS_ramFreeKB = si.freeram * (unsigned long long)si.mem_unit / 1024;
        em_data->SYS_ramTotalKB = si.totalram * (unsigned long long)si.mem_unit / 1024;
        
        // SYS_load
        snprintf(em_data->SYS_load, sizeof(em_data->SYS_load), "%0.02f %0.02f %0.02f", (float)si.loads[0] / (float)(1<<SI_LOAD_SHIFT), (float)si.loads[1] / (float)(1<<SI_LOAD_SHIFT), (float)si.loads[2] / (float)(1<<SI_LOAD_SHIFT));
    }

    // SYS_cpuPercent
    ps = fopen("/proc/stat", "r");
    fscanf(ps, "cpu %Ld %Ld %Ld %Ld %Ld %Ld %Ld", &jiffies[0], &jiffies[1], &jiffies[2], &jiffies[3], &jiffies[4], &jiffies[5], &jiffies[6]);
    fclose(ps);

    for(int i = 0; i < PROC_STAT_VALS; i++) {
        if (i != 3 && jiffies[i] > last_jiffies[i]) {
            total_jiffies += jiffies[i] - last_jiffies[i];
        }
    }

    em_data->SYS_cpuPercent = total_jiffies;

    if(jiffies[3] > last_jiffies[3]) {
        total_jiffies += (jiffies[3] - last_jiffies[3]);
        em_data->SYS_cpuPercent = em_data->SYS_cpuPercent / total_jiffies * 100;
    } else {
        em_data->SYS_cpuPercent = -1.0;
    }

    for(int i = 0; i < PROC_STAT_VALS; i++) { last_jiffies[i] = jiffies[i]; }
    
    // SYS_tempCore0
    ct = fopen("/sys/devices/platform/coretemp.0/temp2_input", "r");
    fscanf(ct, "%llu", &temp);
    fclose(ct);
    em_data->SYS_tempCore0 = temp / 1000;

    // SYS_tempCore1
    ct = fopen("/sys/devices/platform/coretemp.0/temp3_input", "r");
    fscanf(ct, "%llu", &temp);
    fclose(ct);
    em_data->SYS_tempCore1 = temp / 1000;

    //SYS_*DiskTotalBlocks/FreeBlocks
    if(statvfs(CONFIG.BACKUP_DATA_MNT, &vfs) == 0) {
        if(osdisk_lastfree > vfs.f_bavail) {
            em_data->SYS_osDiskMinutesLeft = (int)((double)vfs.f_bavail / (double)(60000000 / POLL_PERIOD / STATS_COLLECTION_INTERVAL * (osdisk_lastfree - vfs.f_bavail)));
        }

        em_data->SYS_osDiskTotalBlocks = vfs.f_blocks;
        em_data->SYS_osDiskFreeBlocks = osdisk_lastfree = vfs.f_bavail;
    }

    if(EM_DATA.SYS_dataDiskPresent) {
        if(statvfs(CONFIG.DATA_MNT, &vfs) == 0) {
            if(datadisk_lastfree > vfs.f_bavail) {
                em_data->SYS_dataDiskMinutesLeft = (int)((double)vfs.f_bavail / (double)(60000000 / POLL_PERIOD / STATS_COLLECTION_INTERVAL * (datadisk_lastfree - vfs.f_bavail)));
            }

            em_data->SYS_dataDiskTotalBlocks = vfs.f_blocks;
            em_data->SYS_dataDiskFreeBlocks = datadisk_lastfree = vfs.f_bavail;
        }
    }
}

void writeJSONState(EM_DATA_TYPE* em_data) {
    char buf[1024] = { '\0' };

    snprintf(buf, sizeof(buf),
    "{ "
        "\"recorderVersion\": \"%s\", "
        "\"currentDateTime\": \"%s\", "
        "\"iterationTimer\": %lu, "
        "\"SYS\": { "
            "\"uptime\": \"%s\", "
            "\"load\": \"%s\", "
            "\"cpuPercent\": %.1f, "
            "\"ramFreeKB\": %llu, "
            "\"ramTotalKB\": %llu, "
            "\"tempCore0\": %u, "
            "\"tempCore1\": %u, "
            "\"dataDiskPresent\": \"%s\", "
            "\"osDiskFreeBlocks\": %lu, "
            "\"osDiskTotalBlocks\": %lu, "
            "\"osDiskMinutesLeft\": %lu, "
            "\"dataDiskLabel\": \"%s\", "
            "\"dataDiskFreeBlocks\": %lu, "
            "\"dataDiskTotalBlocks\": %lu, "
            "\"dataDiskMinutesLeft\": %lu "
        "}, "
        "\"GPS\": { "
            "\"state\": %lu, "
            "\"latitude\": %f, "
            "\"longitude\": %f, "
            "\"heading\": %f, "
            "\"speed\": %f, "
            "\"satQuality\": %d, "
            "\"satsUsed\": %d, "
            "\"hdop\": %f, "
            "\"eph\": %f "
        "}, "
        "\"RFID\": { "
            "\"state\": %lu, "
            "\"lastScannedTag\": %llu, "
            "\"stringScans\": %lu, "
            "\"tripScans\": %lu "
        "}, "
        "\"AD\": { "
            "\"state\": %lu, "
            "\"psi\": %f, "
            "\"battery\": %f "
        "} "
    "}",
        VERSION,
        em_data->currentDateTime,
        em_data->iterationTimer,

        em_data->SYS_uptime,
        em_data->SYS_load,
        em_data->SYS_cpuPercent,
        em_data->SYS_ramFreeKB,
        em_data->SYS_ramTotalKB,
        em_data->SYS_tempCore0,
        em_data->SYS_tempCore1,
        em_data->SYS_dataDiskPresent ? "true" : "false",
        em_data->SYS_osDiskFreeBlocks,
        em_data->SYS_osDiskTotalBlocks,
        em_data->SYS_osDiskMinutesLeft,
        em_data->SYS_dataDiskLabel,
        em_data->SYS_dataDiskFreeBlocks,
        em_data->SYS_dataDiskTotalBlocks,
        em_data->SYS_dataDiskMinutesLeft,

        em_data->GPS_state,
        em_data->GPS_latitude,
        em_data->GPS_longitude,
        em_data->GPS_heading,
        em_data->GPS_speed,
        em_data->GPS_satQuality,
        em_data->GPS_satsUsed,
        em_data->GPS_hdop,
        em_data->GPS_eph,

        em_data->RFID_state,
        em_data->RFID_lastScannedTag,
        em_data->RFID_stringScans,
        em_data->RFID_tripScans,

        em_data->AD_state,
        em_data->AD_psi,
        em_data->AD_battery
    );

    ofstream fout(CONFIG.JSON_STATE_FILE);
    if (!fout.fail()) {
        fout << buf << endl;
        fout.close();
    }
}
