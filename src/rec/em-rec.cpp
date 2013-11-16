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
#include "CaptureManager.h"
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
#include <sys/wait.h>
#include <pthread.h>
#include <errno.h>

using namespace std;

CONFIG_TYPE CONFIG = {};
EM_DATA_TYPE EM_DATA = {};

GPSSensor iGPSSensor(&EM_DATA);
RFIDSensor iRFIDSensor(&EM_DATA);
ADSensor iADSensor(&EM_DATA);

StateMachine smRecorder(STATE_NOT_RUNNING, SM_EXCLUSIVE_STATES);
StateMachine smOptions(0, SM_MULTIPLE_STATES);
StateMachine smSystem(0, SM_MULTIPLE_STATES);
StateMachine smHelperThread(STATE_NOT_RUNNING, SM_EXCLUSIVE_STATES);
StateMachine smTakeScreenshot(false, SM_EXCLUSIVE_STATES);

unsigned long IGNORED_STATES = GPS_NO_FERRY_DATA |
                               GPS_NO_HOME_PORT_DATA |
                               GPS_INSIDE_HOME_PORT |
                               AD_BATTERY_LOW |
                               AD_BATTERY_HIGH |
                               RFID_CHECKSUM_FAILED;

struct stat G_parent_st;
struct tm *G_timeinfo;

int main(int argc, char *argv[]) {
    struct timeval tv;
    unsigned long long tstart = 0, tdiff = 0;
    time_t rawtime;
    pthread_t pt_helper;

    int retVal;
    char buf[256], date[24] = { '\0' };
    string TRACK_lastHash;
    string SCAN_lastHash;
    string tmp;

    cout << "INFO: Ecotrust EM Recorder v" << VERSION << " is starting" << endl;
    cout << setprecision(numeric_limits<double>::digits10 + 1);
    cerr << setprecision(numeric_limits<double>::digits10 + 1);

    if (readConfigFile(FN_CONFIG)) {
        cerr << C_RED << "ERROR: Failed to get configuration" << C_RESET << endl;
        exit(-1);
    }

    EM_DATA.SYS_fishingArea = getConfig("fishing_area", DEFAULT_fishing_area);
    CONFIG.vessel = getConfig("vessel", DEFAULT_vessel);
    CONFIG.vrn = getConfig("vrn", DEFAULT_vrn);
    CONFIG.EM_DIR = getConfig("EM_DIR", DEFAULT_EM_DIR);
    CONFIG.OS_DISK = getConfig("OS_DISK", DEFAULT_OS_DISK);
    CONFIG.DATA_DISK = getConfig("DATA_DISK", DEFAULT_DATA_DISK);
    CONFIG.JSON_STATE_FILE = getConfig("JSON_STATE_FILE", DEFAULT_JSON_STATE_FILE);
    EM_DATA.SYS_numCams = atoi(getConfig("cam", DEFAULT_cam).c_str());

    if(EM_DATA.SYS_fishingArea == "A") {
        smOptions.SetState(OPTION_USING_AD | OPTION_USING_RFID | OPTION_USING_GPS | OPTION_GPRMC_ONLY_HACK | OPTION_ANALOG_CAMERAS);
    } else if(EM_DATA.SYS_fishingArea == "GM") {
        smOptions.SetState(OPTION_USING_AD | OPTION_USING_GPS | OPTION_IP_CAMERAS);
        IGNORED_STATES = IGNORED_STATES | RFID_NO_CONNECTION;
    } else {
        smOptions.SetState(OPTION_USING_AD | OPTION_USING_RFID | OPTION_USING_GPS | OPTION_IP_CAMERAS);
    }

    stat((CONFIG.DATA_DISK + "/../").c_str(), &G_parent_st);

    EM_DATA.sm_recorder = &smRecorder;
    EM_DATA.sm_options = &smOptions;
    EM_DATA.sm_system = &smSystem;
    EM_DATA.sm_helper = &smHelperThread;

    pthread_mutex_init(&EM_DATA.mtx, NULL);

    smRecorder.UnsetState(STATE_NOT_RUNNING); smRecorder.SetState(STATE_RUNNING);

    if(_AD) {
        CONFIG.ARDUINO_DEV = getConfig("ARDUINO_DEV", DEFAULT_ARDUINO_DEV);
        CONFIG.arduino_type = getConfig("arduino", DEFAULT_arduino);
        CONFIG.psi_vmin = getConfig("psi_vmin", DEFAULT_psi_vmin);
        
        iADSensor.SetPort(CONFIG.ARDUINO_DEV.c_str());
        iADSensor.SetBaudRate(B9600);
        iADSensor.SetADCType(CONFIG.arduino_type.c_str(), CONFIG.psi_vmin.c_str());
        iADSensor.Connect();
    }

    if(_RFID) {
        CONFIG.RFID_DEV = getConfig("RFID_DEV", DEFAULT_RFID_DEV);

        iRFIDSensor.SetPort(CONFIG.RFID_DEV.c_str());
        iRFIDSensor.SetBaudRate(B9600);
        iRFIDSensor.SetScanCountsFile(CONFIG.EM_DIR + "/" + FN_SCAN_COUNT);
        iRFIDSensor.Connect();
    }

    if(_GPS) {
        CONFIG.GPS_DEV = getConfig("GPS_DEV", DEFAULT_GPS_DEV);
        EM_DATA.GPS_homePortDataFile = getConfig("HOME_PORT_DATA", DEFAULT_HOME_PORT_DATA);
        EM_DATA.GPS_ferryDataFile = getConfig("FERRY_DATA", DEFAULT_FERRY_DATA);

        iGPSSensor.Connect();
        iGPSSensor.Receive(); // spawn the consumer thread
    }

    // spawn helper thread
    smHelperThread.SetState(STATE_RUNNING);
    if((retVal = pthread_create(&pt_helper, NULL, &thr_helperLoop, NULL)) != 0) {
        smHelperThread.SetState(STATE_NOT_RUNNING);
        cerr << C_RED << "ERROR: Couldn't create main helper thread" << C_RESET << endl;
    }
    
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
        G_timeinfo = localtime(&rawtime);
        strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", G_timeinfo); // writes null-terminated string so okay not to clear DATE_BUF every time
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
                    EM_DATA.SYS_fishingArea.c_str(),
                    CONFIG.vrn.c_str(),
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
                    iADSensor.GetState() | iRFIDSensor.GetState() | iGPSSensor.GetState() | smSystem.GetState());
            pthread_mutex_unlock(&EM_DATA.mtx);

            TRACK_lastHash = writeLog(string(CONFIG.OS_DISK) + "/" + FN_TRACK_LOG, buf, TRACK_lastHash);
        }

        if(_RFID && EM_DATA.RFID_saveFlag) { // RFID_saveFlag is only of concern to this thread (main)
            pthread_mutex_lock(&EM_DATA.mtx);
                snprintf(buf, sizeof(buf),
                    "%s,%s,%s,%llu,%.1f,%lu,%lu,%lf,%lf,%lf,%lf,%d,%d,%.2lf,%.3lf,%lu",
                    EM_DATA.SYS_fishingArea.c_str(),
                    CONFIG.vrn.c_str(),
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
                    iADSensor.GetState() | iRFIDSensor.GetState() | iGPSSensor.GetState() | smSystem.GetState());
            pthread_mutex_unlock(&EM_DATA.mtx);

            SCAN_lastHash = writeLog(string(CONFIG.OS_DISK) + "/" + FN_SCAN_LOG, buf, SCAN_lastHash);

            // no mutex around these either because only this thread ever twiddles them
            // maybe one day when all sensor reading is done in a separate thread will we need mtx
            EM_DATA.RFID_lastSavedTag = EM_DATA.RFID_lastScannedTag;
            pthread_mutex_lock(&EM_DATA.mtx);
                EM_DATA.RFID_lastSaveIteration = EM_DATA.runIterations;
            pthread_mutex_unlock(&EM_DATA.mtx);
            EM_DATA.RFID_saveFlag = false;
        }

        if(_AD && EM_DATA.AD_honkSound != 0) { D("Honking horn");
            iADSensor.HonkMyHorn();
        }

        writeJSONState(&EM_DATA);

        pthread_mutex_lock(&EM_DATA.mtx);
            EM_DATA.runIterations++;
        pthread_mutex_unlock(&EM_DATA.mtx);
        
        // try to make "exactly" 1 second elapse each loop
        gettimeofday(&tv, NULL);
        tdiff = tv.tv_sec * 1000000 + tv.tv_usec - tstart; D("Main loop run time was " << (double)tdiff/1000 << " ms");

        if (tdiff > POLL_PERIOD) {
            tdiff = POLL_PERIOD;
            cerr << C_YELLOW << "WARN: Took longer than 1 second to get data; check sensors" << C_RESET << endl;
        }

        usleep(POLL_PERIOD - tdiff);
    }

    // smRecorder is now STATE_NOT_RUNNING

    if(pthread_join(pt_helper, NULL) == 0) {
        smHelperThread.SetState(STATE_NOT_RUNNING);
        cout << "INFO: Helper thread stopped" << endl;
    }

    if(_GPS) iGPSSensor.Close();
    if(_RFID) iRFIDSensor.Close();
    if(_AD) iADSensor.Close();

    pthread_mutex_destroy(&EM_DATA.mtx);
    cout << "INFO: Stopped" << endl;
    
    return 0;
}

void *thr_helperLoop(void *arg) {
    const unsigned int HELPER_PERIOD = HELPER_INTERVAL * POLL_PERIOD;
    const char *scrot_envp[] = { "DISPLAY=:0", NULL };
    CaptureManager cmVideo(&EM_DATA, CONFIG.EM_DIR + FN_VIDEO_DIR);
    //unsigned long loopIterations = 0;
    struct timeval tv;
    unsigned long long tstart = 0, tdiff = 0;
    string buf;
    double latitude, longitude;
    pid_t pScrot;

    usleep(5 * POLL_PERIOD); // let things settle down, let the GPS info get collected, etc.

    while(smHelperThread.GetState() & STATE_RUNNING && _EM_RUNNING) { D2("Helper loop beginning");
        gettimeofday(&tv, NULL);
        tstart = tv.tv_sec * 1000000 + tv.tv_usec;

        // checks for data disk, tries to mount if not there
        if(!isDataDiskThere(&EM_DATA)) {
            cout << "INFO: Data disk not mounted, attempting to mount" << endl;
            if(mount("/dev/em_data", CONFIG.DATA_DISK.c_str(), "ext4",
                MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_NOEXEC | MS_NOSUID, NULL) == 0) {
                cout << "INFO: Mount success" << endl;
            } else {
                cerr << C_RED << "ERROR: Mount failed" << C_RESET << endl;
            }
        }

        // update system stats, write to log
        buf = updateSystemStats(&EM_DATA);
        writeLog(string(CONFIG.OS_DISK) + "/" + FN_SYSTEM_LOG, buf);
    
        // process position, set special area states, and if we're in the home port ...
        if(iGPSSensor.InSpecialArea() && iGPSSensor.GetState() & GPS_INSIDE_HOME_PORT) { // pause
            // once either of the video capture routines is able to get video data, they set
            // SYS_VIDEO_AVAILABLE, and at that point we can actually play conductor; when we don't
            // know if video is available we keep trying to start it
            if(smSystem.GetState() & SYS_VIDEO_AVAILABLE) {
                if(!(cmVideo.GetState() & STATE_NOT_RUNNING)) {
                    pthread_mutex_lock(&EM_DATA.mtx);
                        latitude = EM_DATA.GPS_latitude;
                        longitude = EM_DATA.GPS_longitude;
                    pthread_mutex_unlock(&EM_DATA.mtx);
                    cout << "INFO: Entered home port (" << latitude << "," << longitude << "), pausing video capture" << endl;
                }

                cmVideo.Stop();
            } else {
                cmVideo.Start();
            }
        } else {
            if(!(_OS_DISK_FULL)) {
                if(!(cmVideo.GetState() & STATE_RUNNING)) { // resume
                    pthread_mutex_lock(&EM_DATA.mtx);
                        latitude = EM_DATA.GPS_latitude;
                        longitude = EM_DATA.GPS_longitude;
                    pthread_mutex_unlock(&EM_DATA.mtx);
                    cout << "INFO: Left home port or was never there (" << latitude << "," << longitude << "), starting video capture" << endl;
                }

                cmVideo.Start(); // what this program is really all about =)
            }
        }

        // checks for errors that are screenshot-worthy
        if((iADSensor.GetState() | iRFIDSensor.GetState() | iGPSSensor.GetState()) & ~(IGNORED_STATES)) {
            // state needs to show up HELPER_INNER_INTERVAL times before it is exposed
            // since HELPER_INTERVAL (this main loop's run interval) is 5 seconds, this means
            // 5*24 = 2 minutes as it's currently set
            smTakeScreenshot.SetState(true, HELPER_INNER_INTERVAL);
        } else {
            // errors have to be on screen for a continuous HELPER_INNER_INTERVALs,
            // a single interruption clears the state
            smTakeScreenshot.UnsetAllStates();
        }
        
        if(smTakeScreenshot.GetState() == true) {
            pScrot = fork();

            if(pScrot == 0) {
                // for some reason the parameter given immediately after the executable is ignored; it's more likely
                // that scrot is doing something funky in its command-line parsing rather than execle misbehaving, but
                // there it is; keep the "" in there otherwise quality defaults to 75 (makes big files)
                execle("/usr/bin/scrot", "", "-q5", (CONFIG.OS_DISK + "/screenshots/%Y%m%d-%H%M%S.jpg").c_str(), NULL, scrot_envp);
            } else if(pScrot > 0) {
                waitpid(pScrot, NULL, 0);
                smTakeScreenshot.UnsetAllStates();
                D2("Took screenshot");
            } else {
                cerr << C_RED << "ERROR: /usr/bin/scrot fork() failed" << C_RESET << endl;
            }
        }

        // try to make "exactly" 1 second elapse each loop
        gettimeofday(&tv, NULL);
        tdiff = tv.tv_sec * 1000000 + tv.tv_usec - tstart; D2("Helper thread loop run time was " << (double)tdiff/1000 << " ms");

        if (tdiff > HELPER_PERIOD) {
            tdiff = HELPER_PERIOD;
            cerr << C_YELLOW << "WARN: Helper thread loop took longer than " << HELPER_INTERVAL << " POLL_PERIODs" << C_RESET << endl;
        }

        // check twice so we don't hold up shutdowns in our long wait period
        if (smHelperThread.GetState() & STATE_RUNNING && _EM_RUNNING) usleep((HELPER_PERIOD - tdiff) / 2);
        if (smHelperThread.GetState() & STATE_RUNNING && _EM_RUNNING) usleep((HELPER_PERIOD - tdiff) / 2);

        //loopIterations++;
    } D2("Broke out of helper thread loop");
    
    cmVideo.Stop();

    smHelperThread.SetState(STATE_CLOSING_OR_UNDEFINED);
    pthread_exit(NULL);
}

void writeLog(string prefix, string buf) {
    if(!(_OS_DISK_FULL)) {
        FILE *fp;
        char date_suffix[9];
        strftime(date_suffix, sizeof(date_suffix), "%Y%m%d", G_timeinfo);
        string file = prefix + "_" + date_suffix + ".csv";

        buf += "\n";

        if(!(fp = fopen(file.c_str(), "a"))) {
            cerr << C_RED << "ERROR: Can't write to data file " << file << C_RESET << endl;
        } else {
            if(fwrite(buf.c_str(), buf.length(), 1, fp) < 1 && errno == ENOSPC) {
                D("fwrite error, setting OS_DISK_FULL");
                smSystem.SetState(SYS_OS_DISK_FULL);
            } 
            fclose(fp);
        }
    }
}

string writeLog(string prefix, string buf, string lastHash) {
    string lastHashFile = prefix + ".lh";
    string newHash;

    if(lastHash.empty()) {
        cout << "INFO: Seeding last hash from " << lastHashFile << endl;

        ifstream fin(lastHashFile.c_str());
        if(!fin.fail()) {
            fin >> lastHash;
            fin.close();
        }
    }

    if(!(_OS_DISK_FULL)) {
        MD5 md5 = MD5(MD5_SALT + buf + lastHash);
        newHash = md5.hexdigest();
        buf += "*" + newHash;

        writeLog(prefix, buf);

        ofstream fout(lastHashFile.c_str());
        if (!fout.fail()) {
            fout << newHash << endl;
            fout.close();
        }
    } else {
        newHash = lastHash;
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
            "\"runIterations\": %lu, "
            "\"SYS\": { "
                "\"state\": %lu, "
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
            em_data->runIterations,

            smSystem.GetState(),
            em_data->SYS_fishingArea.c_str(),
            em_data->SYS_numCams,
            em_data->SYS_uptime.c_str(),
            em_data->SYS_load.c_str(),
            isnan(em_data->SYS_cpuPercent) ? 0 : em_data->SYS_cpuPercent,
            em_data->SYS_ramFreeKB,
            em_data->SYS_ramTotalKB,
            em_data->SYS_tempCore0,
            em_data->SYS_tempCore1,
            em_data->SYS_dataDiskPresent ? "true" : "false",
            em_data->SYS_osDiskFreeBlocks,
            em_data->SYS_osDiskTotalBlocks,
            em_data->SYS_osDiskMinutesLeft,
            em_data->SYS_dataDiskLabel.c_str(),
            em_data->SYS_dataDiskFreeBlocks,
            em_data->SYS_dataDiskTotalBlocks,
            em_data->SYS_dataDiskMinutesLeft,
            em_data->SYS_dataDiskMinutesLeftFake,

            iGPSSensor.GetState(),
            em_data->GPS_latitude,
            em_data->GPS_longitude,
            em_data->GPS_heading,
            em_data->GPS_speed,
            em_data->GPS_satQuality,
            em_data->GPS_satsUsed,
            em_data->GPS_hdop,
            em_data->GPS_eph,

            iRFIDSensor.GetState(),
            em_data->RFID_lastScannedTag,
            em_data->RFID_stringScans,
            em_data->RFID_tripScans,

            iADSensor.GetState(),
            isnan(em_data->AD_psi) ? 0 : em_data->AD_psi,
            isnan(em_data->AD_battery) ? 0 : em_data->AD_battery
        );
    pthread_mutex_unlock(&(em_data->mtx));

    ofstream fout(CONFIG.JSON_STATE_FILE.c_str());
    if (!fout.fail()) {
        fout << buf << endl;
        fout.close();
    }
}

bool isDataDiskThere(EM_DATA_TYPE *em_data) {
    struct stat st;
    struct blkid_struct_cache *blkid_cache;

    // check /mnt/data's device ID and compare it to its parent /mnt
    if(stat(CONFIG.DATA_DISK.c_str(), &st) == 0 && st.st_dev != G_parent_st.st_dev) { // present
        if(!em_data->SYS_dataDiskPresent) {
            blkid_get_cache(&blkid_cache, NULL);

            pthread_mutex_lock(&(em_data->mtx));
                em_data->SYS_dataDiskPresent = true;
                em_data->SYS_dataDiskLabel = blkid_get_tag_value(blkid_cache, "LABEL", blkid_devno_to_devname(st.st_dev));
            pthread_mutex_unlock(&(em_data->mtx));

            cout << "INFO: Data disk PRESENT, label = " << em_data->SYS_dataDiskLabel << endl;
        }

        return true;
    } else { // not present
        if(em_data->SYS_dataDiskPresent) {
            pthread_mutex_lock(&(em_data->mtx));
                em_data->SYS_dataDiskPresent = false;
            pthread_mutex_unlock(&(em_data->mtx));
            
            blkid_gc_cache(blkid_cache);
            cout << "INFO: Data disk NOT PRESENT" << endl;
        }
    }

    return false;
}

#define PROC_STAT_VALS          7
#define DISK_USAGE_SAMPLES      12
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
    char buf[256];

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
            snprintf(buf, sizeof(buf), "%dd%dh%dm", days, hours, mins);
            em_data->SYS_uptime = buf;
            
            // SYS_ramFree/Total
            em_data->SYS_ramFreeKB = si.freeram * (unsigned long long)si.mem_unit / 1024;
            em_data->SYS_ramTotalKB = si.totalram * (unsigned long long)si.mem_unit / 1024;
            
            // SYS_load
            snprintf(buf, sizeof(buf), "%0.02f %0.02f %0.02f", (float)si.loads[0] / (float)(1<<SI_LOAD_SHIFT), (float)si.loads[1] / (float)(1<<SI_LOAD_SHIFT), (float)si.loads[2] / (float)(1<<SI_LOAD_SHIFT));
            em_data->SYS_load = buf;
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
    if(statvfs(CONFIG.OS_DISK.c_str(), &vfs) == 0) {
        if(osDiskLastFree >= vfs.f_bavail) {
            osDiskMinutesLeft_total -= osDiskMinutesLeft[osDiskMinutesLeft_index];
            osDiskDiff = osDiskLastFree - vfs.f_bavail;
            if(osDiskDiff <= 0) osDiskMinutesLeft[osDiskMinutesLeft_index] = osDiskMinutesLeft[osDiskMinutesLeft_index - 1];
            else osDiskMinutesLeft[osDiskMinutesLeft_index] = (int)((double)vfs.f_bavail / (double)(60000000 / POLL_PERIOD / HELPER_INTERVAL * (osDiskLastFree - vfs.f_bavail)));
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

        // if less than 16 MB left (assuming 4K block size)
        if(vfs.f_bavail < 4096 && osDiskMinutesLeft_index > 1) {
            smSystem.SetState(SYS_OS_DISK_FULL);
            D("Set OS_DISK_FULL");
        } else {
            smSystem.UnsetState(SYS_OS_DISK_FULL);
        }
    }

    if(dataDiskPresent) {
        if(statvfs(CONFIG.DATA_DISK.c_str(), &vfs) == 0) {
            if(dataDiskLastFree >= vfs.f_bavail) {
                dataDiskMinutesLeft_total -= dataDiskMinutesLeft[dataDiskMinutesLeft_index];
                dataDiskDiff = dataDiskLastFree - vfs.f_bavail;
                // since we store things on the OS disk before moving them to the data disk (buffering), the total
                // space "consumed" on the data disk is whatever it is now + whatever it will be in the near future,
                // meaning that whatever was consumed on the OS disk is tacked on
                dataDiskDiff += osDiskDiff;

                // if nothing was written, or something was deleted
                if(dataDiskDiff <= 0) {
                    // use last difference value
                    dataDiskMinutesLeft[dataDiskMinutesLeft_index] = dataDiskMinutesLeft[dataDiskMinutesLeft_index - 1];
                } else {
                    dataDiskMinutesLeft[dataDiskMinutesLeft_index] = (int)((double)vfs.f_bavail / (double)(60000000 / POLL_PERIOD / HELPER_INTERVAL * dataDiskDiff));
                }

                dataDiskMinutesLeft_total += dataDiskMinutesLeft[dataDiskMinutesLeft_index];
                
                dataDiskMinutesLeft_index++;
                if(dataDiskMinutesLeft_index >= DISK_USAGE_SAMPLES) dataDiskMinutesLeft_index = 0;
                pthread_mutex_lock(&(em_data->mtx));
                    // the real calculation
                    em_data->SYS_dataDiskMinutesLeft = dataDiskMinutesLeft_total / DISK_USAGE_SAMPLES;

                    // the fake calculation that bases data disk consumption rate on the OS disk, since we now buffer things there
                    // actually this sucks, forget it
                    //em_data->SYS_dataDiskMinutesLeft = (int)(((double)em_data->SYS_osDiskMinutesLeft / (double)em_data->SYS_osDiskFreeBlocks) * (double)vfs.f_bavail);
                pthread_mutex_unlock(&(em_data->mtx));
            }

            // fake hours calculation for Area A; this is kind of a hack
            ////////////////////////////////////////////////////////////
            unsigned long hours;
            string hourCountsFile = string(CONFIG.DATA_DISK.c_str()) + "/hour_counter.dat";
            ifstream fin(hourCountsFile.c_str());
            if(!fin.fail()) {
                fin >> hours;
                pthread_mutex_lock(&(em_data->mtx));
                    em_data->SYS_dataDiskMinutesLeftFake = hours;

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
            /////////////////////////////////////////////////////////////

            pthread_mutex_lock(&(em_data->mtx));
                em_data->SYS_dataDiskTotalBlocks = vfs.f_blocks;
                em_data->SYS_dataDiskFreeBlocks = dataDiskLastFree = vfs.f_bavail;
            pthread_mutex_unlock(&(em_data->mtx));

            if(vfs.f_bavail < 4096 && dataDiskMinutesLeft_index > 1) {
               smSystem.SetState(SYS_DATA_DISK_FULL);
               D("Set DATA_DISK_FULL");
            } else {
               smSystem.UnsetState(SYS_DATA_DISK_FULL);
            }
        }
    }

    // SYSTEM_DATA.csv
    snprintf(buf, sizeof(buf),
        "%s,%s,%s,%lu,%0.02f,%0.02f,%0.02f,%.1f,%u,%u,%llu,%llu,%lu,%lu,%u",
        em_data->SYS_fishingArea.c_str(),
        CONFIG.vrn.c_str(),
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
        em_data->SYS_dataDiskFreeBlocks * 4096 / 1024 / 1024,
        smSystem.GetState() & SYS_VIDEO_RECORDING ? 1 : 0);

    return string(buf);
}

void reset_string_scans_handler(int s) { D("Resetting stringScans");
    iRFIDSensor.resetStringScans();
}

void reset_trip_scans_handler(int s) { D("Resetting tripScans");
    iRFIDSensor.resetTripScans();
}

void exit_handler(int s) {
    cout << "INFO: Ecotrust EM Recorder v" << VERSION << " is stopping ..." << endl;
    smRecorder.UnsetState(STATE_RUNNING); smRecorder.SetState(STATE_NOT_RUNNING);
}
