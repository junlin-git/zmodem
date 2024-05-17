#include "zglobal.h"
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
static zreadline_t zr_cache;
zreadline_t *zreadline_init(int fd, size_t readnum, size_t bufsize, int no_timeout)
{
    memset (&zr_cache, 0, sizeof(zreadline_t));
    zr_cache.readline_fd = fd;
    zr_cache.readline_readnum = readnum;
    zr_cache.readline_buffer = malloc(bufsize > readnum ? bufsize : readnum);
    if (!zr_cache.readline_buffer) {
        log_fatal("out of memory");
        exit(1);
    }
    zr_cache.no_timeout = no_timeout;
    return &zr_cache;
}


/*
 * This version of readline is reasonably well suited for
 * reading many characters.
 *
 * timeout is in tenths of seconds
 */
static int readline_internal(zreadline_t *zr, unsigned int timeout)
{
    zr->readline_ptr = zr->readline_buffer;
    unsigned int loop=0;
    while (1) {
        loop++;
        if(loop>timeout)break;
        zr->readline_left = read(zr->readline_fd,
                                 zr->readline_ptr,
                                 zr->readline_readnum);
        if(zr->readline_left>0)break;
        usleep(1000*10);
    }
    if (zr->readline_left == -1)
        log_trace("Read failure :%s\n", strerror(errno));

    if (zr->readline_left < 1)
        return TIMEOUT;
    zr->readline_left -- ;
    char c = *zr->readline_ptr;
    zr->readline_ptr++;
    return (unsigned char) c;
}


int zreadline_getc(zreadline_t *zr, int timeout)
{
    zr->readline_left --;
    if (zr->readline_left >= 0) {
        char c = *(zr->readline_ptr);
        zr->readline_ptr ++;
        return (unsigned char) c;
    }
    else
        return readline_internal(zr, timeout);
}

void zreadline_flush(zreadline_t *zr)
{
    zr->readline_left=0;
    return;
}


void zreadline_write(char c)
{
     write(zr_cache.readline_fd,&c,1);
}
/* send cancel string to get the other end to shut up */
void zreadline_canit (zreadline_t *zr)
{
    static char canistr[] =
    {
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 0
    };
    write(zr->readline_fd,canistr,strlen(canistr));
}


void HI_LOG_Print(char *info,const char *pszFunc,int u32Line,char *pszFmt, ...)
{
    va_list args;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);
    fprintf(stdout, "%s[%02d:%02d:%02d:%03ld]:%s:%d",info, tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec / 1000,pszFunc,u32Line);
    va_start(args, pszFmt);
    vfprintf(stdout, pszFmt, args);
    va_end(args);
    printf("\r\n");
    return;
}


