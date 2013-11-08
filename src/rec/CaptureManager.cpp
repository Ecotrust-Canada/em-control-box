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
#include <cstdio>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>

extern "C" {
    //#include <nemesi/rtsp.h>
    #include <nemesi/rtp.h>
    #include <nemesi/sdp.h>
}

using namespace std;

CaptureManager::CaptureManager(EM_DATA_TYPE* _em_data, string base):
    StateMachine(STATE_NOT_RUNNING, true) {
    em_data = _em_data;
    baseVideoDirectory = base;

    if(__ANALOG) {
        I("Using ANALOG cameras");
    } else if(__IP) {
        I("Using *" << em_data->SYS_numCams << "* IP cameras");

        for(_ACTIVE_CAMS) {
            smCaptureThread[i] = new StateMachine(STATE_NOT_RUNNING, true);
            rtsp_init_done[i] = false;
        }
    }

    pthread_mutex_init(&mtx_threadIndex, NULL);
    pthread_mutex_init(&mtx_rtspStuff, NULL);
    pthread_mutex_init(&mtx_threadStartedAtIteration, NULL);

    mkdir(baseVideoDirectory.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
}

CaptureManager::~CaptureManager() {
    //for(_ACTIVE_CAMS) {
    //    if(rtsp_init_done[i]) rtsp_uninit(_rtsp_ctrl[i]);
   // }

    pthread_mutex_destroy(&mtx_threadIndex);
    pthread_mutex_destroy(&mtx_rtspStuff);
    pthread_mutex_destroy(&mtx_threadStartedAtIteration);
}

// check if process is still alive
// check that video files are growing
// if not: force close PID, clean up, SetState(VIDEO_NOT_RUNNING);
unsigned long CaptureManager::Start() {
    unsigned long initialState = GetState();
    unsigned long em_runIterations, threadStartedAtIteration;
    string currentVideoFile;
    struct stat st;

    if(!(__OS_DISK_FULL)) {
        // Do all the boring checks and pre-emptory stuff
        if(GetState() & STATE_RUNNING) { // if already running
            D2("CM Start()");

            pthread_mutex_lock(&(em_data->mtx));
                em_runIterations = em_data->runIterations;
            pthread_mutex_unlock(&(em_data->mtx));

            for(_ACTIVE_CAMS) {
                pthread_mutex_lock(&mtx_threadStartedAtIteration);
                    threadStartedAtIteration = startedAtIteration[i];
                pthread_mutex_unlock(&mtx_threadStartedAtIteration);

                if(threadStartedAtIteration > 0 &&
                   em_runIterations - threadStartedAtIteration > MAX_CLIP_LENGTH) {
                    pthread_mutex_lock(&mtx_threadStartedAtIteration);
                        startedAtIteration[i] = 0;
                    pthread_mutex_unlock(&mtx_threadStartedAtIteration);
                    smCaptureThread[i]->SetState(STATE_CLOSING_OR_UNDEFINED);
                    D2("MAX_CLIP_LENGTH reached for " << i << ", restarting");
                }

                if(smCaptureThread[i]->GetState() & STATE_RUNNING) {
                    pthread_mutex_lock(&(em_data->mtx));
                        currentVideoFile = em_data->SYS_currentVideoFile[i];
                    pthread_mutex_unlock(&(em_data->mtx));

                    stat(currentVideoFile.c_str(), &st);
                    
                    if(lastFileSize[i] == st.st_size) {
                        D2("File not growing, restarting that thread");
                        lastFileSize[i] = -1;
                        smCaptureThread[i]->SetState(STATE_CLOSING_OR_UNDEFINED);
                    } else {
                        lastFileSize[i] = st.st_size;
                    }
                }
            }
        }

        // Now actually start things up
        if(__ANALOG) {
            D2("Start Analog");
            E("Analog captures not supported in this release!");
            //SetState(STATE_RUNNING);
        } else if(__IP) {
            D2("About to sync CM threads");
            SyncCaptureThreads(); // this will turn threads in STATE_CLOSING_OR_UNDEFINED into STATE_NOT_RUNNING

            SetState(STATE_RUNNING);

            for(_ACTIVE_CAMS) {
                D("Before pthread_create " << i << ", state = " << smCaptureThread[i]->GetState());
                if(smCaptureThread[i]->GetState() & STATE_NOT_RUNNING) {
                    if(pthread_create(&pt_capture[i], NULL, &thr_CaptureLoopLauncher, (void *)this) != 0) {
                        smCaptureThread[i]->SetState(STATE_NOT_RUNNING); // eh why not
                        E("Couldn't create IP camera capture thread " << i);
                    } else {
                        D2("CREATED " << i);
                        usleep(POLL_PERIOD / 10); // delay the thread creation a bit
                    }
                }
            }
        }
    }
    
    return initialState;
}

unsigned long CaptureManager::Stop() {
    unsigned long initialState = GetState();

    ((StateMachine *)em_data->sm_system)->UnsetState(SYS_VIDEO_RECORDING);
    ((StateMachine *)em_data->sm_system)->SetState(SYS_VIDEO_NOT_RECORDING);

    if(__ANALOG) {
        D2("Stop Analog");
        //SetState(STATE_NOT_RUNNING);
    } else if(__IP) {
        D2("Stop IP");
        SetState(STATE_NOT_RUNNING);
        SyncCaptureThreads();
    }

    return initialState;
}

void CaptureManager::SyncCaptureThreads() {
    //bool alreadyWaited = false;
    usleep(POLL_PERIOD / 10);

    D2("Going into sync, these are the states:");
    #ifdef DEBUG
    for(_ACTIVE_CAMS) {
        signed long s = smCaptureThread[i]->GetState();
        D2("Thread " << i << " = " << s);
    }
    #endif

    for(_ACTIVE_CAMS) {
        if(smCaptureThread[i]->GetState() & STATE_CLOSING_OR_UNDEFINED) {
            //if(!alreadyWaited) {
            //    usleep(POLL_PERIOD);
            //    alreadyWaited = true;
            //}

            D2("Joining with " << i);
            if(pthread_join(pt_capture[i], NULL) == 0) {
                smCaptureThread[i]->SetState(STATE_NOT_RUNNING);
                D2("Synced IP capture thread " << i);
            }
        }
    }
}

void *CaptureManager::thr_CaptureLoopLauncher(void *self) {
    ((CaptureManager*)self)->thr_CaptureLoop();
    return NULL;
}

void CaptureManager::thr_CaptureLoop() {
    signed short threadIndex = -1;

    // find the first thread index in the smCaptureThread object that is in NOT_RUNNING and use it as my own
    pthread_mutex_lock(&mtx_threadIndex);
        for(_ACTIVE_CAMS) {
            if(smCaptureThread[i]->GetState() & STATE_NOT_RUNNING) {
                threadIndex = i;
                smCaptureThread[threadIndex]->SetState(STATE_RUNNING);
                D3("threadIndex = " << threadIndex);
                break;
            }
        }
    pthread_mutex_unlock(&mtx_threadIndex);

    // we never found an available index (this should never happen as these threads should only be created when STATE_NOT_RUNNING for some particular index); alas, maybe I have a sync problem, this is added protection
    if(threadIndex == -1) {
        D3("Never found an index, exiting ...");
        pthread_exit(NULL);
    }

    ((StateMachine *)em_data->sm_system)->UnsetState(SYS_VIDEO_RECORDING);
    ((StateMachine *)em_data->sm_system)->SetState(SYS_VIDEO_NOT_RECORDING);

    if(GetState() & STATE_RUNNING &&
        smCaptureThread[threadIndex]->GetState() & STATE_RUNNING &&
        __EM_RUNNING &&
        !(__OS_DISK_FULL)) {
        
        time_t rawtime;
        struct tm *timeinfo;
        int fd;
        char url[64], date[32], fileName[96];
        
        rtp_thread *_rtp_thread;
        rtp_ssrc *_rtp_ssrc;
        rtp_buff _rtp_buff;
        rtp_frame _rtp_frame;

        nms_rtsp_hints _rtsp_hints = { -1 };
        _rtsp_hints.pref_rtsp_proto = TCP;
        _rtsp_hints.pref_rtp_proto = TCP;

REINIT_RTSP:        
        D3("States good, beginning RTSP init for " << threadIndex);
        pthread_mutex_lock(&mtx_rtspStuff);
            //snprintf(url, sizeof(url), RTSP_URL, threadIndex + 1); // PRODUCTION
            snprintf(url, sizeof(url), RTSP_URL, 1); // HACK to force multi-cam testing with just one cam

            if(!rtsp_init_done[threadIndex]) {
                if((_rtsp_ctrl[threadIndex] = rtsp_init(&_rtsp_hints)) == NULL) {
                    E("rtsp_init() failed in thread " << threadIndex);
                    ((StateMachine *)em_data->sm_system)->UnsetState(SYS_VIDEO_AVAILABLE);
                    smCaptureThread[threadIndex]->SetState(STATE_CLOSING_OR_UNDEFINED);
                    pthread_mutex_unlock(&mtx_rtspStuff);
                    pthread_exit(NULL);
                } else {
                    D3("rtsp_init() success");
                } 

                if (rtsp_open(_rtsp_ctrl[threadIndex], url)) {
                    E("rtsp_open() failed for thread " << threadIndex);
                    rtsp_uninit(_rtsp_ctrl[threadIndex]);

                    ((StateMachine *)em_data->sm_system)->UnsetState(SYS_VIDEO_AVAILABLE);
                    smCaptureThread[threadIndex]->SetState(STATE_CLOSING_OR_UNDEFINED);
                    pthread_mutex_unlock(&mtx_rtspStuff);
                    pthread_exit(NULL);
                }

                rtsp_wait(_rtsp_ctrl[threadIndex]);
                _rtsp_session[threadIndex] = _rtsp_ctrl[threadIndex]->rtsp_queue; // get the session information
                D3("rtsp_open() finished and got session " << _rtsp_session[threadIndex]);

                if (!_rtsp_session[threadIndex]) {
                    E("No RTSP session available for camera " << threadIndex);
                    rtsp_close(_rtsp_ctrl[threadIndex]);
                    rtsp_wait(_rtsp_ctrl[threadIndex]);
                    rtsp_uninit(_rtsp_ctrl[threadIndex]);

                    ((StateMachine *)em_data->sm_system)->UnsetState(SYS_VIDEO_AVAILABLE);
                    smCaptureThread[threadIndex]->SetState(STATE_CLOSING_OR_UNDEFINED);
                    pthread_mutex_unlock(&mtx_rtspStuff);
                    pthread_exit(NULL);
                }

                rtsp_init_done[threadIndex] = true;
            } else {
                D3("Skipped basic RTSP init for " << threadIndex);
            }
        pthread_mutex_unlock(&mtx_rtspStuff);
        D3("Finished RTSP init for " << threadIndex);

        // File name / opening handling
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(date, sizeof(date), "%Y%m%d-%H%M%S", timeinfo);
        snprintf(fileName, sizeof(fileName), "%s/%s-cam%d", baseVideoDirectory.c_str(), date, threadIndex + 1);

        pthread_mutex_lock(&mtx_threadStartedAtIteration);
        pthread_mutex_lock(&(em_data->mtx));
            startedAtIteration[threadIndex] = em_data->runIterations;
            em_data->SYS_currentVideoFile[threadIndex] = fileName;
        pthread_mutex_unlock(&(em_data->mtx));
        pthread_mutex_unlock(&mtx_threadStartedAtIteration);
        
        if((fd = creat(fileName, 00644)) == -1) {
            E("Couldn't create file " << fileName);

            if(errno == ENOSPC) {
                ((StateMachine *)em_data->sm_system)->SetState(SYS_OS_DISK_FULL);
            }

            rtsp_close(_rtsp_ctrl[threadIndex]);
            rtsp_wait(_rtsp_ctrl[threadIndex]);
            rtsp_uninit(_rtsp_ctrl[threadIndex]);
            
            rtsp_init_done[threadIndex] = false;
            smCaptureThread[threadIndex]->SetState(STATE_CLOSING_OR_UNDEFINED);
            pthread_exit(NULL);
        }

        D3("calling rtsp_play() ... ");
        if((rtsp_play(_rtsp_ctrl[threadIndex], 0.0, 0.0)) != 0) {
            rtsp_init_done[threadIndex] = false;
            goto REINIT_RTSP;
        }
        D3("rtsp_play() succeeded");
        rtsp_wait(_rtsp_ctrl[threadIndex]);

        D3("calling rtsp_get_rtp_th() ...");
        _rtp_thread = rtsp_get_rtp_th(_rtsp_ctrl[threadIndex]);

        // no more pthread_exits beyond this point
        // everything now in a loop dependent on run states

        ((StateMachine *)em_data->sm_system)->SetState(SYS_VIDEO_AVAILABLE);
        
        ((StateMachine *)em_data->sm_system)->UnsetState(SYS_VIDEO_NOT_RECORDING);
        ((StateMachine *)em_data->sm_system)->SetState(SYS_VIDEO_RECORDING);

        while(GetState() & STATE_RUNNING &&
            smCaptureThread[threadIndex]->GetState() & STATE_RUNNING &&
            __EM_RUNNING &&
            !(__OS_DISK_FULL) &&
            !rtp_fill_buffers(_rtp_thread)) {
            for (_rtp_ssrc = rtp_active_ssrc_queue(rtsp_get_rtp_queue(_rtsp_ctrl[threadIndex])); _rtp_ssrc; _rtp_ssrc = rtp_next_active_ssrc(_rtp_ssrc)) {
                if (!rtp_fill_buffer(_rtp_ssrc, &_rtp_frame, &_rtp_buff)) { // parse the stream
                    if (write(fd, _rtp_frame.data, _rtp_frame.len) < _rtp_frame.len) {
                        E("Couldn't write whole RTSP packet for camera " << threadIndex);

                        ((StateMachine *)em_data->sm_system)->SetState(SYS_OS_DISK_FULL);
                        smCaptureThread[threadIndex]->SetState(STATE_CLOSING_OR_UNDEFINED);
                        break;
                    }
                }
            }
        } D3("Capture loop broken for " << threadIndex);

        ((StateMachine *)em_data->sm_system)->UnsetState(SYS_VIDEO_RECORDING);
        ((StateMachine *)em_data->sm_system)->SetState(SYS_VIDEO_NOT_RECORDING);
        
        close(fd);
        rename(fileName, (string(fileName) + ".h264").c_str());
        
        pthread_mutex_lock(&mtx_rtspStuff);
            if(lastFileSize[threadIndex] != -1) { // normal stop
                D3("pre rtsp_stop"); rtsp_stop(_rtsp_ctrl[threadIndex]);D3("post rtsp_stop");
                D3("pre rtsp_wait"); rtsp_wait(_rtsp_ctrl[threadIndex]);D3("post rtsp_wait");
            } else { // file stopped growing; could mean disconnection, network stack failure, etc.
                D3("pre rtsp_close");rtsp_close(_rtsp_ctrl[threadIndex]);D3("post rtsp_close");
                D3("pre rtsp_wait");rtsp_wait(_rtsp_ctrl[threadIndex]);D3("post rtsp_wait");
                D3("pre rtsp_uninit");rtsp_uninit(_rtsp_ctrl[threadIndex]);D3("post rtsp_uninit");

                rtsp_init_done[threadIndex] = false;
            }

            // rtsp_wait will set REINITIALIZED if reconnected; we need to run reinit routes then
        pthread_mutex_unlock(&mtx_rtspStuff);
    } // end massive if

    D3("Capture thread " << threadIndex << " exiting ...");
    smCaptureThread[threadIndex]->SetState(STATE_CLOSING_OR_UNDEFINED);
    pthread_exit(NULL);
}
