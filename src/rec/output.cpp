#include "output.h"

#include <iostream>
#include <sstream>

using namespace std;

extern pthread_mutex_t G_mtxOut;
extern __thread unsigned short __threadId;

void msgOut(string moduleName, string msg, unsigned short level) {
    string threadColor, threadName;
    if(__threadId == THREAD_MAIN)         { threadColor = C_RESET; threadName = "(main) "; }
    else if(__threadId == THREAD_AUX)     { threadColor = C_GREEN; threadName = "(aux)  " ; }
    else if(__threadId == THREAD_GPS)     { threadColor = C_MAGENTA; threadName = "(gps)  "; }
    else if(__threadId == THREAD_CAPTURE) { threadColor = C_CYAN; threadName = "(cap)  "; }

    if(!moduleName.empty()) msg = moduleName + ": " + msg;

    pthread_mutex_lock(&G_mtxOut);
    if(level == OUTPUT_NORMAL)     { cout << "      " << threadColor << threadName << msg; }
    else if(level == OUTPUT_INFO)  { cout << C_WHITE << "INFO: " << threadColor << threadName << msg; }
    else if(level == OUTPUT_WARN)  { cout << C_YELLOW; cerr << "WARN: "; cout << threadColor; cerr << threadName << msg; }
    else if(level == OUTPUT_ERROR) { cout << C_RED; cerr << "ERROR:"; cout << threadColor; cerr << threadName << msg; }
    else if(level == OUTPUT_DEBUG) { cout << C_BLUE; cerr << "DEBUG:"; cout << threadColor; cerr << threadName << msg; }
    cout << C_RESET << endl;
    pthread_mutex_unlock(&G_mtxOut);
}

string intToString(int i) {
    stringstream out;
    out << i;
    return out.str();
}