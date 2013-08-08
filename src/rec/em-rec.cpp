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

#include "em-rec.h"
#include "config.h"
#include "GPSSensor.h"
#include "ADSensor.h"
#include "RFIDSensor.h"
#include "md5.h"

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <iomanip>
#include <cmath>

#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <blkid/blkid.h>
#include <termios.h>
#include <unistd.h>

using namespace std;

CONFIG_TYPE CONFIG = {};
EM_DATA_TYPE EM_DATA = {};

GPSSensor iGPSSensor(&EM_DATA);
RFIDSensor iRFIDSensor(&EM_DATA);
ADSensor iADSensor(&EM_DATA);

struct stat st, parent_st;

void makeVideoSymlink(const char *path) {
    unlink(CONFIG.VIDEOS_DIR);
    symlink(path, CONFIG.VIDEOS_DIR);
    cout << "INFO: Created symlink " << CONFIG.VIDEOS_DIR << " to " << path << endl;
}

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

    if (readConfigFile(CONFIG_FILENAME)) {
        cerr << "ERROR: Failed to get configuration" << endl;
        exit(-1);
    }

    strcpy(CONFIG.fishing_area, getConfig("fishing_area", DEFAULT_fishing_area));
    strcpy(CONFIG.vessel, getConfig("vessel", DEFAULT_vessel));
    strcpy(CONFIG.vrn, getConfig("vrn", DEFAULT_vrn));
    strcpy(CONFIG.arduino_type, getConfig("arduino", DEFAULT_arduino));
    strcpy(CONFIG.psi_vmin, getConfig("psi_vmin", DEFAULT_psi_vmin));
    strcpy(CONFIG.cam, getConfig("cam", DEFAULT_cam));
    strcpy(CONFIG.DATA_MNT, getConfig("DATA_MNT", DEFAULT_DATA_MNT));
    strcpy(CONFIG.BACKUP_DATA_MNT, getConfig("BACKUP_DATA_MNT", DEFAULT_BACKUP_DATA_MNT));
    strcpy(CONFIG.EM_DIR, getConfig("EM_DIR", DEFAULT_EM_DIR));
    strcpy(CONFIG.JSON_STATE_FILE, getConfig("JSON_STATE_FILE", DEFAULT_JSON_STATE_FILE));
    strcpy(CONFIG.VIDEOS_DIR, getConfig("EM_DIR", DEFAULT_EM_DIR)); strcat(CONFIG.VIDEOS_DIR, "/video");
    strcpy(CONFIG.ARDUINO_DEV, getConfig("ARDUINO_DEV", DEFAULT_ARDUINO_DEV));
    strcpy(CONFIG.GPS_DEV, getConfig("GPS_DEV", DEFAULT_GPS_DEV));
    strcpy(CONFIG.RFID_DEV, getConfig("RFID_DEV", DEFAULT_RFID_DEV));
    strcpy(CONFIG.SENSOR_DATA, getConfig("SENSOR_DATA", DEFAULT_SENSOR_DATA));
    strcpy(CONFIG.RFID_DATA, getConfig("RFID_DATA", DEFAULT_RFID_DATA));
    strcpy(CONFIG.SYSTEM_DATA, getConfig("SYSTEM_DATA", DEFAULT_SYSTEM_DATA));
    strcpy(EM_DATA.GPS_homePortDataFile, getConfig("HOME_PORT_DATA", DEFAULT_HOME_PORT_DATA));
    strcpy(EM_DATA.GPS_ferryDataFile, getConfig("FERRY_DATA", DEFAULT_FERRY_DATA));
    strcpy(CONFIG.PAUSE_MARKER, getConfig("PAUSE_MARKER", DEFAULT_PAUSE_MARKER));
    strcpy(CONFIG.SCREENSHOT_MARKER, getConfig("SCREENSHOT_MARKER", DEFAULT_SCREENSHOT_MARKER));

    EM_DATA.SENSOR_DATA_files.push_back(string(CONFIG.BACKUP_DATA_MNT) + "/" + CONFIG.SENSOR_DATA);
    EM_DATA.RFID_DATA_files.push_back(string(CONFIG.BACKUP_DATA_MNT) + "/" + CONFIG.RFID_DATA);
    EM_DATA.SYSTEM_DATA_files.push_back(string(CONFIG.BACKUP_DATA_MNT) + "/" + CONFIG.SYSTEM_DATA);

    // check /mnt/data's device ID and compare it to its parent /mnt
    stat((string(CONFIG.DATA_MNT) + "/../").c_str(), &parent_st);
    if(stat(CONFIG.DATA_MNT, &st) == 0 && st.st_dev != parent_st.st_dev) {
        blkid_get_cache(&blkid_cache, NULL);
        EM_DATA.SYS_dataDiskLabel = blkid_get_tag_value(blkid_cache, "LABEL", blkid_devno_to_devname(st.st_dev));

        cout << "INFO: Data disk PRESENT, label = " << EM_DATA.SYS_dataDiskLabel << endl;
        EM_DATA.SYS_dataDiskPresent = true;
        
        EM_DATA.SENSOR_DATA_files.push_back(string(CONFIG.DATA_MNT) + "/" + CONFIG.SENSOR_DATA);
        EM_DATA.RFID_DATA_files.push_back(string(CONFIG.DATA_MNT) + "/" + CONFIG.RFID_DATA);
        EM_DATA.SYSTEM_DATA_files.push_back(string(CONFIG.DATA_MNT) + "/" + CONFIG.SYSTEM_DATA);

        makeVideoSymlink(CONFIG.DATA_MNT);
    } else {
        cout << "INFO: Data disk NOT PRESENT" << endl;
        EM_DATA.SYS_dataDiskPresent = false;

        makeVideoSymlink(CONFIG.BACKUP_DATA_MNT);
    }

    string scanCountsFile = string(CONFIG.EM_DIR) + "/" + SCAN_COUNT_FILENAME;
    ifstream fin(scanCountsFile.c_str());
    if(!fin.fail()) {
        fin >> EM_DATA.RFID_stringScans >> EM_DATA.RFID_tripScans;
        fin.close();
        cout << "INFO: Got previous scan counts from " << scanCountsFile << endl;
    }

    iADSensor.SetPort(CONFIG.ARDUINO_DEV);
    iADSensor.SetBaudRate(B9600);
    iADSensor.SetADCType(CONFIG.arduino_type, CONFIG.psi_vmin);

    iRFIDSensor.SetPort(CONFIG.RFID_DEV);
    iRFIDSensor.SetBaudRate(B9600);

    iADSensor.Connect();
    iRFIDSensor.Connect();
    // don't connect to GPS sensor until we first try to Receive() from it
    // which won't be until the GPS_WARMUP_PERIOD has elapsed
    //iGPSSensor.Connect();

    recordLoop();

    iGPSSensor.Close();
    iRFIDSensor.Close();
    iADSensor.Close();

    ofstream fout(scanCountsFile.c_str());
    if(!fout.fail()) {
        fout << EM_DATA.RFID_stringScans << ' ' << EM_DATA.RFID_tripScans;
        fout.close();
        cout << "INFO: Wrote out scan counts to " << scanCountsFile << endl;
    }

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
    short encodingState = STATE_ENCODING_UNDEFINED;

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

        // this whole block once every STATS_COLLECTION_INTERVAL (5 seconds)
        if ((specialPeriodIndex = EM_DATA.iterationTimer % STATS_COLLECTION_INTERVAL) == 0) {
            D("Checking for data disk and computing system stats");

            // check if data disk disappeared
            if(EM_DATA.SYS_dataDiskPresent && stat(CONFIG.DATA_MNT, &st) == 0 && st.st_dev == parent_st.st_dev) {
                cerr << "ERROR: Lost data disk somehow, continuing to write to backup data location" << endl;
                EM_DATA.SYS_dataDiskPresent = false;
                EM_DATA.SENSOR_DATA_files.pop_back();
                EM_DATA.RFID_DATA_files.pop_back();
                EM_DATA.SYSTEM_DATA_files.pop_back();
                
                makeVideoSymlink(CONFIG.BACKUP_DATA_MNT);
            }

            if(EM_DATA.iterationTimer >= GPS_WARMUP_PERIOD) {
                if(iGPSSensor.CheckSpecialAreas() == STATE_ENCODING_PAUSED) { // pause encoding
                    if(encodingState != STATE_ENCODING_PAUSED) {
                        cout << "INFO: Entered home port (" << EM_DATA.GPS_latitude << "," << EM_DATA.GPS_longitude << "), creating pause marker " << CONFIG.PAUSE_MARKER << endl;
                        encodingState = STATE_ENCODING_PAUSED;
                    }

                    FILE *fp = fopen(CONFIG.PAUSE_MARKER, "w"); fclose(fp);
                } else { // don't pause encoding
                    if(encodingState != STATE_ENCODING_RUNNING) {
                        cout << "INFO: Left home port or was never there (" << EM_DATA.GPS_latitude << "," << EM_DATA.GPS_longitude << "), removing pause marker " << CONFIG.PAUSE_MARKER << endl;
                        encodingState = STATE_ENCODING_RUNNING;
                    }

                    unlink(CONFIG.PAUSE_MARKER);
                }
            }

            computeAndLogSystemStats(&EM_DATA, specialPeriodIndex);

            if (EM_DATA.iterationTimer > 0) {
                if(shouldTakeScreenshot(&EM_DATA)) {
                    FILE *fp = fopen(CONFIG.SCREENSHOT_MARKER, "w"); fclose(fp);
                } else {
                    unlink(CONFIG.SCREENSHOT_MARKER);
                }
            }
        }

        writeLogs(&EM_DATA);
        writeJSONState(&EM_DATA);

        EM_DATA.iterationTimer++;

        // try to make "exactly" 1 second elapse each loop
        gettimeofday(&tv, NULL);
        tdiff = tv.tv_sec * 1000000 + tv.tv_usec - tstart;
        D("Loop run time was " << (double)tdiff/1000 << " ms");

        if (tdiff > POLL_PERIOD) {
            tdiff = POLL_PERIOD;
            cerr << "WARN: Took longer than 1 second to get data; check sensors" << endl;
        }

        if(!REMOVE_DELAY) usleep(POLL_PERIOD - tdiff);
    }
}

void writeLogs(EM_DATA_TYPE* em_data) {
    char SENSOR_DATA_buf[256] = { '\0' };

    // SENSOR_DATA.csv
    snprintf(SENSOR_DATA_buf, sizeof(SENSOR_DATA_buf), "%s,%s,%s,%.1f,%lf,%lf,%lf,%lf,%d,%d,%.2lf,%.3lf,%lu",
        CONFIG.fishing_area,
        CONFIG.vrn,
        em_data->currentDateTime,
        isnan(em_data->AD_psi) ? 0 : em_data->AD_psi,
        isnan(em_data->GPS_latitude) ? 0 : em_data->GPS_latitude,
        isnan(em_data->GPS_longitude) ? 0 : em_data->GPS_longitude,
        isnan(em_data->GPS_heading) ? 0 : em_data->GPS_heading,
        isnan(em_data->GPS_speed) ? 0 : em_data->GPS_speed,
        em_data->GPS_satsUsed,
        em_data->GPS_satQuality,
        isnan(em_data->GPS_hdop) ? 0 : em_data->GPS_hdop,
        isnan(em_data->GPS_eph) ? 0 : em_data->GPS_eph,
        em_data->AD_state | em_data->RFID_state | em_data->GPS_state);

    em_data->SENSOR_DATA_lastHash = appendLogWithMD5(string(SENSOR_DATA_buf), em_data->SENSOR_DATA_files, em_data->SENSOR_DATA_lastHash);

    // RFID_DATA.csv
    if(em_data->RFID_saveFlag) {
        char RFID_DATA_buf[256] = { '\0' };

        snprintf(RFID_DATA_buf, sizeof(RFID_DATA_buf), "%s,%s,%s,%llu,%.1f,%lu,%lu,%lf,%lf,%lf,%lf,%d,%d,%.2lf,%.3lf,%lu",
            CONFIG.fishing_area,
            CONFIG.vrn,
            em_data->currentDateTime,
            em_data->RFID_lastScannedTag,
            isnan(em_data->AD_psi) ? 0 : em_data->AD_psi,
            em_data->RFID_stringScans,
            em_data->RFID_tripScans,
            isnan(em_data->GPS_latitude) ? 0 : em_data->GPS_latitude,
            isnan(em_data->GPS_longitude) ? 0 : em_data->GPS_longitude,
            isnan(em_data->GPS_heading) ? 0 : em_data->GPS_heading,
            isnan(em_data->GPS_speed) ? 0 : em_data->GPS_speed,
            em_data->GPS_satsUsed,
            em_data->GPS_satQuality,
            isnan(em_data->GPS_hdop) ? 0 : em_data->GPS_hdop,
            isnan(em_data->GPS_eph) ? 0 : em_data->GPS_eph,
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

#define PROC_STAT_VALS          7
#define DISK_USAGE_SAMPLES      24
#define DISK_USAGE_START_VAL    30240
void computeAndLogSystemStats(EM_DATA_TYPE* em_data, unsigned short index) {
    struct sysinfo si;
    int days, hours, mins;
    FILE *ps, *ct;
    unsigned long long jiffies[PROC_STAT_VALS] = {0}, totalJiffies = 0, temp;
    struct statvfs vfs;
    char SYSTEM_DATA_buf[256] = { '\0' };

    static unsigned long long lastJiffies[PROC_STAT_VALS];
    static unsigned long osDiskLastFree, osDiskDiff, dataDiskLastFree, dataDiskDiff;
    static unsigned long osDiskMinutesLeft[DISK_USAGE_SAMPLES], dataDiskMinutesLeft[DISK_USAGE_SAMPLES];
    static unsigned long osDiskMinutesLeft_total = DISK_USAGE_SAMPLES * DISK_USAGE_START_VAL, dataDiskMinutesLeft_total = DISK_USAGE_SAMPLES * DISK_USAGE_START_VAL;
    static signed short osDiskMinutesLeft_index = -1, dataDiskMinutesLeft_index = -1;

    if (osDiskMinutesLeft_index == -1) {
        for(int i = 0; i < DISK_USAGE_SAMPLES; i++) osDiskMinutesLeft[i] = DISK_USAGE_START_VAL;
        em_data->SYS_osDiskMinutesLeft = osDiskMinutesLeft_total / DISK_USAGE_SAMPLES;
        osDiskMinutesLeft_index = 0;
    }

    if (dataDiskMinutesLeft_index == -1) {
        for(int i = 0; i < DISK_USAGE_SAMPLES; i++) dataDiskMinutesLeft[i] = DISK_USAGE_START_VAL;
        em_data->SYS_dataDiskMinutesLeft = dataDiskMinutesLeft_total / DISK_USAGE_SAMPLES;
        dataDiskMinutesLeft_index = 0;
    }

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
        if (i != 3 && jiffies[i] > lastJiffies[i]) {
            totalJiffies += jiffies[i] - lastJiffies[i];
        }
    }

    em_data->SYS_cpuPercent = totalJiffies;

    if(jiffies[3] > lastJiffies[3]) {
        totalJiffies += (jiffies[3] - lastJiffies[3]);
        em_data->SYS_cpuPercent = em_data->SYS_cpuPercent / totalJiffies * 100;
    } else {
        em_data->SYS_cpuPercent = -1.0;
    }

    for(int i = 0; i < PROC_STAT_VALS; i++) { lastJiffies[i] = jiffies[i]; }
    
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
        if(osDiskLastFree >= vfs.f_bavail) {
            osDiskMinutesLeft_total -= osDiskMinutesLeft[osDiskMinutesLeft_index];
            osDiskDiff = osDiskLastFree - vfs.f_bavail;
            if(osDiskDiff <= 0) osDiskMinutesLeft[osDiskMinutesLeft_index] = osDiskMinutesLeft[osDiskMinutesLeft_index - 1];
            else osDiskMinutesLeft[osDiskMinutesLeft_index] = (int)((double)vfs.f_bavail / (double)(60000000 / POLL_PERIOD / STATS_COLLECTION_INTERVAL * (osDiskLastFree - vfs.f_bavail)));
            osDiskMinutesLeft_total += osDiskMinutesLeft[osDiskMinutesLeft_index];
            
            osDiskMinutesLeft_index++;
            if(osDiskMinutesLeft_index >= DISK_USAGE_SAMPLES) osDiskMinutesLeft_index = 0;

            em_data->SYS_osDiskMinutesLeft = osDiskMinutesLeft_total / DISK_USAGE_SAMPLES;
        }

        em_data->SYS_osDiskTotalBlocks = vfs.f_blocks;
        em_data->SYS_osDiskFreeBlocks = osDiskLastFree = vfs.f_bavail;
    }

    if(EM_DATA.SYS_dataDiskPresent) {
        if(statvfs(CONFIG.DATA_MNT, &vfs) == 0) {

            if(dataDiskLastFree >= vfs.f_bavail) {
                dataDiskMinutesLeft_total -= dataDiskMinutesLeft[dataDiskMinutesLeft_index];
                dataDiskDiff = dataDiskLastFree - vfs.f_bavail;
                if(dataDiskDiff <= 0) dataDiskMinutesLeft[dataDiskMinutesLeft_index] = dataDiskMinutesLeft[dataDiskMinutesLeft_index - 1];
                else dataDiskMinutesLeft[dataDiskMinutesLeft_index] = (int)((double)vfs.f_bavail / (double)(60000000 / POLL_PERIOD / STATS_COLLECTION_INTERVAL * dataDiskDiff));
                dataDiskMinutesLeft_total += dataDiskMinutesLeft[dataDiskMinutesLeft_index];
                
                dataDiskMinutesLeft_index++;
                if(dataDiskMinutesLeft_index >= DISK_USAGE_SAMPLES) dataDiskMinutesLeft_index = 0;
                em_data->SYS_dataDiskMinutesLeft = dataDiskMinutesLeft_total / DISK_USAGE_SAMPLES;
            }

            string hourCountsFile = string(CONFIG.DATA_MNT) + "/hour_counter.dat";
            ifstream fin(hourCountsFile.c_str());
            if(!fin.fail()) {
                fin >> em_data->SYS_dataDiskMinutesLeftFake;
                fin.close();

                if(em_data->SYS_dataDiskMinutesLeftFake > DATA_DISK_FAKE_DAYS_START * 24) {
                    em_data->SYS_dataDiskMinutesLeftFake = 0;
                } else {
                    em_data->SYS_dataDiskMinutesLeftFake = (DATA_DISK_FAKE_DAYS_START * 24 - em_data->SYS_dataDiskMinutesLeftFake) * 60;
                }
            } else {
                em_data->SYS_dataDiskMinutesLeftFake = DATA_DISK_FAKE_DAYS_START * 24 * 60;
            }

            em_data->SYS_dataDiskTotalBlocks = vfs.f_blocks;
            em_data->SYS_dataDiskFreeBlocks = dataDiskLastFree = vfs.f_bavail;
        }
    }

    // SYSTEM_DATA.csv
    snprintf(SYSTEM_DATA_buf, sizeof(SYSTEM_DATA_buf),
        "%s,%s,%s,%lu,%0.02f,%0.02f,%0.02f,%.1f,%u,%u,%llu,%llu,%lu,%lu\n",
        CONFIG.fishing_area,
        CONFIG.vrn,
        em_data->currentDateTime,
        si.uptime,
        (float)si.loads[0] / (float)(1<<SI_LOAD_SHIFT),
        (float)si.loads[1] / (float)(1<<SI_LOAD_SHIFT),
        (float)si.loads[2] / (float)(1<<SI_LOAD_SHIFT),
        isnan(em_data->SYS_cpuPercent) ? 0 : em_data->SYS_cpuPercent,
        em_data->SYS_tempCore0,
        em_data->SYS_tempCore1,
        em_data->SYS_ramFreeKB,
        si.freeswap * (unsigned long long)si.mem_unit / 1024,
        em_data->SYS_osDiskFreeBlocks * 4096 / 1024 / 1024,
        em_data->SYS_dataDiskFreeBlocks * 4096 / 1024 / 1024);

    FILE *fp;
    list<string>::iterator i;
    for(i = em_data->SYSTEM_DATA_files.begin(); i != em_data->SYSTEM_DATA_files.end(); i++) {
        if(!(fp = fopen((*i).c_str(), "a"))) {
            cerr << "ERROR: Can't write to data file " << *i << endl;
        } else {
            fwrite(SYSTEM_DATA_buf, strlen(SYSTEM_DATA_buf), 1, fp);
            fclose(fp);
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
            "\"fishingArea\": \"%s\", "
            "\"numCam\": %s, "
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
            "\"dataDiskMinutesLeft\": %lu, "
            "\"dataDiskMinutesLeftFake\": %lu "
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

        CONFIG.fishing_area,
        CONFIG.cam,
        em_data->SYS_uptime,
        em_data->SYS_load,
        isnan(em_data->SYS_cpuPercent) ? 0 : em_data->SYS_cpuPercent,
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
        em_data->SYS_dataDiskMinutesLeftFake,

        em_data->GPS_state,
        isnan(em_data->GPS_latitude) ? 0 : em_data->GPS_latitude,
        isnan(em_data->GPS_longitude) ? 0 : em_data->GPS_longitude,
        isnan(em_data->GPS_heading) ? 0 : em_data->GPS_heading,
        isnan(em_data->GPS_speed) ? 0 : em_data->GPS_speed,
        em_data->GPS_satQuality,
        em_data->GPS_satsUsed,
        isnan(em_data->GPS_hdop) ? 0 : em_data->GPS_hdop,
        isnan(em_data->GPS_eph) ? 0 : em_data->GPS_eph,

        em_data->RFID_state,
        em_data->RFID_lastScannedTag,
        em_data->RFID_stringScans,
        em_data->RFID_tripScans,

        em_data->AD_state,
        isnan(em_data->AD_psi) ? 0 : em_data->AD_psi,
        isnan(em_data->AD_battery) ? 0 : em_data->AD_battery
    );

    ofstream fout(CONFIG.JSON_STATE_FILE);
    if (!fout.fail()) {
        fout << buf << endl;
        fout.close();
    }
}

bool shouldTakeScreenshot(EM_DATA_TYPE* em_data) {
    static unsigned long ignoredStates =
        GPS_NO_FERRY_DATA |
        GPS_NO_HOME_PORT_DATA |
        GPS_INSIDE_HOME_PORT |
        AD_BATTERY_LOW |
        AD_BATTERY_HIGH |
        RFID_CHECKSUM_FAILED;

    if((em_data->AD_state | em_data->RFID_state | em_data->GPS_state) & (~ignoredStates)) return true;
    
    return false;
}
