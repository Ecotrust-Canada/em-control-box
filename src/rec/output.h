#ifndef OUTPUT_H
#define OUTPUT_H

    extern pthread_mutex_t mtxOut;

    #define C_RED    "\33[0;31m"
    #define C_GREEN  "\33[0;32m"
    #define C_YELLOW "\33[0;33m"
    #define C_MAGENTA "\33[0;35m"
    #define C_CYAN   "\33[0;36m"
    #define C_RESET  "\33[0m"

    #define O(s) pthread_mutex_lock(&mtxOut); cout << s << endl; pthread_mutex_unlock(&mtxOut);
    #define I(s) pthread_mutex_lock(&mtxOut); cout << "INFO: " << s << endl; pthread_mutex_unlock(&mtxOut);
    #define W(s) pthread_mutex_lock(&mtxOut); cout << C_YELLOW << "WARN: " << s << C_RESET << endl; pthread_mutex_unlock(&mtxOut);
    #define E(s) pthread_mutex_lock(&mtxOut); cout << C_RED << "ERROR: " << s << C_RESET << endl; pthread_mutex_unlock(&mtxOut);

    #ifdef DEBUG
        #define D(s) pthread_mutex_lock(&mtxOut); cerr << C_GREEN << "DEBUG: " << s << C_RESET << endl; pthread_mutex_unlock(&mtxOut);
        #define D2(s) pthread_mutex_lock(&mtxOut); cerr << C_MAGENTA << "DEBUG: " << s << C_RESET << endl; pthread_mutex_unlock(&mtxOut);
        #define D3(s) pthread_mutex_lock(&mtxOut); cerr << C_CYAN << "DEBUG: " << s << C_RESET << endl; pthread_mutex_unlock(&mtxOut);
        #define OVERRIDE_SILENCE        true
    #else
        #define D(s)
        #define D2(s)
        #define D3(s)
        #define OVERRIDE_SILENCE        false
    #endif

#endif