#ifndef LIBZMODEM_ZREADLINE_H
#define LIBZMODEM_ZREADLINE_H

#include <stddef.h>

typedef struct zreadline_ {
    char *readline_ptr; /* pointer for removing chars from linbuf */
    int readline_left; /* number of buffered chars left to read */
    size_t readline_readnum;
    int readline_fd;
    char *readline_buffer;
    int no_timeout; 	/* when true, readline does not timeout */
}zreadline_t;

zreadline_t *zreadline_init(int fd, size_t readnum, size_t bufsize, int no_timeout);
void zreadline_flush (zreadline_t *zr);
int zreadline_getc(zreadline_t *zr, int timeout);
void zreadline_write(char c);
void zreadline_canit (zreadline_t *zr);
void HI_LOG_Print(char *info,const char *pszFunc,int u32Line,char *pszFmt, ...);
#endif
