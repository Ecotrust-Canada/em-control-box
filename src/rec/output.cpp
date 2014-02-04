#include "output.h"

#include <iostream>
#include <sstream>

using namespace std;

extern pthread_mutex_t mtxOut;
extern __thread unsigned short threadId;

void msgOut(string moduleName, string msg, unsigned short level) {
    string threadColor, threadName;
    if(threadId == THREAD_MAIN)         { threadColor = C_RESET; threadName = "(main) "; }
    else if(threadId == THREAD_AUX)     { threadColor = C_GREEN; threadName = "(aux)  " ; }
    else if(threadId == THREAD_GPS)     { threadColor = C_MAGENTA; threadName = "(gps)  "; }
    else if(threadId == THREAD_CAPTURE) { threadColor = C_CYAN; threadName = "(cap)  "; }

    if(!moduleName.empty()) msg = moduleName + ": " + msg;

    pthread_mutex_lock(&mtxOut);
    if(level == OUTPUT_NORMAL)     { cout << "      " << threadColor << threadName << msg; }
    else if(level == OUTPUT_INFO)  { cout << C_WHITE << "INFO: " << threadColor << threadName << msg; }
    else if(level == OUTPUT_WARN)  { cout << C_YELLOW; cerr << "WARN: "; cout << threadColor; cerr << threadName << msg; }
    else if(level == OUTPUT_ERROR) { cout << C_RED; cerr << "ERROR:"; cout << threadColor; cerr << threadName << msg; }
    else if(level == OUTPUT_DEBUG) { cout << C_BLUE; cerr << "DEBUG:"; cout << threadColor; cerr << threadName << msg; }
    cout << C_RESET << endl;
    pthread_mutex_unlock(&mtxOut);
}

string intToString(int i) {
    stringstream out;
    out << i;
    return out.str();
}