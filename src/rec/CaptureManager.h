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

#ifndef CAPTUREMANAGER_H
#define CAPTUREMANAGER_H
#include "StateMachine.h"
#include <sys/types.h>
#include <unistd.h>

#define RTSP_URL        "rtsp://1.1.1.%d:7070/track1"
#define _ACTIVE_CAMS    unsigned short i = 0; i < em_data->SYS_numCams; i++

class CaptureManager: public StateMachine {
    private:
        pthread_t pt_capture[MAX_CAMS];
        StateMachine *smCaptureThread[MAX_CAMS];
        EM_DATA_TYPE *em_data;
        pthread_mutex_t mtx_threadIndex, mtx_rtspStuff, mtx_threadStartedAtIteration;
        string baseVideoDirectory;
        unsigned long startedAtIteration[MAX_CAMS];
        off_t lastFileSize[MAX_CAMS];

        static void *thr_CaptureLoopLauncher(void*);
        void thr_CaptureLoop();
        void SyncCaptureThreads();
    
    public:
        CaptureManager(EM_DATA_TYPE*, string);
        ~CaptureManager();
        
        unsigned long Start();
        unsigned long Stop();
};

#endif