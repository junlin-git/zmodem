#ifndef ZMODEM_GLOBAL_H
#define ZMODEM_GLOBAL_H

#include <sys/types.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#include <sys/select.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <locale.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "zreadline.h"

#define OK 0
#define FALSE 0
#define TRUE 1
#define ERROR (-1)

/* Ward Christensen / CP/M parameters - Don't change these! */
#define ENQ 005
#define CAN ('X'&037)
#define XOFF ('s'&037)
#define XON ('q'&037)
#define SOH 1
#define STX 2
#define EOT 4
#define ACK 6
#define NAK 025
#define CPMEOF 032
#define WANTCRC 0103    /* send C not NAK to get crc not checksum */
#define WANTG 0107  /* Send G not NAK to get nonstop batch xmsn */
#define TIMEOUT (-2)
#define RCDO (-3)
#define WCEOT (-10)

#define RETRYMAX 10
#define UNIXFILE 0xF000  /* The S_IFMT file mask bit for stat */
#define DEFBYTL 2000000000L	/* default rx file size */



enum zm_type_enum {
    ZM_ZMODEM
};

struct zm_fileinfo {
    char *fname;
    time_t modtime;
    mode_t mode;
    size_t bytes_total;
    size_t bytes_sent;
    size_t bytes_received;
    size_t bytes_skipped; /* crash recovery */
    int    eof_seen;
};

#define R_BYTESLEFT(x) ((x)->bytes_total-(x)->bytes_received)

#define log_info(fmt, args...) HI_LOG_Print("info",__FUNCTION__, __LINE__,fmt, ##args)
#define log_error(fmt, args...) HI_LOG_Print("error",__FUNCTION__, __LINE__, fmt, ##args)
#define log_trace(fmt, args...) HI_LOG_Print("trace",__FUNCTION__, __LINE__, fmt, ##args)
#define log_fatal(fmt, args...) HI_LOG_Print("fatal",__FUNCTION__, __LINE__, fmt, ##args)
#define log_debug(fmt, args...) HI_LOG_Print("debug",__FUNCTION__, __LINE__, fmt, ##args)


#endif
