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

#include "settings.h"
#include "StateMachine.h"
#include "liveRTSP.h"
#include <queue>
#include <sys/types.h>
#include <unistd.h>

#define _ACTIVE_CAMS   unsigned short i = 0; i < em_data->SYS_numCams; i++

class CaptureManager: public StateMachine {
    private:
        string moduleName;
        EM_DATA_TYPE *em_data;
        string videoDirectory;
        char encoderArgs[512];
        char encoderArgsSlow[512];
        bool wasInReducedBitrateMode;
        pid_t pid_encoder[ANALOG_MAX_CAMS];
        std::queue<pid_t> pidReaperQueue;

        // IP
        pthread_t pt_capture;
        pthread_t pt_threads[5];
        char captureLoopWatchVar;
        //MultiRTSPClient* rtspClients[DIGITAL_MAX_CAMS];
        //unsigned long startedAtIteration[IP_MAX_CAMS]; // ? needed?
        //off_t lastFileSize[IP_MAX_CAMS]; /// ? needed?

        static void *thr_IPCaptureLoopLauncher(void*);
        void thr_IPCaptureLoop();
        void JoinIPCaptureThreadBlocking();
        void JoinIPCaptureThreadNonBlocking();
        static void *thr_ScreenshotsLoop(void*);
        //void SetIPOutputFrameRate(unsigned short);

        struct tm *waitingtime;
        time_t rawwaittime;

    public:
        CaptureManager(EM_DATA_TYPE*, string);
        ~CaptureManager();
        unsigned long Start();
        unsigned long Stop();
        unsigned short GetSecondsUntilNextClipTime();
        void ReapZombieChildren(bool); // sounds fantastic
        void ReapZombieChildrenBlocking();
        void ReapZombieChildrenNonBlocking();
        void GetRecCount();
        void WriteRecCount();
        void TakeScreenShots();
};

#endif
