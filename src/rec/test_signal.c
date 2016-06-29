#include <stdio.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <cstdio>
#include <pthread.h>
#include <errno.h>
#include <string>
#include <cstring>
#include <unistd.h>

#define POLL_PERIOD 				1000000 // how many microseconds (default 1 sec) DO NOT ALTER
#define AUX_INTERVAL				5       // how many POLL_PERIODs before something different happens (all intervals in units of POLL_PERIOD) 5 seconds
#define THREAD_AUX          1

int em_running = 0;
int video_recording = 0;
struct tm      *G_timeinfo;
__thread unsigned short __threadId;

void exit_handler(int s) {
    // I("Ecotrust EM Recorder v" + VERSION + " is stopping ...");
    // smRecorder.SetState(STATE_NOT_RUNNING);
    em_running = 0;
    video_recording = 0;
    printf("Ecotrust EM Recorder Simulator is stopping ...\n");
    exit(EXIT_SUCCESS);
}

/**
 * Read one line text file that contains command from nodejs server
 */
void process_command_handler(int s) {
    const char* runConfigFile = "/tmp/em_command.conf";
    printf("Within  process_command_handler\n");   
    char command[1024];
    FILE *fp = fopen(runConfigFile, "r");
    if (fp == NULL) {
      printf("Failed to open %s file\n", runConfigFile);
    } else {
      fscanf(fp,"%[^\n]",command);    
      printf("%s\n",command);

      if ( strcmp(command,"stop_video_recording") == 0){
        video_recording = 0;
      } else if (strcmp(command,"start_video_recording") == 0){
        video_recording = 1;
      } else if (strcmp(command,"reset_trip") == 0){
      } else if (strcmp(command,"reset_string") == 0){
      }
    }
    return;
}

void *thr_auxiliaryLoop(void *arg) {
    const unsigned int AUX_PERIOD = AUX_INTERVAL * POLL_PERIOD;
    const char *scrot_envp[] = { "DISPLAY=:0", NULL };
    pid_t pid_scrot;

    __threadId = THREAD_AUX;
    std::string targetDisk = "/mnt/data";
    mkdir("/mnt/data/screenshots", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    while (em_running){
      if (video_recording) {
            pid_scrot = fork();
            
            if(pid_scrot == 0) {
                execle("/usr/bin/scrot", "scrot", "-q5", (targetDisk + "/screenshots/%Y%m%d-%H%M%S.jpg").c_str(), NULL, scrot_envp);
            } else if(pid_scrot > 0) {
                waitpid(pid_scrot, NULL, 0);
                //smTakeScreenshot.UnsetAllStates();
                printf("Took screenshot\n");
            } else {
                printf("/usr/bin/scrot fork() failed\n");
            }
      }
      usleep((AUX_PERIOD) / 4);
    }

    pthread_exit(NULL);
}

int main(void) {
    pthread_t pt_aux;
    char date[24] = { '\0' };
    time_t rawtime;
    int retVal;

    signal(SIGINT, exit_handler);
    signal(SIGHUP, exit_handler);
    signal(SIGTERM, exit_handler);
    signal(SIGUSR1, process_command_handler);
    
    em_running = 1;
    video_recording = 1;
    // spawn auxiliary thread
    if((retVal = pthread_create(&pt_aux, NULL, &thr_auxiliaryLoop, NULL)) != 0) {
        fprintf(stderr,"Couldn't spawn auxiliary thread\n");
    } 
    
    printf("Spawned auxiliary thread\n");

    while(em_running) {

        // get date
        time(&rawtime);
        G_timeinfo = localtime(&rawtime);
        strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", G_timeinfo); // writes null-terminated string so okay not to clear DATE_BUF every time
        printf("Current date: %s\n",date);

        usleep(POLL_PERIOD);
    }

    printf("Joining aux thread ... ");
    if(pthread_join(pt_aux, NULL) == 0) {
        //smAuxThread.SetState(STATE_NOT_RUNNING);
        printf("Auxiliary thread stopped");
    }
    exit(EXIT_SUCCESS);
}
