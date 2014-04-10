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

#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

UsageEnvironment *env;
TaskScheduler *scheduler;
char* captureLoopWatchPtr;
MultiRTSPClient* rtspClients[DIGITAL_MAX_CAMS];
unsigned rtspClientCount; // shared between me and liveRTSP.cpp
//unsigned short nextFrameRate;

CaptureManager::CaptureManager(EM_DATA_TYPE* _em_data, string _videoDirectory):StateMachine(STATE_NOT_RUNNING, SM_EXCLUSIVE_STATES) {
    em_data = _em_data;
    videoDirectory = _videoDirectory;
    moduleName = "CAP";
    wasInReducedBitrateMode = false;
    //lastRecordMode = RECORDING_INACTIVE;
    //nextRecordMode = RECORDING_INACTIVE;

    if(__ANALOG) {
        I("Using *" + to_string(em_data->SYS_numCams+1) + "* ANALOG cameras");

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

        scheduler = BasicTaskScheduler::createNew();
        env = BasicUsageEnvironment::createNew(*scheduler);
        captureLoopWatchVar = LOOP_WATCH_VAR_NOT_RUNNING;
        captureLoopWatchPtr = &captureLoopWatchVar;
        rtspClientCount = 0;
    }
}

CaptureManager::~CaptureManager() {
    if(__IP) {
        env->reclaim();
        env = NULL;
        delete scheduler;
        scheduler = NULL;
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

            if(__ANALOG) {}
            else if(__IP) {
                for(_ACTIVE_CAMS) {
                    StreamClientState& scs = ((MultiRTSPClient*)rtspClients[i])->scs; // alias
                    env->taskScheduler().rescheduleDelayedTask(scs.periodicFileOutputTask, 0, (TaskFunc*)periodicFileOutputTimerHandler, rtspClients[i]);
                }
            }

            wasInReducedBitrateMode = false;
        } else if(!wasInReducedBitrateMode && __SYS_GET_STATE & SYS_REDUCED_VIDEO_BITRATE) {
            I("Switching to LOWER bitrate recording for next video");
            wasInReducedBitrateMode = true;
        }
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

                if(GetSecondsUntilNextClipTime() >= AUX_INTERVAL) {
                    KillAndReapZombieChildren(true);
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
                D("waitpid() error occurred; THIS SHOULDN'T HAPPEN");
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
                snprintf(fileName, sizeof(fileName), "%s/%s-cam%d.mp4", (em_data->SYS_targetDisk /*+ ((MultiRTSPClient*)rtspClient)->videoDirectory*/).c_str(), date, HARDCODED_CAM_INDEX);
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

        KillAndReapZombieChildren(true);
    }

    else if(__IP) {
        D("In IP mode");

        JoinIPCaptureThreadNonblocking(); // this will turn threads in STATE_CLOSING_OR_UNDEFINED into STATE_NOT_RUNNING

        if(GetState() & STATE_NOT_RUNNING) {
            if(pthread_create(&pt_capture, NULL, &thr_IPCaptureLoopLauncher, (void *)this) != 0) {
                E("Couldn't create IP camera capture thread");
            } else {
                D("Created IP camera capture thread");
            }
        } /*else if(GetState() & STATE_RUNNING) {
            // check that rtspClientCount == number of cams
            if (rtspClientCount != em_data->SYS_numCams) {

            }
        }*/
    }
    
    return initialState;
}

unsigned long CaptureManager::Stop() {
    unsigned long initialState = GetState();

    if(__ANALOG) {
        D("CaptureManager::Stop() ANALOG");
        pidReaperQueue.push(pid_encoder[HARDCODED_CAM_INDEX]);
        KillAndReapZombieChildren(false);
        SetState(STATE_NOT_RUNNING);
    }

    else if(__IP) {
        D("CaptureManager::Stop() IP");

        if(GetState() & STATE_STARTING || GetState() & STATE_RUNNING) {
            for(_ACTIVE_CAMS) {
                shutdownStream(rtspClients[i], LIVERTSP_EXIT_CLEAN);
            }
        }
        
        JoinIPCaptureThreadBlocking(); // this one sets STATE_NOT_RUNNING
    }

    return initialState;
}

unsigned short CaptureManager::GetSecondsUntilNextClipTime() {
    return (MAX_CLIP_LENGTH - (time(0) % MAX_CLIP_LENGTH));
}

void CaptureManager::KillAndReapZombieChildren(bool dontBlock) {
    int options = 0;
    if(dontBlock) options = WNOHANG;

    while(!pidReaperQueue.empty()) {
        pid_t pid = pidReaperQueue.front();
        
        D("Sending SIGTERM and waiting on PID " + to_string(pid));
        kill(pid, SIGTERM);
        if(waitpid(pid, NULL, options) > 0) {
            D("PID " + to_string(pid) + " reaped");
            pidReaperQueue.pop();
        }

        usleep(50000);
    }
}

void CaptureManager::JoinIPCaptureThreadBlocking() {
    if(GetState() & STATE_NOT_RUNNING) return;

    do {
        D("Waiting to join with capture thread ...");
        usleep(POLL_PERIOD / 10);
    } while(GetState() & STATE_RUNNING || GetState() & STATE_STARTING);

    JoinIPCaptureThreadNonblocking();
}

void CaptureManager::JoinIPCaptureThreadNonblocking() {
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
    extern __thread unsigned short threadId;
    threadId = THREAD_CAPTURE;

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
            /*MultiRTSPClient**/ rtspClients[i] = new MultiRTSPClient(*env,
                                                    url,
                                                    LIVERTSP_CLIENT_VERBOSITY_LEVEL,
                                                    "em-rec",
                                                    videoDirectory,
                                                    i, /* cam index */
                                                    MAX_CLIP_LENGTH,
                                                    em_data/*,
                                                    frameRate*/);
            if (rtspClients[i] == NULL) {
                E("Failed to create a RTSP client for URL '" + url + "': " + env->getResultMsg());
                continue;
            } else {
                D("Created a RTSP client for URL '" + url + "'");
            }

            rtspClientCount++;
            madeProgress = true;

            // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
            // Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
            // Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
            rtspClients[i]->sendDescribeCommand(continueAfterDESCRIBE);
        }
        D("Finished Live555 init");
    }
    
    if(madeProgress && GetState() & STATE_STARTING && __EM_RUNNING && __SYS_GET_STATE & SYS_TARGET_DISK_WRITABLE) {
        D("Beginning Live555 doEventLoop() ...");

        captureLoopWatchVar = LOOP_WATCH_VAR_RUNNING;
        SetState(STATE_RUNNING);
        //__SYS_SET_STATE(SYS_VIDEO_RECORDING);
        
        // does not return unless captureLoopWatchVar set to non-zero
        env->taskScheduler().doEventLoop(&captureLoopWatchVar);
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

// void CaptureManager::ShutdownRTSPStreams() {
//     for(_ACTIVE_CAMS) {
//         shutdownStream(rtspClient[i], LIVERTSP_EXIT_CLEAN);
//     }
// }