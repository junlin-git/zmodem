#include "zglobal.h"
#include "timing.h"


double timing (int reset, time_t *nowp)
{
    static double elaptime, starttime, stoptime;
    double yet;
    struct timeval tv;
    struct timezone tz;

#ifdef DST_NONE
    tz.tz_dsttime = DST_NONE;
#else
    tz.tz_dsttime = 0;
#endif
    gettimeofday (&tv, &tz);
    yet=tv.tv_sec + tv.tv_usec/1000000.0;

    if (nowp)
        *nowp=(time_t) yet;
    if (reset) {
        starttime = yet;
        return starttime;
    }
    else {
        stoptime = yet;
        elaptime = stoptime - starttime;
        return elaptime;
    }
}

/*#define TEST*/
#ifdef TEST
main()
{
    int i;
    display("timing %g",timing(1));
    display("timing %g",timing(0));
    for(i=0;i<20;i++){
        sleep(1);
        display("timing %g",timing(0));
    }
}
#endif
