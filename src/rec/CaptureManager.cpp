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

#include "CaptureManager.h"
#include "output.h"
#include "config.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

extern CONFIG_TYPE G_CONFIG;

UsageEnvironment  *G_env;
TaskScheduler     *G_scheduler;
char              *G_captureLoopWatchPtr;
MultiRTSPClient   *G_rtspClients[DIGITAL_MAX_CAMS];
unsigned           G_rtspClientCount;

CaptureManager::CaptureManager(EM_DATA_TYPE* _em_data, string _videoDirectory):StateMachine(STATE_NOT_RUNNING, SM_EXCLUSIVE_STATES) {
    em_data = _em_data;
    videoDirectory = _videoDirectory;
    moduleName = "CAP";
    wasInReducedBitrateMode = false;

    if(__ANALOG) {
        I("Using *" + to_string(em_data->SYS_numCams) + "* ANALOG cameras");

        snprintf(encoderArgs, sizeof(encoderArgs),
            FFMPEG_ARGS_LINE,
            ANALOG_INPUT_RESOLUTION,
            ANALOG_INPUT_FPS,
            ANALOG_INPUT_DEVICE,
            ANALOG_OUTPUT_FPS_NORMAL,
            ANALOG_H264_OPTS,
            ANALOG_MAX_RATE,
            ANALOG_BUF_SIZE);

        snprintf(encoderArgsSlow, sizeof(encoderArgs),
            FFMPEG_ARGS_LINE,
            ANALOG_INPUT_RESOLUTION,
            ANALOG_INPUT_FPS,
            ANALOG_INPUT_DEVICE,
            ANALOG_OUTPUT_FPS_SLOW,
            ANALOG_H264_OPTS,
            ANALOG_MAX_RATE,
            ANALOG_BUF_SIZE);
    } else if(__IP) {
        I("Using *" + std::to_string(em_data->SYS_numCams) + "* IP cameras");

        G_scheduler = BasicTaskScheduler::createNew();
        G_env = BasicUsageEnvironment::createNew(*G_scheduler);
        captureLoopWatchVar = LOOP_WATCH_VAR_NOT_RUNNING;
        G_captureLoopWatchPtr = &captureLoopWatchVar;
        G_rtspClientCount = 0;
    }
}

CaptureManager::~CaptureManager() {
    if(__IP) {
        G_env->reclaim();
        G_env = NULL;
        delete G_scheduler;
        G_scheduler = NULL;
    }
}

unsigned long CaptureManager::Start() {
    D("CaptureManager::Start()");
    unsigned long initialState = GetState();

    // if we're already supposed to be running and we're recording in slow mode
    // but normal mode is requested, we do this right away
    if(GetState() & STATE_RUNNING) {
        if(wasInReducedBitrateMode && !(__SYS_GET_STATE & SYS_REDUCED_VIDEO_BITRATE)) {
            I("Switching to HIGHER bitrate recording immediately");

            if(__ANALOG) {
                Stop();
            }
            else if(__IP) {
                for(_ACTIVE_CAMS) {
                    StreamClientState& scs = ((MultiRTSPClient*)G_rtspClients[i])->scs; // alias
                    G_env->taskScheduler().rescheduleDelayedTask(scs.periodicFileOutputTask, 0, (TaskFunc*)periodicFileOutputTimerHandler, G_rtspClients[i]);
                }
            }

            wasInReducedBitrateMode = false;
        } else if(!wasInReducedBitrateMode && __SYS_GET_STATE & SYS_REDUCED_VIDEO_BITRATE) {
            I("Switching to LOWER bitrate recording for next video");
            wasInReducedBitrateMode = true;
        }

        pthread_mutex_lock(&(em_data->mtx));
            em_data->SYS_videoSecondsRecorded = em_data->SYS_videoSecondsRecorded + (POLL_PERIOD * AUX_INTERVAL / 1000000);
        pthread_mutex_unlock(&(em_data->mtx));

        WriteRecCount();
    }

    // Now actually start things up
    if(__ANALOG) {
        D("In ANALOG mode");
        #define HARDCODED_CAM_INDEX 0 // because we only support one analog camera at the current time

        int wait_result;
        bool delayedStart = false;

        // this is not a first run; there exists a previously executed ffmpeg in some state
        if(pid_encoder[HARDCODED_CAM_INDEX] != 0) {
            // we're going to attempt to determine its state

            // waitpid w/ WNOHANG returns 0 if pid exists but has not yet changed state (meaning it's still running)
            //if(waitpid(pid_encoder[HARDCODED_CAM_INDEX], &pid_status, WNOHANG) == 0) {
            wait_result = waitpid(pid_encoder[HARDCODED_CAM_INDEX], NULL, WNOHANG);

            if(wait_result == 0) {
                D("Current ffmpeg still running");

                if(GetSecondsUntilNextClipTime() > AUX_INTERVAL) {
                    ReapZombieChildrenNonBlocking();
                    // there's nothing to do
                    return initialState;
                }

                D("Next clip occurs within next aux loop wait period; spawning a delayed ffmpeg to be ready");
                // we're gonna spawn a new child anyway
                // note that this child sleeps until the exact second that ffmpeg needs to begin operations
                pidReaperQueue.push(pid_encoder[HARDCODED_CAM_INDEX]);
                delayedStart = true;
            } else if(wait_result > 0) { // ffmpeg already exited (possibly in error)
                D("Current ffmpeg found exited and was reaped");
                __SYS_UNSET_STATE(SYS_VIDEO_RECORDING);
                SetState(STATE_NOT_RUNNING);
            } else {
                D("waitpid() error occurred; maybe ffmpeg was already reaped?");
            }
        }

        if(__EM_RUNNING && __SYS_GET_STATE & SYS_TARGET_DISK_WRITABLE) {
            pid_encoder[HARDCODED_CAM_INDEX] = fork();
        
            // CHILD
            if(pid_encoder[HARDCODED_CAM_INDEX] == 0) {
                time_t rawtime;
                struct tm *timeinfo;
                char date[32];
                char fileName[128];
                char *argv[FFMPEG_MAX_ARGS];
                char progName[8] = "ffmpeg";
                argv[0] = progName;
                unsigned short secsUntilNextClip;
                string token;
                int argIndex = 1;
                stringstream ssin;
                
                if(__SYS_GET_STATE & SYS_REDUCED_VIDEO_BITRATE) {
                    ssin << encoderArgsSlow;
                } else {
                    ssin << encoderArgs;
                }
                
                while(ssin >> token) {
                    //cout << "Found token: '" << token << "' size: " << token.size() << endl;
                    argv[argIndex] = new char[token.size() + 1];
                    token.copy(argv[argIndex], token.size());
                    argv[argIndex][token.size()] = '\0';
                    //cout << "a argv[" << argIndex << "] = " << argv[argIndex] << endl;
                    argIndex++;
                }

                secsUntilNextClip = GetSecondsUntilNextClipTime();

                if(delayedStart) {
                    D("In fork child, waiting " + to_string(secsUntilNextClip) + " seconds before execv() ...");
                    usleep(secsUntilNextClip * 1000000); // possibly more granularity needed
                    secsUntilNextClip = MAX_CLIP_LENGTH;
                }

                // determine filename based on current time and add it as an arg
                time(&rawtime);
                timeinfo = localtime(&rawtime);
                strftime(date, sizeof(date), "%Y%m%d-%H%M%S", timeinfo);
                snprintf(fileName, sizeof(fileName), "%s/%s-cam%d.mp4", (em_data->SYS_targetDisk + videoDirectory).c_str(), date, HARDCODED_CAM_INDEX + 1);
                argv[argIndex++] = const_cast<char*>(to_string(secsUntilNextClip).c_str());
                argv[argIndex++] = fileName;
                argv[argIndex++] = (char*)NULL;

                D("execv() " + FFMPEG_BINARY + " " + encoderArgs + to_string(secsUntilNextClip) + " " + fileName);
                execv(FFMPEG_BINARY, argv);
                E("execv() failed: " + strerror(errno));
                exit(-1);

            // PARENT
            } else if(pid_encoder[HARDCODED_CAM_INDEX] > 0) {
                __SYS_SET_STATE(SYS_VIDEO_RECORDING);
                SetState(STATE_RUNNING);

            // NOTHING HAPPENED
            } else {
                E(FFMPEG_BINARY + " fork() failed");
            }
        }

        ReapZombieChildrenNonBlocking();
    }

    else if(__IP) {
        D("In IP mode");

        JoinIPCaptureThreadNonBlocking(); // this will turn threads in STATE_CLOSING_OR_UNDEFINED into STATE_NOT_RUNNING

        if(GetState() & STATE_NOT_RUNNING) {
            if(pthread_create(&pt_capture, NULL, &thr_IPCaptureLoopLauncher, (void *)this) != 0) {
                E("Couldn't create IP camera capture thread");
            } else {
                D("Created IP camera capture thread");
            }
        } /*else if(GetState() & STATE_RUNNING) {
            // check that G_rtspClientCount == number of cams
            if (G_rtspClientCount != em_data->SYS_numCams) {

            }
        }*/
    }
    
    return initialState;
}

unsigned long CaptureManager::Stop() {
    unsigned long initialState = GetState();

    WriteRecCount();

    if(__ANALOG) {
        D("CaptureManager::Stop() ANALOG");
        if(pid_encoder[HARDCODED_CAM_INDEX] != 0) {
            kill(pid_encoder[HARDCODED_CAM_INDEX], SIGTERM);
            pidReaperQueue.push(pid_encoder[HARDCODED_CAM_INDEX]);
            ReapZombieChildrenBlocking();
        }
        SetState(STATE_NOT_RUNNING);
    } else if(__IP) {
        D("CaptureManager::Stop() IP");

        if(GetState() & STATE_STARTING || GetState() & STATE_RUNNING) {
            for(_ACTIVE_CAMS) {
                shutdownStream(G_rtspClients[i], LIVERTSP_EXIT_CLEAN);
            }
        }
        
        JoinIPCaptureThreadBlocking(); // this one sets STATE_NOT_RUNNING
    }

    return initialState;
}

unsigned short CaptureManager::GetSecondsUntilNextClipTime() {
    return (MAX_CLIP_LENGTH - (time(0) % MAX_CLIP_LENGTH));
}

void CaptureManager::ReapZombieChildrenBlocking() {
    ReapZombieChildren(false);
}

void CaptureManager::ReapZombieChildrenNonBlocking() {
    ReapZombieChildren(true);
}

void CaptureManager::ReapZombieChildren(bool dontBlock) {
    int options = 0;
    if(dontBlock) options = WNOHANG;

    unsigned int waitCount = 0;

    while(!pidReaperQueue.empty()) {
        pid_t pid = pidReaperQueue.front();

        D("Waiting on PID " + to_string(pid));
        if(waitpid(pid, NULL, options) > 0) {
            D("PID " + to_string(pid) + " reaped");
            pidReaperQueue.pop();
        } else {
            waitCount++;

            if(waitCount >= 20) { // 2 seconds
                //D("Waited long enough; sending SIGTERM to PID " + to_string(pid));
                //kill(pid, SIGTERM);
                waitCount = 0;
                break; // we'll wait until next round to reap this guy to give him a chance to TERM properly
            }
        }

        usleep(100000);
    }
}

void CaptureManager::JoinIPCaptureThreadBlocking() {
    if(GetState() & STATE_NOT_RUNNING) return;

    do {
        D("Waiting to join with capture thread ...");
        usleep(POLL_PERIOD / 10);
    } while(GetState() & STATE_RUNNING || GetState() & STATE_STARTING);

    JoinIPCaptureThreadNonBlocking();
}

void CaptureManager::JoinIPCaptureThreadNonBlocking() {
    usleep(POLL_PERIOD / 10);

    if(GetState() & STATE_CLOSING) {
        D("Found capture thread in STATE_CLOSING, joining ...");
        if(pthread_join(pt_capture, NULL) == 0) {
            SetState(STATE_NOT_RUNNING);
            D("pthread_join() success");
        }
    }
}

void *CaptureManager::thr_IPCaptureLoopLauncher(void *self) {
    extern __thread unsigned short __threadId;
    __threadId = THREAD_CAPTURE;

    ((CaptureManager*)self)->thr_IPCaptureLoop();
    return NULL;
}

void CaptureManager::thr_IPCaptureLoop() {
    bool madeProgress = false;
    char url[64];
    //unsigned short frameRate;
    
    if(__EM_RUNNING && __SYS_GET_STATE & SYS_TARGET_DISK_WRITABLE) {
        SetState(STATE_STARTING);
        
        /*if(nextRecordMode == RECORDING_NORMAL) {
            frameRate = DIGITAL_OUTPUT_FPS_NORMAL;
        } else {
            frameRate = DIGITAL_OUTPUT_FPS_SLOW;
        }

        //if(nextRecordMode != lastRecordMode) {
        //    SetIPOutputFrameRate(frameRate);
        //}*/

        D("Beginning Live555 init ...");
        for(_ACTIVE_CAMS) { // i = cam index
            snprintf(url, sizeof(url), DIGITAL_RTSP_URL, i + 1); // PRODUCTION
            //snprintf(url, sizeof(url), DIGITAL_RTSP_URL, 1); // HACK to force multi-cam testing with just one cam

            // open and start streaming each URL
            /*MultiRTSPClient**/ G_rtspClients[i] = new MultiRTSPClient(*G_env,
                                                    url,
                                                    LIVERTSP_CLIENT_VERBOSITY_LEVEL,
                                                    "em-rec",
                                                    videoDirectory,
                                                    i,
                                                    MAX_CLIP_LENGTH,
                                                    em_data);
            if (G_rtspClients[i] == NULL) {
                E("Failed to create a RTSP client for URL '" + url + "': " + G_env->getResultMsg());
                continue;
            } else {
                D("Created a RTSP client for URL '" + url + "'");
            }

            G_rtspClientCount++;
            madeProgress = true;

            // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
            // Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
            // Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
            G_rtspClients[i]->sendDescribeCommand(continueAfterDESCRIBE);
        }
        D("Finished Live555 init");
    }
    
    if(madeProgress && GetState() & STATE_STARTING && __EM_RUNNING && __SYS_GET_STATE & SYS_TARGET_DISK_WRITABLE) {
        D("Beginning Live555 doEventLoop() ...");

        captureLoopWatchVar = LOOP_WATCH_VAR_RUNNING;
        SetState(STATE_RUNNING);
        //__SYS_SET_STATE(SYS_VIDEO_RECORDING);
        
        // does not return unless captureLoopWatchVar set to non-zero
        G_env->taskScheduler().doEventLoop(&captureLoopWatchVar);
        D("Broke out of Live555 doEventLoop()");

        __SYS_UNSET_STATE(SYS_VIDEO_RECORDING);
    }

    // Stop(); // ABSOLUTELY NOT
    // this is here so no one ever thinks to do it ... Stop()/Start() should only be called from a controlling thread
    // and never this capture thread
    
    D("Capture thread stopped, waiting for join ...");
    SetState(STATE_CLOSING);
    pthread_exit(NULL);
}

void CaptureManager::GetRecCount() {
    string dataDiskLabel, lastDataDisk;
    unsigned long lastSecondsCount = 0, videoSecondsRecorded = 0;

    pthread_mutex_lock(&(em_data->mtx));
        dataDiskLabel = em_data->SYS_dataDiskLabel;
    pthread_mutex_unlock(&(em_data->mtx));

    ifstream fin((G_CONFIG.EM_DIR + "/" + FN_REC_COUNT).c_str());
    if(!fin.fail()) {
        fin >> lastDataDisk >> lastSecondsCount;
        fin.close();
        I("Last data disk was " + lastDataDisk + " with " + to_string(lastSecondsCount) + " secs");
    }

    if(lastDataDisk == "0" || (__SYS_GET_STATE & SYS_DATA_DISK_PRESENT && dataDiskLabel == lastDataDisk)) {
        videoSecondsRecorded = lastSecondsCount;
        D("videoSecondsRecorded = " + to_string(lastSecondsCount));
    } else {
        videoSecondsRecorded = 0;
        D("videoSecondsRecorded = 0");
    }

    pthread_mutex_lock(&(em_data->mtx));
        em_data->SYS_videoSecondsRecorded = videoSecondsRecorded;
    pthread_mutex_unlock(&(em_data->mtx));
}

void CaptureManager::WriteRecCount() {
    string dataDiskLabel;
    unsigned long videoSecondsRecorded;

    //if(__SYS_GET_STATE & SYS_TARGET_DISK_WRITABLE) {
        pthread_mutex_lock(&(em_data->mtx));
            dataDiskLabel = em_data->SYS_dataDiskLabel;
            videoSecondsRecorded = em_data->SYS_videoSecondsRecorded;
        pthread_mutex_unlock(&(em_data->mtx));

        if(dataDiskLabel.empty()) {
            dataDiskLabel = "0";
        }

        ofstream fout((G_CONFIG.EM_DIR + "/" + FN_REC_COUNT).c_str());

        if(!fout.fail()) {
            fout << dataDiskLabel << ' ' << videoSecondsRecorded;
            fout.close();
            D("Wrote out videoSecondsRecorded = " + to_string(videoSecondsRecorded) + " for disk " + dataDiskLabel);
        }
    //}
}