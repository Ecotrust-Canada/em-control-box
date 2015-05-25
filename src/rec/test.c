#include <stdio.h>
#include <time.h>

int gs() {
    time_t nowEpoch, thenEpoch;
    struct tm timeNow, timeThen;

    time(&nowEpoch);
    timeNow = *localtime(&nowEpoch);
    timeThen.tm_zone = timeNow.tm_zone;
    timeThen.tm_isdst = timeNow.tm_isdst;
    timeThen.tm_year = timeNow.tm_year;
    timeThen.tm_mon = timeNow.tm_mon;
    timeThen.tm_mday = timeNow.tm_mday;
    timeThen.tm_hour = timeNow.tm_hour + 1;
    timeThen.tm_min = 0;
    timeThen.tm_sec = 0;
    thenEpoch = mktime(&timeThen);
    //if(date != NULL) strftime(date, sizeof(*date), "%Y%m%d-%H%M%S", &timeNow);
    return (thenEpoch - nowEpoch);    
}

int gs2() {
    int res;
    time_t nowEpoch;
    time(&nowEpoch);
    res = nowEpoch % 3600;
    return (3600 - res);
}

int main(void) {
    while(1) {
        printf("%d %d\n", gs(), gs2());
        usleep(250000);
    }
}