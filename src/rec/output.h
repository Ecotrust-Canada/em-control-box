#include <string>

#ifndef OUTPUT_H
#define OUTPUT_H

#define THREAD_MAIN     0
#define THREAD_AUX      1
#define THREAD_GPS      2
#define THREAD_CAPTURE  3

#define OUTPUT_NORMAL   0
#define OUTPUT_INFO     1
#define OUTPUT_WARN     2
#define OUTPUT_ERROR    3
#define OUTPUT_DEBUG    4

#define C_RED     "\33[0;31m"
#define C_GREEN   "\33[0;32m"
#define C_YELLOW  "\33[0;33m"
#define C_BLUE    "\33[1;34m"
#define C_MAGENTA "\33[0;35m"
#define C_CYAN    "\33[0;36m"
#define C_WHITE   "\33[1;37m"
#define C_RESET   "\33[0m"

void msgOut(std::string, std::string, unsigned short);
std::string intToString(int);

#define O(s) msgOut(moduleName, string("") + s, OUTPUT_NORMAL);
#define I(s) msgOut(moduleName, string("") + s, OUTPUT_INFO);
#define W(s) msgOut(moduleName, string("") + s, OUTPUT_WARN);
#define E(s) msgOut(moduleName, string("") + s, OUTPUT_ERROR);

#ifdef DEBUG
    #define D(s) msgOut(moduleName, string("") + s, OUTPUT_DEBUG);
    #define OVERRIDE_SILENCE        true
#else
    #define D(s)
    #define OVERRIDE_SILENCE        false
#endif

#endif