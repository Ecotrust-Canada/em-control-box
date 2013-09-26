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

#include <sys/mount.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <blkid/blkid.h>
#include <termios.h>
#include <unistd.h>
#include <pthread.h>

using namespace std;

CONFIG_TYPE CONFIG = {};
EM_DATA_TYPE EM_DATA = {};

GPSSensor iGPSSensor(&EM_DATA);
RFIDSensor iRFIDSensor(&EM_DATA);
ADSensor iADSensor(&EM_DATA);

StateMachine smEM(STATE_RUNNING, true);
StateMachine smOptions(0, false);
StateMachine smHelperThread(STATE_NOT_RUNNING, true);
StateMachine smVideoCapture(STATE_NOT_RUNNING, true);

struct g_parent_st;
struct tm *g_timeinfo;

int main(int argc, char *argv[]) {
    struct timeval tv;
    unsigned long long tstart = 0, tdiff = 0;
    time_t rawtime;
    pthread_t pt_helper;

    char buf[256] = { '\0' }, date[24] = { '\0' };
    string TRACK_lastHash;
    string SCAN_lastHash;

    cout << "INFO: Ecotrust EM Recorder v" << VERSION << " is starting" << endl;
    cout << setprecision(numeric_limits<double>::digits10 + 1);
    cerr << setprecision(numeric_limits<double>::digits10 + 1);

    if (readConfigFile(CONFIG_FILENAME)) {
        cerr << "ERROR: Failed to get configuration" << endl;
        exit(-1);
    }

    strcpy(EM_DATA.SYS_fishingArea, getConfig("fishing_area", DEFAULT_fishing_area));
    strcpy(CONFIG.vessel, getConfig("vessel", DEFAULT_vessel));
    strcpy(CONFIG.vrn, getConfig("vrn", DEFAULT_vrn));
    strcpy(CONFIG.EM_DIR, getConfig("EM_DIR", DEFAULT_EM_DIR));
    strcpy(CONFIG.OS_DISK, getConfig("OS_DISK", DEFAULT_OS_DISK));
    strcpy(CONFIG.DATA_DISK, getConfig("DATA_DISK", DEFAULT_DATA_DISK));
    strcpy(CONFIG.JSON_STATE_FILE, getConfig("JSON_STATE_FILE", DEFAULT_JSON_STATE_FILE));
    EM_DATA.SYS_numCams = atoi(getConfig("cam", DEFAULT_cam));

    if(strcmp(EM_DATA.SYS_fishingArea, "A") == 0) {
        smOptions.SetState(OPTION_USING_AD & OPTION_USING_RFID & OPTION_USING_GPS & OPTION_GPRMC_ONLY_HACK & OPTION_ANALOG_CAMERAS);
    } else if(strcmp(EM_DATA.SYS_fishingArea, "GM") == 0) {
        smOptions.SetState(OPTION_USING_AD & OPTION_USING_GPS & OPTION_IP_CAMERAS);
    } else {
        smOptions.SetState(OPTION_USING_AD & OPTION_USING_RFID & OPTION_USING_GPS & OPTION_IP_CAMERAS);
    }

    stat((string(CONFIG.DATA_DISK) + "/../").c_str(), &g_parent_st);

    EM_DATA.sm_em = &smEM;
    EM_DATA.sm_options = &smOptions;
    EM_DATA.sm_helper = &sm_helperThread;
    EM_DATA.sm_video = &sm_videoCapture;

    pthread_mutex_init(&EM_DATA.mtx, NULL);
    EM_DATA.runState = STATE_RUNNING;

    if(_AD) {
        strcpy(CONFIG.ARDUINO_DEV, getConfig("ARDUINO_DEV", DEFAULT_ARDUINO_DEV));
        strcpy(CONFIG.arduino_type, getConfig("arduino", DEFAULT_arduino));
        strcpy(CONFIG.psi_vmin, getConfig("psi_vmin", DEFAULT_psi_vmin));
        
        iADSensor.SetPort(CONFIG.ARDUINO_DEV);
        iADSensor.SetBaudRate(B9600);
        iADSensor.SetADCType(CONFIG.arduino_type, CONFIG.psi_vmin);
        iADSensor.Connect();
    }

    if(_RFID) {
        strcpy(CONFIG.RFID_DEV, getConfig("RFID_DEV", DEFAULT_RFID_DEV));

        string scanCountsFile = string(CONFIG.EM_DIR) + "/" + FN_SCAN_COUNT;
        ifstream fin(scanCountsFile.c_str());
        if(!fin.fail()) {
            fin >> EM_DATA.RFID_stringScans >> EM_DATA.RFID_tripScans;
            fin.close();
            cout << "INFO: Got previous scan counts from " << scanCountsFile << endl;
        }

        iRFIDSensor.SetPort(CONFIG.RFID_DEV);
        iRFIDSensor.SetBaudRate(B9600);
        iRFIDSensor.Connect();
    }

    if(_GPS) {
        strcpy(CONFIG.GPS_DEV, getConfig("GPS_DEV", DEFAULT_GPS_DEV));
        strcpy(EM_DATA.GPS_homePortDataFile, getConfig("HOME_PORT_DATA", DEFAULT_HOME_PORT_DATA));
        strcpy(EM_DATA.GPS_ferryDataFile, getConfig("FERRY_DATA", DEFAULT_FERRY_DATA));

        iGPSSensor.Connect();
        iGPSSensor.Receive(); // spawn the consumer thread
    }

    // spawn helper thread
    smHelperThread.SetState(STATE_RUNNING);
    pthread_create(&pt_helper, NULL, &thr_helperLoop, NULL)
    
    cout << "INFO: Created helper thread" << endl;

    signal(SIGINT, exit_handler);
    signal(SIGHUP, exit_handler);
    signal(SIGTERM, exit_handler);
    signal(SIGUSR1, reset_string_scans_handler);
    signal(SIGUSR2, reset_trip_scans_handler);

    cout << "INFO: Beginning main recording loop" << endl;

    while(_EM_RUNNING) { // only EM_DATA.runState == STATE_NOT_RUNNING can stop this
        gettimeofday(&tv, NULL);
        tstart = tv.tv_sec * 1000000 + tv.tv_usec;

        // get date
        time(&rawtime);
        g_timeinfo = localtime(&rawtime);
        strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", g_timeinfo); // writes null-terminated string so okay not to clear DATE_BUF every time
        pthread_mutex_lock(&EM_DATA.mtx);
        snprintf(EM_DATA.currentDateTime, sizeof(EM_DATA.currentDateTime), "%s.%06d", date, (unsigned int)tv.tv_usec);
        pthread_mutex_unlock(&EM_DATA.mtx);

        if(_GPS) iGPSSensor.Receive(); // a separate thread now consumes GPS data, kick it into gear if it's dead
        if(_RFID) iRFIDSensor.Receive();
        if(_AD) iADSensor.Receive();

        if(_GPS) {
            pthread_mutex_lock(&EM_DATA.mtx);
                snprintf(buf, sizeof(buf),
                    "%s,%s,%s,%.1f,%lf,%lf,%lf,%lf,%d,%d,%.2lf,%.3lf,%lu",
                    EM_DATA.SYS_fishingArea,
                    CONFIG.vrn,
                    EM_DATA.currentDateTime,
                    isnan(EM_DATA.AD_psi) ? 0 : EM_DATA.AD_psi,
                    EM_DATA.GPS_latitude,
                    EM_DATA.GPS_longitude,
                    EM_DATA.GPS_heading,
                    EM_DATA.GPS_speed,
                    EM_DATA.GPS_satsUsed,
                    EM_DATA.GPS_satQuality,
                    EM_DATA.GPS_hdop,
                    EM_DATA.GPS_eph,
                    EM_DATA.AD_state | EM_DATA.RFID_state | EM_DATA.GPS_state);
            pthread_mutex_unlock(&EM_DATA.mtx);

            TRACK_lastHash = writeLog(FN_TRACK_LOG, buf, TRACK_lastHash);
        }

        if(_RFID && EM_DATA.RFID_saveFlag) { // RFID_saveFlag is only of concern to this thread (main)
            pthread_mutex_lock(&EM_DATA.mtx);
                snprintf(buf, sizeof(buf),
                    "%s,%s,%s,%llu,%.1f,%lu,%lu,%lf,%lf,%lf,%lf,%d,%d,%.2lf,%.3lf,%lu",
                    EM_DATA.SYS_fishingArea,
                    CONFIG.vrn,
                    EM_DATA.currentDateTime,
                    EM_DATA.RFID_lastScannedTag,
                    isnan(EM_DATA.AD_psi) ? 0 : EM_DATA.AD_psi,
                    EM_DATA.RFID_stringScans,
                    EM_DATA.RFID_tripScans,
                    EM_DATA.GPS_latitude,
                    EM_DATA.GPS_longitude,
                    EM_DATA.GPS_heading,
                    EM_DATA.GPS_speed,
                    EM_DATA.GPS_satsUsed,
                    EM_DATA.GPS_satQuality,
                    EM_DATA.GPS_hdop,
                    EM_DATA.GPS_eph,
                    EM_DATA.AD_state | EM_DATA.RFID_state | EM_DATA.GPS_state);
            pthread_mutex_unlock(&EM_DATA.mtx);

            SCAN_lastHash = writeLog(FN_SCAN_LOG, buf, SCAN_lastHash);

            // no mutex around these either because only this thread ever twiddles them
            // maybe one day when all sensor reading is done in a separate thread will we need mtx
            EM_DATA.RFID_lastSavedTag = EM_DATA.RFID_lastScannedTag;
            EM_DATA.RFID_lastSaveIteration = EM_DATA.iterationTimer;
            EM_DATA.RFID_saveFlag = false;
        }

        if(_AD && EM_DATA.AD_honkSound != 0) {
            D("Honking horn");
            iADSensor.HonkMyHorn();
        }

        writeJSONState(&EM_DATA);

        pthread_mutex_lock(&EM_DATA.mtx);
            EM_DATA.iterationTimer++;
        pthread_mutex_unlock(&EM_DATA.mtx);
        
        // try to make "exactly" 1 second elapse each loop
        gettimeofday(&tv, NULL);
        tdiff = tv.tv_sec * 1000000 + tv.tv_usec - tstart;
        D("Main loop run time was " << (double)tdiff/1000 << " ms");

        if (tdiff > POLL_PERIOD) {
            tdiff = POLL_PERIOD;
            cerr << "WARN: Took longer than 1 second to get data; check sensors" << endl;
        }

        if(!REMOVE_DELAY) usleep(POLL_PERIOD - tdiff);
    }

    // smEM is now STATE_NOT_RUNNING
    
    usleep(POLL_PERIOD / 2); // let helper thread pick up on main program stopping
    // pthread_join

    if(_GPS) iGPSSensor.Close();

    if(_RFID) {
        iRFIDSensor.Close();
        ofstream fout(scanCountsFile.c_str());

        if(!fout.fail()) {
            fout << EM_DATA.RFID_stringScans << ' ' << EM_DATA.RFID_tripScans;
            fout.close();
            cout << "INFO: Wrote out scan counts to " << scanCountsFile << endl;
        }
    }

    if(_AD) iADSensor.Close();

    pthread_mutex_destroy(&EM_DATA.mtx);
    cout << "INFO: Stopped" << endl;
    
    return 0;
}

// * video recording status in UI, log files

void thr_helperLoop(void *arg) {
    string buf;
    cout << "INFO: Helper thread running" << endl;

    while(smHelperThread.GetState() & STATE_RUNNING && _EM_RUNNING) {
        D("Helper loop beginning");

        // checks for data disk, tries to mount if not there
        if(!isDataDiskThere(&EM_DATA)) {
            cout << "INFO: Data disk not mounted, attempting to mount: ";
            if(mount("/dev/em_data", CONFIG.DATA_DISK, "ext4",
                MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_NOEXEC | MS_NOSUID, NULL) == 0) {
                cout << "ok" << endl;
            } else {
                cout << "failed" << endl;
            }
        }

        // update system stats, write to log
        buf = updateSystemStats(&EM_DATA);
        writeLog(FN_SYSTEM_LOG, buf, NULL);

        // checks for special areas
        
        // checks for screenshots
        
        // spawns video capture, stops video capture
        // checks that video files are growing

        usleep(?);
    }

    D("Broke out of helper thread loop");
    smHelperThread.SetState(STATE_CLOSING_OR_UNDEFINED);
    pthread_exit(NULL);

    // *********************************** OLD
        

            if(iGPSSensor.CheckSpecialAreas() == STATE_ENCODING_PAUSED) { // pause encoding
                if(encodingState != STATE_ENCODING_PAUSED) {
                    //cout << "INFO: Entered home port (" << EM_DATA.GPS_latitude << "," << EM_DATA.GPS_longitude << "), creating pause marker " << CONFIG.PAUSE_MARKER << endl;
                    encodingState = STATE_ENCODING_PAUSED;
                }

                //FILE *fp = fopen(CONFIG.PAUSE_MARKER, "w"); fclose(fp);
            } else { // don't pause encoding
                if(encodingState != STATE_ENCODING_RUNNING) {
                    //cout << "INFO: Left home port or was never there (" << EM_DATA.GPS_latitude << "," << EM_DATA.GPS_longitude << "), removing pause marker " << CONFIG.PAUSE_MARKER << endl;
                    encodingState = STATE_ENCODING_RUNNING;
                }

                //unlink(CONFIG.PAUSE_MARKER);
            }
        }

        

        if (EM_DATA.iterationTimer > 0) {
            if(shouldTakeScreenshot(&EM_DATA)) {
                //FILE *fp = fopen(CONFIG.SCREENSHOT_MARKER, "w"); fclose(fp);
            } else {
                //unlink(CONFIG.SCREENSHOT_MARKER);
            }
        }
    }
}

string writeLog(const char *prefix, string buf, string lastHash) {
    FILE *fp;
    char date_suffix[9];
    strftime(date_suffix, sizeof(date_suffix), "%Y%m%d", g_timeinfo);

    string file = prefix + "_" + date_suffix + ".csv";

    if(lastHash != NULL) {
        string lastHashFile = prefix + ".lh";

        if(lastHash.empty()) {
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
    } else {
        buf += "\n";
    }

    if(!(fp = fopen(file.c_str(), "a"))) {
        cerr << "ERROR: Can't write to data file " << file << endl;
    } else {
        fwrite(buf.c_str(), buf.length(), 1, fp);
        fclose(fp);

        if(lastHash != NULL) {
            ofstream fout(lastHashFile.c_str());
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

    pthread_mutex_lock(&(em_data->mtx));
        snprintf(buf, sizeof(buf),
        "{ "
            "\"recorderVersion\": \"%s\", "
            "\"currentDateTime\": \"%s\", "
            "\"iterationTimer\": %lu, "
            "\"SYS\": { "
                "\"fishingArea\": \"%s\", "
                "\"numCams\": %hu, "
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

            em_data->SYS_fishingArea,
            em_data->SYS_numCams,
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
            isnan(em_data->AD_psi) ? 0 : em_data->AD_psi,
            isnan(em_data->AD_battery) ? 0 : em_data->AD_battery
        );
    pthread_mutex_unlock(&(em_data->mtx));

    ofstream fout(CONFIG.JSON_STATE_FILE);
    if (!fout.fail()) {
        fout << buf << endl;
        fout.close();
    }
}

bool isDataDiskThere(EM_DATA_TYPE *em_data) {
    struct stat st;
    struct blkid_struct_cache *blkid_cache;

    // check /mnt/data's device ID and compare it to its parent /mnt
    if(stat(CONFIG.DATA_DISK, &st) == 0 && st.st_dev != g_parent_st.st_dev) { // present
        if(!em_data->SYS_dataDiskPresent) {
            blkid_get_cache(&_blkid_cache, NULL);

            pthread_mutex_lock(&(em_data->mtx));
                em_data->SYS_dataDiskPresent = true;
                em_data->SYS_dataDiskLabel = blkid_get_tag_value(blkid_cache, "LABEL", blkid_devno_to_devname(st.st_dev));
            pthread_mutex_unlock(&(em_data->mtx));

            cout << "INFO: Data disk PRESENT, label = " << em_data->SYS_dataDiskLabel << endl;

            //em_data->SENSOR_DATA_files.push_back(string(CONFIG.DATA_DISK) + "/" + CONFIG.SENSOR_DATA);
            //em_data->SYSTEM_DATA_files.push_back(string(CONFIG.DATA_DISK) + "/" + CONFIG.SYSTEM_DATA);
            //if(_RFID) em_data->RFID_DATA_files.push_back(string(CONFIG.DATA_DISK) + "/" + CONFIG.RFID_DATA);
            
            //unlink(CONFIG.VIDEOS_DIR);
            //symlink(CONFIG.DATA_DISK, CONFIG.VIDEOS_DIR);
            //cout << "INFO: Created symlink " << CONFIG.VIDEOS_DIR << " to " << CONFIG.DATA_DISK << endl;

            return true;
        }
    } else { // not present
        if(em_data->SYS_dataDiskPresent) {
            pthread_mutex_lock(&(em_data->mtx));
                em_data->SYS_dataDiskPresent = false;
            pthread_mutex_unlock(&(em_data->mtx));
            
            blkid_gc_cache(&_blkid_cache);
            cout << "INFO: Data disk NOT PRESENT" << endl;
        
            //unlink(CONFIG.VIDEOS_DIR);
            //symlink(CONFIG.OS_DISK, CONFIG.VIDEOS_DIR);
            //cout << "INFO: Created symlink " << CONFIG.VIDEOS_DIR << " to " << CONFIG.OS_DISK << endl;
        }
    }

    return false;
}

#define PROC_STAT_VALS          7
#define DISK_USAGE_SAMPLES      24
#define DISK_USAGE_START_VAL    30240
string updateSystemStats(EM_DATA_TYPE *em_data) {
    static unsigned long long lastJiffies[PROC_STAT_VALS];
    static unsigned long osDiskLastFree, osDiskDiff, dataDiskLastFree, dataDiskDiff;
    static unsigned long osDiskMinutesLeft[DISK_USAGE_SAMPLES], dataDiskMinutesLeft[DISK_USAGE_SAMPLES];
    static unsigned long osDiskMinutesLeft_total = DISK_USAGE_SAMPLES * DISK_USAGE_START_VAL, dataDiskMinutesLeft_total = DISK_USAGE_SAMPLES * DISK_USAGE_START_VAL;
    static signed short osDiskMinutesLeft_index = -1, dataDiskMinutesLeft_index = -1;

    struct sysinfo si;
    int days, hours, mins;
    FILE *ps, *ct;
    unsigned long long jiffies[PROC_STAT_VALS] = {0}, totalJiffies = 0, temp;
    struct statvfs vfs;
    bool dataDiskPresent;
    char buf[256] = { '\0' }

    pthread_mutex_lock(&(em_data->mtx));
        dataDiskPresent = em_data->SYS_dataDiskPresent;

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
    pthread_mutex_unlock(&(em_data->mtx));

    //SYS_*DiskTotalBlocks/FreeBlocks
    if(statvfs(CONFIG.OS_DISK, &vfs) == 0) {
        
        if(osDiskLastFree >= vfs.f_bavail) {
            osDiskMinutesLeft_total -= osDiskMinutesLeft[osDiskMinutesLeft_index];
            osDiskDiff = osDiskLastFree - vfs.f_bavail;
            if(osDiskDiff <= 0) osDiskMinutesLeft[osDiskMinutesLeft_index] = osDiskMinutesLeft[osDiskMinutesLeft_index - 1];
            else osDiskMinutesLeft[osDiskMinutesLeft_index] = (int)((double)vfs.f_bavail / (double)(60000000 / POLL_PERIOD / SPECIAL_INTERVAL * (osDiskLastFree - vfs.f_bavail)));
            osDiskMinutesLeft_total += osDiskMinutesLeft[osDiskMinutesLeft_index];
            
            osDiskMinutesLeft_index++;
            if(osDiskMinutesLeft_index >= DISK_USAGE_SAMPLES) osDiskMinutesLeft_index = 0;

            pthread_mutex_lock(&(em_data->mtx));
                em_data->SYS_osDiskMinutesLeft = osDiskMinutesLeft_total / DISK_USAGE_SAMPLES;
            pthread_mutex_unlock(&(em_data->mtx));
        }

        pthread_mutex_lock(&(em_data->mtx));
            em_data->SYS_osDiskTotalBlocks = vfs.f_blocks;
            em_data->SYS_osDiskFreeBlocks = osDiskLastFree = vfs.f_bavail;
        pthread_mutex_unlock(&(em_data->mtx));
    }

    if(dataDiskPresent) {
        if(statvfs(CONFIG.DATA_DISK, &vfs) == 0) {
            if(dataDiskLastFree >= vfs.f_bavail) {
                dataDiskMinutesLeft_total -= dataDiskMinutesLeft[dataDiskMinutesLeft_index];
                dataDiskDiff = dataDiskLastFree - vfs.f_bavail;
                if(dataDiskDiff <= 0) dataDiskMinutesLeft[dataDiskMinutesLeft_index] = dataDiskMinutesLeft[dataDiskMinutesLeft_index - 1];
                else dataDiskMinutesLeft[dataDiskMinutesLeft_index] = (int)((double)vfs.f_bavail / (double)(60000000 / POLL_PERIOD / SPECIAL_INTERVAL * dataDiskDiff));
                dataDiskMinutesLeft_total += dataDiskMinutesLeft[dataDiskMinutesLeft_index];
                
                dataDiskMinutesLeft_index++;
                if(dataDiskMinutesLeft_index >= DISK_USAGE_SAMPLES) dataDiskMinutesLeft_index = 0;
                pthread_mutex_lock(&(em_data->mtx));
                    em_data->SYS_dataDiskMinutesLeft = dataDiskMinutesLeft_total / DISK_USAGE_SAMPLES;
                pthread_mutex_unlock(&(em_data->mtx));
            }

            // this is kind of a hack
            string hourCountsFile = string(CONFIG.DATA_DISK) + "/hour_counter.dat";
            ifstream fin(hourCountsFile.c_str());
            if(!fin.fail()) {
                pthread_mutex_lock(&(em_data->mtx));
                    fin >> em_data->SYS_dataDiskMinutesLeftFake;

                    if(em_data->SYS_dataDiskMinutesLeftFake > DATA_DISK_FAKE_DAYS_START * 24) {
                        em_data->SYS_dataDiskMinutesLeftFake = 0;
                    } else {
                        em_data->SYS_dataDiskMinutesLeftFake = (DATA_DISK_FAKE_DAYS_START * 24 - em_data->SYS_dataDiskMinutesLeftFake) * 60;
                    }
                pthread_mutex_unlock(&(em_data->mtx));

                fin.close();
            } else {
                pthread_mutex_lock(&(em_data->mtx));
                    em_data->SYS_dataDiskMinutesLeftFake = DATA_DISK_FAKE_DAYS_START * 24 * 60;
                pthread_mutex_unlock(&(em_data->mtx));
            }

            pthread_mutex_lock(&(em_data->mtx));
                em_data->SYS_dataDiskTotalBlocks = vfs.f_blocks;
                em_data->SYS_dataDiskFreeBlocks = dataDiskLastFree = vfs.f_bavail;
            pthread_mutex_unlock(&(em_data->mtx));
        }
    }

    // SYSTEM_DATA.csv
    snprintf(buf, sizeof(buf),
        "%s,%s,%s,%lu,%0.02f,%0.02f,%0.02f,%.1f,%u,%u,%llu,%llu,%lu,%lu\n",
        em_data->SYS_fishingArea,
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

    return string(buf);
}

bool shouldTakeScreenshot(EM_DATA_TYPE *em_data) {
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
    smEM.SetState(STATE_NOT_RUNNING);
}