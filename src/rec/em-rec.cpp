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
#include "output.h"

#include <sstream>
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

CONFIG_TYPE     G_CONFIG = {};
EM_DATA_TYPE    G_EM_DATA = {};
bool            G_ARG_DUMP_CONFIG = false;
struct stat     G_parent_st;
struct tm      *G_timeinfo;
pthread_mutex_t G_mtxOut;
unsigned long   G_IGNORED_STATES = GPS_NO_FERRY_DATA |
                                   GPS_NO_HOME_PORT_DATA |
                                   GPS_IN_HOME_PORT |
                                   AD_BATTERY_LOW |
                                   AD_BATTERY_HIGH |
                                   RFID_CHECKSUM_FAILED |
                                   SYS_DATA_DISK_PRESENT |
                                   SYS_TARGET_DISK_WRITABLE |
                                   SYS_VIDEO_RECORDING |
                                   SYS_REDUCED_VIDEO_BITRATE;

GPSSensor  iGPSSensor(&G_EM_DATA);
RFIDSensor iRFIDSensor(&G_EM_DATA);
ADSensor   iADSensor(&G_EM_DATA);

StateMachine smRecorder(STATE_NOT_RUNNING, SM_EXCLUSIVE_STATES);
StateMachine smOptions(0, SM_MULTIPLE_STATES);
StateMachine smSystem(0, SM_MULTIPLE_STATES);
StateMachine smAuxThread(STATE_NOT_RUNNING, SM_EXCLUSIVE_STATES);
StateMachine smTakeScreenshot(false, SM_EXCLUSIVE_STATES);

__thread unsigned short __threadId;
string moduleName = "";

int main(int argc, char *argv[]) {
    if(argc > 0) {
        // rudimentary argument processing
        for(int i = 0; i < argc; i++) {
            if(strcmp(argv[i], "--dump-config") == 0) { G_ARG_DUMP_CONFIG = true; }
            //else if(strcmp(argv[i], "") == 0) { exit(0); }
            //else if(strcmp(argv[i], "") == 0) { }
            //else if(strcmp(argv[i], "") == 0) { }
        }
    }
    
    if (readConfigFile(FN_CONFIG)) exit(-1);

    G_EM_DATA.SYS_fishingArea = getConfig("fishing_area", DEFAULT_fishing_area);
    G_EM_DATA.SYS_RFID = getConfig("rfid", DEFAULT_rfid);
    G_EM_DATA.SYS_videoType = getConfig("video_type", DEFAULT_video_type);
    G_CONFIG.vessel = getConfig("vessel", DEFAULT_vessel);
    G_CONFIG.vrn = getConfig("vrn", DEFAULT_vrn);
    G_CONFIG.arduino_type = getConfig("arduino", DEFAULT_arduino);
    G_CONFIG.psi_vmin = getConfig("psi_vmin", DEFAULT_psi_vmin);
    G_CONFIG.psi_low_threshold = atoi(getConfig("psi_low_threshold", DEFAULT_psi_low_threshold).c_str());
    G_CONFIG.psi_high_threshold = atoi(getConfig("psi_high_threshold", DEFAULT_psi_high_threshold).c_str());
    G_CONFIG.fps_low_delay = atoi(getConfig("fps_low_delay", DEFAULT_fps_low_delay).c_str());
    G_EM_DATA.SYS_numCams = atoi(getConfig("cam", DEFAULT_cam).c_str());
    G_CONFIG.EM_DIR = getConfig("EM_DIR", DEFAULT_EM_DIR);
    G_CONFIG.OS_DISK = getConfig("OS_DISK", DEFAULT_OS_DISK);
    G_CONFIG.DATA_DISK = getConfig("DATA_DISK", DEFAULT_DATA_DISK);
    G_CONFIG.JSON_STATE_FILE = getConfig("JSON_STATE_FILE", DEFAULT_JSON_STATE_FILE);
    G_CONFIG.ARDUINO_DEV = getConfig("ARDUINO_DEV", DEFAULT_ARDUINO_DEV);
    G_CONFIG.RFID_DEV = getConfig("RFID_DEV", DEFAULT_RFID_DEV);
    G_CONFIG.GPS_DEV = getConfig("GPS_DEV", DEFAULT_GPS_DEV);
    G_EM_DATA.GPS_homePortDataFile = getConfig("HOME_PORT_DATA", DEFAULT_HOME_PORT_DATA);
    G_EM_DATA.GPS_ferryDataFile = getConfig("FERRY_DATA", DEFAULT_FERRY_DATA);
    
    if(G_ARG_DUMP_CONFIG) exit(0);

    __threadId = THREAD_MAIN;
    I("Ecotrust EM Recorder v" + VERSION + " is starting");

    struct timeval tv;
    unsigned long long tstart = 0, tdiff = 0;
    time_t rawtime;
    pthread_t pt_aux;

    int retVal;
    char buf[256], date[24] = { '\0' };
    unsigned long allStates = 0;
    unsigned long sm_options = 0x00000000;
    string TRACK_lastHash;
    string SCAN_lastHash;
    string targetDisk;

    pthread_mutex_init(&G_EM_DATA.mtx, NULL);
    pthread_mutex_init(&G_mtxOut, NULL);

    cout << setprecision(numeric_limits<double>::digits10 + 1);
    cerr << setprecision(numeric_limits<double>::digits10 + 1);

    // deprecated.
    if(G_EM_DATA.SYS_fishingArea == "A") {
        smOptions.SetState(OPTIONS_USING_AD | OPTIONS_USING_RFID | OPTIONS_USING_GPS | OPTIONS_GPRMC_ONLY_HACK | OPTIONS_USING_ANALOG_CAMERAS);
        G_EM_DATA.SYS_numCams = 1; // forced, until we support more than two cams
        cout << "deprecation warning: please use rfid=yes and video=analog instead of specifying fishing_area=A.\n";
    } else if(G_EM_DATA.SYS_fishingArea == "GM") {
        smOptions.SetState(OPTIONS_USING_AD | OPTIONS_USING_GPS | OPTIONS_USING_DIGITAL_CAMERAS);
        G_IGNORED_STATES = G_IGNORED_STATES | RFID_NO_CONNECTION;
        cout << "deprecation warning: please use rfid=no and video=digital instead of specifying fishing_area=GM.\n";
    } else if(G_EM_DATA.SYS_fishingArea == "QIN") {
        smOptions.SetState(OPTIONS_USING_AD | OPTIONS_USING_RFID | OPTIONS_USING_GPS | OPTIONS_USING_DIGITAL_CAMERAS);
        cout << "deprecation warning: please use rfid=yes and video=digital instead of specifying fishing_area=QIN.\n";
    } else {

        //smOptions.SetState(OPTIONS_USING_AD | OPTIONS_USING_RFID | OPTIONS_USING_GPS | OPTIONS_USING_DIGITAL_CAMERAS);
        sm_options = sm_options | OPTIONS_USING_AD | OPTIONS_USING_GPS;

        if (G_EM_DATA.SYS_RFID == "yes") {
            sm_options = sm_options | OPTIONS_USING_RFID;
        } else {
            G_IGNORED_STATES = G_IGNORED_STATES | RFID_NO_CONNECTION;
        }
        O("video type:")
        O(G_EM_DATA.SYS_videoType);
        if (G_EM_DATA.SYS_videoType == "analog") {
            sm_options = sm_options | OPTIONS_USING_ANALOG_CAMERAS;
        } else if (G_EM_DATA.SYS_videoType == "digital") {
            sm_options = sm_options | OPTIONS_USING_DIGITAL_CAMERAS;
        }

        smOptions.SetState(sm_options);
    }


    // get FS stats of PARENT of root of data disk, meaning the OS disk
    stat((G_CONFIG.DATA_DISK + "/../").c_str(), &G_parent_st);
    G_EM_DATA.SYS_targetDisk = G_CONFIG.OS_DISK;

    G_EM_DATA.sm_recorder = &smRecorder;
    G_EM_DATA.sm_options = &smOptions;
    G_EM_DATA.sm_system = &smSystem;
    G_EM_DATA.sm_aux = &smAuxThread;

    smSystem.UnsetState(SYS_VIDEO_RECORDING); // for completeness
    smRecorder.SetState(STATE_RUNNING);

    if(_AD) {
        iADSensor.SetPort(G_CONFIG.ARDUINO_DEV.c_str());
        iADSensor.SetBaudRate(B9600);
        iADSensor.SetADCType(G_CONFIG.arduino_type.c_str(), G_CONFIG.psi_vmin.c_str());
        iADSensor.Connect();
    } else {
        iADSensor.SetState(0);
    }

    if(_RFID) {
        iRFIDSensor.SetPort(G_CONFIG.RFID_DEV.c_str());
        iRFIDSensor.SetBaudRate(B9600);
        iRFIDSensor.GetScanCounts(G_CONFIG.EM_DIR + "/" + FN_SCAN_COUNT);
        iRFIDSensor.Connect();
    } else {
        iRFIDSensor.SetState(0);
    }

    if(_GPS) {
        iGPSSensor.Connect();
        iGPSSensor.Receive(); // spawn the consumer thread
    } else {
        iGPSSensor.SetState(0);
    }

    // write recorder info to system log header
    time(&rawtime);
    G_timeinfo = localtime(&rawtime);
    snprintf(buf, sizeof(buf), "*** EM-REC VERSION %s, OPTIONS %lu ***", VERSION, smOptions.GetState());
    writeLog(G_EM_DATA.SYS_targetDisk + "/" + FN_SYSTEM_LOG, buf, true /*forceWrite*/);
    
    // spawn auxiliary thread
    if((retVal = pthread_create(&pt_aux, NULL, &thr_auxiliaryLoop, NULL)) != 0) {
        E("Couldn't spawn auxiliary thread");
    } O("Spawned auxiliary thread");
    
    // AFTER THIS POINT
    // there are threads that may write to G_EM_DATA, so we should always use mutexes when accessing
    // this data struct from now on

    signal(SIGINT, exit_handler);
    signal(SIGHUP, exit_handler);
    signal(SIGTERM, exit_handler);
    signal(SIGUSR1, reset_string_scans_handler);
    signal(SIGUSR2, reset_trip_scans_handler);

    O("Beginning main recording loop");
    while(_EM_RUNNING) { // only G_EM_DATA.runState == STATE_NOT_RUNNING can stop this
        gettimeofday(&tv, NULL);
        tstart = tv.tv_sec * 1000000 + tv.tv_usec;

        // get date
        time(&rawtime);
        G_timeinfo = localtime(&rawtime);
        strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", G_timeinfo); // writes null-terminated string so okay not to clear DATE_BUF every time
        pthread_mutex_lock(&G_EM_DATA.mtx);
            snprintf(G_EM_DATA.currentDateTime, sizeof(G_EM_DATA.currentDateTime), "%s.%06d", date, (unsigned int)tv.tv_usec);
            targetDisk = G_EM_DATA.SYS_targetDisk;
        pthread_mutex_unlock(&G_EM_DATA.mtx);

        allStates = smSystem.GetState();

        if(_GPS) {
            iGPSSensor.Receive(); // a separate thread now consumes GPS data, kick it into gear if it's dead
            allStates = allStates | iGPSSensor.GetState();
        }
        
        if(_RFID) {
            iRFIDSensor.Receive();
            allStates = allStates | iRFIDSensor.GetState();
        }
        
        if(_AD) {
            iADSensor.Receive();
            allStates = allStates | iADSensor.GetState();
        }

        if(_GPS) {
            pthread_mutex_lock(&G_EM_DATA.mtx);
                snprintf(buf, sizeof(buf),
                    "%s,%s,%s,%.1f,%lf,%lf,%lf,%lf,%d,%d,%.2lf,%.3lf,%lu",
                    G_EM_DATA.SYS_fishingArea.c_str(),
                    G_CONFIG.vrn.c_str(),
                    G_EM_DATA.currentDateTime,
                    isnan(G_EM_DATA.AD_psi) ? 0 : G_EM_DATA.AD_psi,
                    G_EM_DATA.GPS_latitude,
                    G_EM_DATA.GPS_longitude,
                    G_EM_DATA.GPS_heading,
                    G_EM_DATA.GPS_speed,
                    G_EM_DATA.GPS_satsUsed,
                    G_EM_DATA.GPS_satQuality,
                    G_EM_DATA.GPS_hdop,
                    G_EM_DATA.GPS_eph,
                    allStates);
            pthread_mutex_unlock(&G_EM_DATA.mtx);

            TRACK_lastHash = writeLog(targetDisk + "/" + FN_TRACK_LOG, buf, TRACK_lastHash);
        }

        if(_RFID && G_EM_DATA.RFID_saveFlag) { // RFID_saveFlag is only of concern to this thread (main)
            pthread_mutex_lock(&G_EM_DATA.mtx);
                snprintf(buf, sizeof(buf),
                    "%s,%s,%s,%llu,%.1f,%lu,%lu,%lf,%lf,%lf,%lf,%d,%d,%.2lf,%.3lf,%lu",
                    G_EM_DATA.SYS_fishingArea.c_str(),
                    G_CONFIG.vrn.c_str(),
                    G_EM_DATA.currentDateTime,
                    G_EM_DATA.RFID_lastScannedTag,
                    isnan(G_EM_DATA.AD_psi) ? 0 : G_EM_DATA.AD_psi,
                    G_EM_DATA.RFID_stringScans,
                    G_EM_DATA.RFID_tripScans,
                    G_EM_DATA.GPS_latitude,
                    G_EM_DATA.GPS_longitude,
                    G_EM_DATA.GPS_heading,
                    G_EM_DATA.GPS_speed,
                    G_EM_DATA.GPS_satsUsed,
                    G_EM_DATA.GPS_satQuality,
                    G_EM_DATA.GPS_hdop,
                    G_EM_DATA.GPS_eph,
                    allStates);
            pthread_mutex_unlock(&G_EM_DATA.mtx);

            SCAN_lastHash = writeLog(targetDisk + "/" + FN_SCAN_LOG, buf, SCAN_lastHash);

            // no mutex around these either because only this thread ever twiddles them
            // maybe one day when all sensor reading is done in a separate thread will we need mtx
            G_EM_DATA.RFID_lastSavedTag = G_EM_DATA.RFID_lastScannedTag;
            pthread_mutex_lock(&G_EM_DATA.mtx);
                G_EM_DATA.RFID_lastSaveIteration = G_EM_DATA.runIterations;
            pthread_mutex_unlock(&G_EM_DATA.mtx);
            G_EM_DATA.RFID_saveFlag = false;
        }

        if(_AD && G_EM_DATA.AD_honkSound != 0) { D("Honking horn");
            iADSensor.HonkMyHorn();
        }

        writeJSONState();

        pthread_mutex_lock(&G_EM_DATA.mtx);
            G_EM_DATA.runIterations++;
        pthread_mutex_unlock(&G_EM_DATA.mtx);
        
        // try to make "exactly" 1 second elapse each loop
        gettimeofday(&tv, NULL);
        tdiff = tv.tv_sec * 1000000 + tv.tv_usec - tstart; D("Main loop run time was " + to_string((double)tdiff/1000) + " ms");

        if (tdiff > POLL_PERIOD) {
            tdiff = POLL_PERIOD;
            W("Took longer than 1 second to get data; check sensors");
        }

        usleep(POLL_PERIOD - tdiff);
    }

    // smRecorder is now STATE_NOT_RUNNING

    D("Joining aux thread ... ");
    if(pthread_join(pt_aux, NULL) == 0) {
        smAuxThread.SetState(STATE_NOT_RUNNING);
        O("Auxiliary thread stopped");
    }

    if(_GPS) iGPSSensor.Close();
    if(_RFID) iRFIDSensor.Close();
    if(_AD) iADSensor.Close();

    O("Main thread stopped");

    pthread_mutex_destroy(&G_EM_DATA.mtx);
    pthread_mutex_destroy(&G_mtxOut);
    
    return 0;
}

void *thr_auxiliaryLoop(void *arg) {
    const unsigned int AUX_PERIOD = AUX_INTERVAL * POLL_PERIOD;
    const char *scrot_envp[] = { "DISPLAY=:0", NULL };

    __threadId = THREAD_AUX;
    struct timeval tv;
    unsigned long long tstart = 0, tdiff = 0;
    string buf, targetDisk;
    double latitude, longitude;
    pid_t pid_scrot;
    float last_psi;
    bool gotRecCount = false;
    CaptureManager videoCapture(&G_EM_DATA, FN_VIDEO_DIR);

    mkdir(((string)G_CONFIG.OS_DISK + FN_VIDEO_DIR).c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

    smAuxThread.SetState(STATE_RUNNING);
    D("Auxiliary loop beginning, waiting 4 seconds ...");
    usleep(4 * POLL_PERIOD); // let things settle down, let the GPS info get collected, etc.

    while(_EM_RUNNING) { // only G_EM_DATA.runState == STATE_NOT_RUNNING can stop this
        gettimeofday(&tv, NULL);
        tstart = tv.tv_sec * 1000000 + tv.tv_usec;

        buf = updateSystemStats(); // by now we know state of disks

        pthread_mutex_lock(&G_EM_DATA.mtx);
            targetDisk = G_EM_DATA.SYS_targetDisk;
        pthread_mutex_unlock(&G_EM_DATA.mtx);
        
        writeLog(targetDisk + "/" + FN_SYSTEM_LOG, buf);
        
        // process position, set special area states, and if we're in the home port ...
        if(iGPSSensor.InSpecialArea() && iGPSSensor.GetState() & GPS_IN_HOME_PORT) { // pause
            if(videoCapture.GetState() & STATE_RUNNING) {
                pthread_mutex_lock(&G_EM_DATA.mtx);
                    latitude = G_EM_DATA.GPS_latitude;
                    longitude = G_EM_DATA.GPS_longitude;
                pthread_mutex_unlock(&G_EM_DATA.mtx);
                I("Entered home port (" + to_string(latitude) + "," + to_string(longitude) + "), pausing video capture");
            }

            videoCapture.Stop();
        } else {
            if(videoCapture.GetState() & STATE_NOT_RUNNING) { // resume
                pthread_mutex_lock(&G_EM_DATA.mtx);
                    latitude = G_EM_DATA.GPS_latitude;
                    longitude = G_EM_DATA.GPS_longitude;
                pthread_mutex_unlock(&G_EM_DATA.mtx);
                I("Left home port or was never there (" + to_string(latitude) + "," + to_string(longitude) + "), starting video capture");
            }

            // fishing activity check
            pthread_mutex_lock(&G_EM_DATA.mtx);
                last_psi = G_EM_DATA.AD_psi;
            pthread_mutex_unlock(&G_EM_DATA.mtx);
            
            D(intToString(G_CONFIG.psi_low_threshold) + " < " + intToString(last_psi) + " < " + intToString(G_CONFIG.psi_high_threshold) )
            
            // should check if we are even using the AD
            if(last_psi >= G_CONFIG.psi_high_threshold ||
                iADSensor.GetState() & AD_NO_CONNECTION ||
                iADSensor.GetState() & AD_NO_DATA ||
                iADSensor.GetState() & AD_PSI_LOW_OR_ZERO
                  ) {
                smSystem.UnsetState(SYS_REDUCED_VIDEO_BITRATE);
                D("Unset SYS_REDUCED_VIDEO_BITRATE");
            } else if(last_psi < G_CONFIG.psi_low_threshold) {
                // want to see this state set repeatedly over a period of
                // fps_low_delay minutes before we actually get a reduction
                smSystem.SetState(SYS_REDUCED_VIDEO_BITRATE, (unsigned short)(G_CONFIG.fps_low_delay * 60 * 1000000 / AUX_INTERVAL / POLL_PERIOD));
                D("Set SYS_REDUCED_VIDEO_BITRATE");
            }

            if(smSystem.GetState() & SYS_REDUCED_VIDEO_BITRATE) {
                D("Activated SYS_REDUCED_VIDEO_BITRATE");
            }

            videoCapture.Start(); // what this app is really all about =)
        }

        // checks for errors that are screenshot-worthy
        if((iADSensor.GetState() | iRFIDSensor.GetState() | iGPSSensor.GetState() | smSystem.GetState()) & ~(G_IGNORED_STATES)) {
            // state needs to be around SCREENSHOT_STATE_DELAY seconds before really showing up
            smTakeScreenshot.SetState(true, (unsigned short)(SCREENSHOT_STATE_DELAY * 1000000 / AUX_INTERVAL / POLL_PERIOD));
        } else {
            // a single interruption clears the state
            smTakeScreenshot.UnsetAllStates();
        }
        
        if(smTakeScreenshot.GetState() == true) {
            if(smSystem.GetState() & SYS_TARGET_DISK_WRITABLE) {
                pid_scrot = fork();
                
                if(pid_scrot == 0) {
                    execle("/usr/bin/scrot", "scrot", "-q5", (targetDisk + "/screenshots/%Y%m%d-%H%M%S.jpg").c_str(), NULL, scrot_envp);
                } else if(pid_scrot > 0) {
                    waitpid(pid_scrot, NULL, 0);
                    smTakeScreenshot.UnsetAllStates();
                    D("Took screenshot");
                } else {
                    E("/usr/bin/scrot fork() failed");
                }
            } else {
                E("No writable disks; not taking screenshot");
            }    
        }

        if(!gotRecCount) {
            videoCapture.GetRecCount();
            gotRecCount = true;
        }
        // No more disk space consumed after this point
        ///////////////////////////////////////////////

        // try to make "exactly" 1 second elapse each loop
        gettimeofday(&tv, NULL);
        tdiff = tv.tv_sec * 1000000 + tv.tv_usec - tstart; D("Auxiliary thread loop run time was " + to_string((double)tdiff/1000) + " ms");

        if (tdiff > AUX_PERIOD) {
            tdiff = AUX_PERIOD;
            W("Auxiliary thread loop took longer than " + to_string(AUX_INTERVAL) + " POLL_PERIODs");
        }

        // usleep until next run, but chunk it out so we can catch an EM shutdown quickly (b/c Tom is impatient)
        for(int x = 0; x < 4; x++) {
            if (_EM_RUNNING) {
                usleep((AUX_PERIOD - tdiff) / 4);
            }
        }
    } D("Broke out of auxiliary thread loop; stopping video capture ...");
    
    videoCapture.Stop();
    smAuxThread.SetState(STATE_CLOSING);

    pthread_exit(NULL);
}

void writeLog(string prefix, string buf) {
    writeLog(prefix, buf, false);
}

void writeLog(string prefix, string buf, bool forceWrite) {
    // would be nice to buffer things until it becomes writable
    if(smSystem.GetState() & SYS_TARGET_DISK_WRITABLE || forceWrite) {
        FILE *fp;
        char date_suffix[9];
        strftime(date_suffix, sizeof(date_suffix), "%Y%m%d", G_timeinfo);
        string file = prefix + "_" + date_suffix + ".csv";

        buf += "\n";

        if(!(fp = fopen(file.c_str(), "a"))) {
            E("Can't write to data file " + file);
        } else {
            if(fwrite(buf.c_str(), buf.length(), 1, fp) < 1 && errno == ENOSPC) {
                D("fwrite() error");
            } 
            fclose(fp);
        }
    }
}

string writeLog(string prefix, string buf, string lastHash) {
    string lastHashFile = prefix + ".lh";
    string newHash;

    if(lastHash.empty()) {
        I("Seeding last hash from " + lastHashFile);

        ifstream fin(lastHashFile.c_str());
        if(!fin.fail()) {
            fin >> lastHash;
            fin.close();
        }
    }

    if(smSystem.GetState() & SYS_TARGET_DISK_WRITABLE) {
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

void writeJSONState() {
    char buf[1024] = { '\0' };

    pthread_mutex_lock(&G_EM_DATA.mtx);
        snprintf(buf, sizeof(buf),
        "{ "
            "\"recorderVersion\": \"%s\", "
            "\"currentDateTime\": \"%s\", "
            "\"runIterations\": %lu, "
            "\"SYS\": { "
                "\"state\": %lu, "
                "\"fishingArea\": \"%s\", "
                "\"numCams\": %hu, "
                "\"targetDisk\": \"%s\", "
                "\"uptime\": \"%s\", "
                "\"load\": \"%s\", "
                "\"cpuPercent\": %.1f, "
                "\"ramFreeKB\": %llu, "
                "\"ramTotalKB\": %llu, "
                "\"tempCore0\": %u, "
                "\"tempCore1\": %u, "
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
            "}, "
            "\"OPTIONS\": { "
                "\"state\": %lu "
            "} "
        "}",
            VERSION,
            G_EM_DATA.currentDateTime,
            G_EM_DATA.runIterations,

            smSystem.GetState(),
            G_EM_DATA.SYS_fishingArea.c_str(),
            G_EM_DATA.SYS_numCams,
            G_EM_DATA.SYS_targetDisk.c_str(),
            G_EM_DATA.SYS_uptime.c_str(),
            G_EM_DATA.SYS_load.c_str(),
            std::isnan(G_EM_DATA.SYS_cpuPercent) ? 0 : G_EM_DATA.SYS_cpuPercent,
            G_EM_DATA.SYS_ramFreeKB,
            G_EM_DATA.SYS_ramTotalKB,
            G_EM_DATA.SYS_tempCore0,
            G_EM_DATA.SYS_tempCore1,
            G_EM_DATA.SYS_osDiskFreeBlocks,
            G_EM_DATA.SYS_osDiskTotalBlocks,
            G_EM_DATA.SYS_osDiskMinutesLeft,
            G_EM_DATA.SYS_dataDiskLabel.c_str(),
            G_EM_DATA.SYS_dataDiskFreeBlocks,
            G_EM_DATA.SYS_dataDiskTotalBlocks,
            G_EM_DATA.SYS_dataDiskMinutesLeft,
            G_EM_DATA.SYS_dataDiskMinutesLeftFake,

            iGPSSensor.GetState(),
            G_EM_DATA.GPS_latitude,
            G_EM_DATA.GPS_longitude,
            G_EM_DATA.GPS_heading,
            G_EM_DATA.GPS_speed,
            G_EM_DATA.GPS_satQuality,
            G_EM_DATA.GPS_satsUsed,
            G_EM_DATA.GPS_hdop,
            G_EM_DATA.GPS_eph,

            iRFIDSensor.GetState(),
            G_EM_DATA.RFID_lastScannedTag,
            G_EM_DATA.RFID_stringScans,
            G_EM_DATA.RFID_tripScans,

            iADSensor.GetState(),
            std::isnan(G_EM_DATA.AD_psi) ? 0 : G_EM_DATA.AD_psi,
            std::isnan(G_EM_DATA.AD_battery) ? 0 : G_EM_DATA.AD_battery,

            smOptions.GetState()
        );
    pthread_mutex_unlock(&G_EM_DATA.mtx);

    ofstream fout(G_CONFIG.JSON_STATE_FILE.c_str());
    if (!fout.fail()) {
        fout << buf << endl;
        fout.close();
    }
}

string updateSystemStats() {
    static unsigned long long lastJiffies[PROC_STAT_VALS];
    static unsigned long osDiskLastFree, osDiskDiff, dataDiskLastFree, dataDiskDiff;
    static unsigned long osDiskMinutesLeft[DISK_USAGE_SAMPLES], dataDiskMinutesLeft[DISK_USAGE_SAMPLES];
    static unsigned long osDiskMinutesLeft_total = DISK_USAGE_SAMPLES * DISK_USAGE_START_VAL, dataDiskMinutesLeft_total = DISK_USAGE_SAMPLES * DISK_USAGE_START_VAL;
    static signed short osDiskMinutesLeft_index = -1, dataDiskMinutesLeft_index = -1;

    struct stat st;
    struct blkid_struct_cache *blkid_cache;
    struct sysinfo si;
    int days, hours, mins;
    FILE *ps, *ct;
    unsigned long long jiffies[PROC_STAT_VALS] = {0}, totalJiffies = 0, temp;
    struct statvfs vfs;
    char buf[256];
    string targetDisk;

    while(true) { // responsible for data disk presence check + mount
        // check /mnt/data's device ID and compare it to its parent /mnt
        if(stat(G_CONFIG.DATA_DISK.c_str(), &st) == 0 && st.st_dev != G_parent_st.st_dev) { // present
            // this is a new state, there wasn't a disk before
            // retrieve the label; give output; set diskPresent in global G_EM_DATA
            if (!(smSystem.GetState() & SYS_DATA_DISK_PRESENT)) {
                blkid_get_cache(&blkid_cache, NULL);

                smSystem.SetState(SYS_DATA_DISK_PRESENT);
                pthread_mutex_lock(&G_EM_DATA.mtx);
                    G_EM_DATA.SYS_dataDiskLabel = blkid_get_tag_value(blkid_cache, "LABEL", blkid_devno_to_devname(st.st_dev));
                pthread_mutex_unlock(&G_EM_DATA.mtx);
                
                mkdir(((string)G_CONFIG.DATA_DISK + FN_VIDEO_DIR).c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

                I("Data disk PRESENT, label = " + G_EM_DATA.SYS_dataDiskLabel);
            }

            break; // do nothing else
        }

        // disk not present
        O("Attempting to mount data disk ...");
        if(mount("/dev/em_data", G_CONFIG.DATA_DISK.c_str(), "ext4",
            MS_NOATIME | MS_NODEV | MS_NODIRATIME | MS_NOEXEC | MS_NOSUID, NULL) == 0) {
            I("Mount success");
            continue; // we want to rerun the while() so we don't break
        }

        // strictly speaking if the mount() returns 0 but the device ID/parent check at
        // the top of the loop fails then we'll go into an infinite loop here; I don't know
        // of any OS conditions that would cause this, but just sayin' ...
        // (actually I guess if you mounted the same device under a secondary mount point it might cause this)

        E("Mount failed");
        if(smSystem.GetState() & SYS_DATA_DISK_PRESENT) {
            smSystem.UnsetState(SYS_DATA_DISK_PRESENT);
            blkid_gc_cache(blkid_cache);
            I("Data disk NOT PRESENT");
        }

        break; // all data disk processing complete
    } // end while

    pthread_mutex_lock(&G_EM_DATA.mtx);
        // populate averaging array with reasonable start values, so UI isn't crazy
        if (osDiskMinutesLeft_index == -1) {
            for(int i = 0; i < DISK_USAGE_SAMPLES; i++) osDiskMinutesLeft[i] = DISK_USAGE_START_VAL;
            G_EM_DATA.SYS_osDiskMinutesLeft = osDiskMinutesLeft_total / DISK_USAGE_SAMPLES;
            osDiskMinutesLeft_index = 0;
        }

        if (dataDiskMinutesLeft_index == -1) {
            for(int i = 0; i < DISK_USAGE_SAMPLES; i++) dataDiskMinutesLeft[i] = DISK_USAGE_START_VAL;
            G_EM_DATA.SYS_dataDiskMinutesLeft = dataDiskMinutesLeft_total / DISK_USAGE_SAMPLES;
            dataDiskMinutesLeft_index = 0;
        }

        if(sysinfo(&si) == 0) {
            // SYS_uptime
            days = si.uptime / 86400;
            hours = (si.uptime / 3600) - (days * 24);
            mins = (si.uptime / 60) - (days * 1440) - (hours * 60);
            snprintf(buf, sizeof(buf), "%dd%dh%dm", days, hours, mins);
            G_EM_DATA.SYS_uptime = buf;
            
            // SYS_ramFree/Total
            G_EM_DATA.SYS_ramFreeKB = si.freeram * (unsigned long long)si.mem_unit / 1024;
            G_EM_DATA.SYS_ramTotalKB = si.totalram * (unsigned long long)si.mem_unit / 1024;
            
            // SYS_load
            snprintf(buf, sizeof(buf), "%0.02f %0.02f %0.02f", (float)si.loads[0] / (float)(1<<SI_LOAD_SHIFT), (float)si.loads[1] / (float)(1<<SI_LOAD_SHIFT), (float)si.loads[2] / (float)(1<<SI_LOAD_SHIFT));
            G_EM_DATA.SYS_load = buf;
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

        G_EM_DATA.SYS_cpuPercent = totalJiffies;

        if(jiffies[3] > lastJiffies[3]) {
            totalJiffies += (jiffies[3] - lastJiffies[3]);
            G_EM_DATA.SYS_cpuPercent = G_EM_DATA.SYS_cpuPercent / totalJiffies * 100;
        } else {
            G_EM_DATA.SYS_cpuPercent = -1.0;
        }

        for(int i = 0; i < PROC_STAT_VALS; i++) { lastJiffies[i] = jiffies[i]; }
        
        // SYS_tempCore0
        ct = fopen("/sys/devices/platform/coretemp.0/temp2_input", "r");
        fscanf(ct, "%llu", &temp);
        fclose(ct);
        G_EM_DATA.SYS_tempCore0 = temp / 1000;

        // SYS_tempCore1
        ct = fopen("/sys/devices/platform/coretemp.0/temp3_input", "r");
        fscanf(ct, "%llu", &temp);
        fclose(ct);
        G_EM_DATA.SYS_tempCore1 = temp / 1000;
    pthread_mutex_unlock(&G_EM_DATA.mtx);

    //SYS_*DiskTotalBlocks/FreeBlocks
    if(statvfs(G_CONFIG.OS_DISK.c_str(), &vfs) == 0) {
        if(osDiskLastFree >= vfs.f_bavail) {
            osDiskMinutesLeft_total -= osDiskMinutesLeft[osDiskMinutesLeft_index];
            osDiskDiff = osDiskLastFree - vfs.f_bavail;

            if(osDiskDiff <= 0) {
                osDiskMinutesLeft[osDiskMinutesLeft_index] = osDiskMinutesLeft[osDiskMinutesLeft_index - 1];
            } else {
                osDiskMinutesLeft[osDiskMinutesLeft_index] = (int)((double)vfs.f_bavail / (double)(60000000 / POLL_PERIOD / AUX_INTERVAL * osDiskDiff));
            }

            osDiskMinutesLeft_total += osDiskMinutesLeft[osDiskMinutesLeft_index];
            
            osDiskMinutesLeft_index++;
            if(osDiskMinutesLeft_index >= DISK_USAGE_SAMPLES) osDiskMinutesLeft_index = 0;

            pthread_mutex_lock(&G_EM_DATA.mtx);
                G_EM_DATA.SYS_osDiskMinutesLeft = osDiskMinutesLeft_total / DISK_USAGE_SAMPLES;
            pthread_mutex_unlock(&G_EM_DATA.mtx);
        }

        pthread_mutex_lock(&G_EM_DATA.mtx);
            G_EM_DATA.SYS_osDiskTotalBlocks = vfs.f_blocks;
            G_EM_DATA.SYS_osDiskFreeBlocks = osDiskLastFree = vfs.f_bavail;
        pthread_mutex_unlock(&G_EM_DATA.mtx);

        // if less than 16 MB left (assuming 4K block size)
        if(vfs.f_bavail < 4096/* && osDiskMinutesLeft_index > 1*/) {
            smSystem.SetState(SYS_OS_DISK_FULL);
            D("Set OS_DISK_FULL");

            pthread_mutex_lock(&G_EM_DATA.mtx);
                G_EM_DATA.SYS_osDiskMinutesLeft = 0;
            pthread_mutex_unlock(&G_EM_DATA.mtx);
        } else {
            smSystem.UnsetState(SYS_OS_DISK_FULL);
        }
    }

    if(smSystem.GetState() & SYS_DATA_DISK_PRESENT) {
        if(statvfs(G_CONFIG.DATA_DISK.c_str(), &vfs) == 0) {
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
                    dataDiskMinutesLeft[dataDiskMinutesLeft_index] = (int)((double)vfs.f_bavail / (double)(60000000 / POLL_PERIOD / AUX_INTERVAL * dataDiskDiff));
                }

                dataDiskMinutesLeft_total += dataDiskMinutesLeft[dataDiskMinutesLeft_index];
                
                dataDiskMinutesLeft_index++;
                if(dataDiskMinutesLeft_index >= DISK_USAGE_SAMPLES) {
                    dataDiskMinutesLeft_index = 0;
                }

                pthread_mutex_lock(&G_EM_DATA.mtx);
                    // the real calculation
                    G_EM_DATA.SYS_dataDiskMinutesLeft = dataDiskMinutesLeft_total / DISK_USAGE_SAMPLES;
                pthread_mutex_unlock(&G_EM_DATA.mtx);
            }

            pthread_mutex_lock(&G_EM_DATA.mtx);
                G_EM_DATA.SYS_dataDiskTotalBlocks = vfs.f_blocks;
                G_EM_DATA.SYS_dataDiskFreeBlocks = dataDiskLastFree = vfs.f_bavail;
                G_EM_DATA.SYS_dataDiskMinutesLeftFake = (DATA_DISK_FAKE_DAYS_START * 24 * 60) - (G_EM_DATA.SYS_videoSecondsRecorded / 60);
            pthread_mutex_unlock(&G_EM_DATA.mtx);

            if(vfs.f_bavail < 4096/* && dataDiskMinutesLeft_index > 1*/) {
                smSystem.SetState(SYS_DATA_DISK_FULL);
                D("Set DATA_DISK_FULL");

                pthread_mutex_lock(&G_EM_DATA.mtx);
                    G_EM_DATA.SYS_dataDiskMinutesLeft = 0;
                pthread_mutex_unlock(&G_EM_DATA.mtx);
            } else {
                smSystem.UnsetState(SYS_DATA_DISK_FULL);
            }
        }
    } else { // no data disk present
        pthread_mutex_lock(&G_EM_DATA.mtx);
            G_EM_DATA.SYS_dataDiskFreeBlocks = 0;
            G_EM_DATA.SYS_dataDiskTotalBlocks = 0;
            G_EM_DATA.SYS_dataDiskMinutesLeft = 0;
            G_EM_DATA.SYS_dataDiskMinutesLeftFake = 0;
            G_EM_DATA.SYS_dataDiskLabel.clear();
        pthread_mutex_unlock(&G_EM_DATA.mtx);
    }

    // if OS disk is full
    if(smSystem.GetState() & SYS_OS_DISK_FULL) {
        // if data disk is present and not full
        if(smSystem.GetState() & SYS_DATA_DISK_PRESENT && !(smSystem.GetState() & SYS_DATA_DISK_FULL)) {
            targetDisk = G_CONFIG.DATA_DISK;
            smSystem.SetState(SYS_TARGET_DISK_WRITABLE);
            D("targetDisk = DATA, writable");
        } else {
            smSystem.UnsetState(SYS_TARGET_DISK_WRITABLE);
            D("No writable disks!");
        }
    } else {
        targetDisk = G_CONFIG.OS_DISK;
        smSystem.SetState(SYS_TARGET_DISK_WRITABLE);
        D("targetDisk = OS, writable");
    }

    pthread_mutex_lock(&G_EM_DATA.mtx);
        G_EM_DATA.SYS_targetDisk = targetDisk;
    pthread_mutex_unlock(&G_EM_DATA.mtx);

    // SYSTEM_DATA.csv
    snprintf(buf, sizeof(buf),
        "%s,%s,%s,%lu,%0.02f,%0.02f,%0.02f,%.1f,%u,%u,%llu,%llu,%lu,%lu,%u",
        G_EM_DATA.SYS_fishingArea.c_str(),
        G_CONFIG.vrn.c_str(),
        G_EM_DATA.currentDateTime,
        si.uptime,
        (float)si.loads[0] / (float)(1<<SI_LOAD_SHIFT),
        (float)si.loads[1] / (float)(1<<SI_LOAD_SHIFT),
        (float)si.loads[2] / (float)(1<<SI_LOAD_SHIFT),
        std::isnan(G_EM_DATA.SYS_cpuPercent) ? 0 : G_EM_DATA.SYS_cpuPercent,
        G_EM_DATA.SYS_tempCore0,
        G_EM_DATA.SYS_tempCore1,
        G_EM_DATA.SYS_ramFreeKB,
        si.freeswap * (unsigned long long)si.mem_unit / 1024,
        G_EM_DATA.SYS_osDiskFreeBlocks * 4096 / 1024 / 1024,
        G_EM_DATA.SYS_dataDiskFreeBlocks * 4096 / 1024 / 1024,
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
    I("Ecotrust EM Recorder v" + VERSION + " is stopping ...");
    smRecorder.SetState(STATE_NOT_RUNNING);
}
