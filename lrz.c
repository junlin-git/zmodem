/*
  lrz - receive files with x/y/zmodem
  Copyright (C) until 1988 Chuck Forsberg (Omen Technology INC)
  Copyright (C) 1994 Matt Porter, Michael D. Black
  Copyright (C) 1996, 1997 Uwe Ohse
  Copyright (C) 2018 Michael L. Gran

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
  02111-1307, USA.

  originally written by Chuck Forsberg
*/
#define SS_NORMAL 0
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <utime.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "zglobal.h"
#include "crctab.h"
#include "zm.h"
#include "zm.h"
#include "uart.h"
#define MAX_BLOCK 8192


struct rz_ {
    zm_t *zm;		/* Zmodem comm primitives' state. */
    // Workspaces
    char attn[ZATTNLEN+1];  /* Attention string rx sends to tx on err */
    char secbuf[MAX_BLOCK + 1]; /* Workspace to store up to 8k
                     * blocks */
    // Dynamic state
    FILE *fout;		/* FP to output file. */
    int lastrx;		/* Either 0, or CAN if last receipt
                 * was sender-cancelled */
    int firstsec;
    int errors;		/* Count of read failures */
    char *pathname;		/* filename of the file being received */
    int thisbinary;		/* When > 0, current file is to be
                 * received in bin mode */
    char zconv;		/* ZMODEM file conversion request. */
    char zmanag;		/* ZMODEM file management request. */
    char ztrans;		/* SET BUT UNUSED: ZMODEM file transport
                 * request byte */
    int tryzhdrtype;         /* Header type to send corresponding
                  * to Last rx close */

    // Constant
    int restricted;	/* restricted; no /.. or ../ in filenames */
    /*  restricted > 0 prevents unlinking
                 restricted > 0 prevents overwriting dot files
                 restricted = 2 prevents overwriting files
                 restricted > 0 restrict files to curdir or PUBDIR
               */
    int rxclob;		/* A flag. Allow clobbering existing file */
    int try_resume; /* A flag. When true, try restarting downloads */
    int o_sync;		/* A flag. When true, each write will
                 * be reliably completed before
                 * returning. */
    unsigned long min_bps;	/* When non-zero, sets a minimum allow
                 * transmission rate.  Dropping below
                 * that rate will cancel the
                 * transfer. */
    long min_bps_time;	/* Length of time transmission is
                 * allowed to be below 'min_bps' */
    char lzmanag;		/* Local file management
                   request. ZF1_ZMAPND, ZF1_ZMCHNG,
                   ZF1_ZMCRC, ZF1_ZMNEWL, ZF1_ZMNEW,
                   ZF1_ZMPROT, ZF1_ZMCRC, or 0 */
    time_t stop_time;	/* Zero or seconds in the epoch.  When
                 * non-zero, indicates a shutdown
                 * time. */
    int under_rsh;		/* A flag.  Set to true if we're
                 * running under a restricted
                 * environment. When true, files save
                 * as 'rw' not 'rwx' */

};

typedef struct rz_ rz_t;

rz_t *rz_init(int fd, size_t readnum, size_t bufsize, int no_timeout,
              int rxtimeout, int znulls, int eflag, int zctlesc,
              int under_rsh, int restricted, char lzmanag,
              unsigned long min_bps, long min_bps_time,
              time_t stop_time, int try_resume,
              int rxclob,
              int o_sync
              );
static int rz_zmodem_session_startup (rz_t *rz);
static void rz_checkpath (rz_t *rz, const char *name);
static int rz_write_string_to_file (rz_t *rz, struct zm_fileinfo *zi, char *buf, size_t n);
static int rz_process_header (rz_t *rz, char *name, struct zm_fileinfo *);
static int rz_receive (rz_t *rz);
static int rz_receive_file (rz_t *rz, struct zm_fileinfo *);
static int rz_closeit (rz_t *rz, struct zm_fileinfo *);

rz_t* rz_init(int fd, size_t readnum, size_t bufsize, int no_timeout,
              int rxtimeout, int znulls, int eflag, int zctlesc,
              int under_rsh, int restricted, char lzmanag,
              unsigned long min_bps, long min_bps_time,
              time_t stop_time, int try_resume,
              int rxclob,
              int o_sync
              )
{
    rz_t *rz = (rz_t *)malloc(sizeof(rz_t));
    memset (rz, 0, sizeof(rz_t));
    rz->zm = zm_init(fd, readnum, bufsize, no_timeout,
                     rxtimeout, znulls, eflag, zctlesc);
    rz->under_rsh = under_rsh;
    rz->restricted = restricted;
    rz->lzmanag = lzmanag;
    rz->zconv = 0;
    rz->min_bps = min_bps;
    rz->min_bps_time = min_bps_time;
    rz->stop_time = stop_time;
    rz->try_resume = try_resume;
    rz->rxclob = rxclob;
    rz->o_sync = o_sync;
    rz->pathname = NULL;
    rz->fout = NULL;
    rz->errors = 0;
    rz->tryzhdrtype=ZRINIT;
    rz->rxclob = FALSE;
    return rz;

}

/*
 * Let's receive something already.
 */

static size_t zmodem_receive()
{
    int fd_tty=-1;
    init_uart(&fd_tty,"/dev/ttyUSB1");
    if(fd_tty==-1)
    {
        log_error("init_uart error =%d",fd_tty);
        return -1;
    }
    rz_t *rz = rz_init(fd_tty, /* fd */
                       8192, /* readnum */
                       16384, /* bufsize */
                       1, /* no_timeout */
                       100,		 /* rxtimeout */
                       0,		 /* znulls */
                       0,		 /* eflag */
                       0,		 /* zctlesc */
                       0,		 /* under_rsh */
                       1,		 /* restricted */
                       0,		 /* lzmanag */
                       0,		 /* min_bps */
                       120,		 /* min_bps_tim */
                       0,		 /* stop_time */
                       0,		 /* try_resume */
                       0,		 /* rxclob */
                       0/* o_sync */
                       );

    if (rz_receive(rz)==ERROR) {
        zreadline_canit(rz->zm->zr);
        log_info("Transfer incomplete");
    }
    else
        log_info("Transfer complete");

    return 0u;
}


static int rz_receive(rz_t *rz)
{
    int c;
    struct zm_fileinfo zi;
    zi.fname=NULL;
    zi.modtime=0;
    zi.mode=0;
    zi.bytes_total=0;
    zi.bytes_sent=0;
    zi.bytes_received=0;
    zi.bytes_skipped=0;
    zi.eof_seen=0;
    c = 0;
    c = rz_zmodem_session_startup(rz);
    if (c != 0) {
        if (c == ZCOMPL)
            return OK;
        if (c == ERROR)
            goto fubar;
        timing(1,NULL);
        c = rz_receive_file(rz, &zi);
        switch (c) {
        case ZEOF:
        {
            double d;
            long bps;
            d=timing(0,NULL);
            if (d==0)
                d=0.5; /* can happen if timing uses time() */
            bps=(zi.bytes_received-zi.bytes_skipped)/d;
            log_info("Bytes received: %7ld/%7ld   BPS:%-6ld",
                     (long) zi.bytes_received, (long) zi.bytes_total, bps);
        }
            /* FALL THROUGH */
        case ZSKIP:
            if (c==ZSKIP)
            {
                log_info("Skipped");
            }
            switch (rz_zmodem_session_startup(rz)) {
            case ZCOMPL:
                return OK;
            default:
                return ERROR;
            case ZFILE:
                break;
            }
        default: break;
        case ERROR:
            return ERROR;
        }
        if (c) goto fubar;
    }
    return OK;
fubar:
    zreadline_canit(rz->zm->zr);
    if (rz->fout)
        fclose(rz->fout);

    if (rz->restricted && rz->pathname) {
        unlink(rz->pathname);

    }
    return ERROR;
}


/*
 * Process incoming file information header
 */
static int rz_process_header(rz_t *rz, char *name, struct zm_fileinfo *zi)
{
    const char *openmode;
    static char *name_static=NULL;
    char *nameend;

    if (name_static)
        free(name_static);
    name_static=malloc(strlen(name)+1);
    if (!name_static) {
        log_fatal("out of memory");
        exit(1);
    }
    strcpy(name_static,name);
    zi->fname=name_static;

    log_debug("zmanag=%d, Lzmanag=%d", rz->zmanag, rz->lzmanag);
    log_debug("zconv=%d",rz->zconv);

    /* set default parameters and overrides */
    openmode = "w";
    rz->thisbinary = TRUE;
    if (rz->lzmanag)
        rz->zmanag = rz->lzmanag;

    /*
     *  Process ZMODEM remote file management requests
     */
    if (rz->zconv == ZCNL)	/* Remote ASCII override */
        rz->thisbinary = 0;
    if (rz->zconv == ZCBIN)	/* Remote Binary override */
        rz->thisbinary = TRUE;
    if (rz->zconv == ZCBIN && rz->try_resume)
        rz->zconv=ZCRESUM;
    if (rz->zmanag == ZF1_ZMAPND && rz->zconv!=ZCRESUM)
        openmode = "a";

    zi->bytes_total = DEFBYTL;
    zi->mode = 0;
    zi->eof_seen = 0;
    zi->modtime = 0;

    nameend = name + 1 + strlen(name);
    if (*nameend) {	/* file coming from Unix or DOS system */
        long modtime;
        long bytes_total;
        int mode;
        sscanf(nameend, "%ld%lo%o", &bytes_total, &modtime, &mode);
        zi->modtime=modtime;
        zi->bytes_total=bytes_total;
        zi->mode=mode;
        if (zi->mode & UNIXFILE)
            ++rz->thisbinary;
    }

    if (rz->pathname)
        free(rz->pathname);
    rz->pathname=malloc((PATH_MAX)*2);
    if (!rz->pathname) {
        log_fatal("out of memory");
        exit(1);
    }
    strcpy(rz->pathname, name_static);
    /* overwrite the "waiting to receive" line */
    log_info("Receiving: %s", name_static);
    rz_checkpath(rz, name_static);
    rz->fout = fopen(name_static, openmode);
    if (!rz->fout)
    {
        log_error("cannot open %s: %s", name_static, strerror(errno));
        return ERROR;
    }
    if (rz->o_sync) {
        int oldflags;
        oldflags = fcntl (fileno(rz->fout), F_GETFD, 0);
        if (oldflags>=0 && !(oldflags & O_SYNC)) {
            oldflags|=O_SYNC;
            fcntl (fileno(rz->fout), F_SETFD, oldflags); /* errors don't matter */
        }
    }
    zi->bytes_received=zi->bytes_skipped;

    if (name_static)
        free(name_static);
    return OK;
}


/*
 * Rz_Write_String_To_File writes the n characters of buf to receive file fout.
 *  If not in binary mode, carriage returns, and all characters
 *  starting with CPMEOF are discarded.
 */
static int rz_write_string_to_file(rz_t *rz, struct zm_fileinfo *zi, char *buf, size_t n)
{
    char *p;

    if (n == 0)
        return OK;
    if (rz->thisbinary) {
        if (fwrite(buf,n,1,rz->fout)!=1)
            return ERROR;
    }
    else {
        if (zi->eof_seen)
            return OK;
        for (p=buf; n>0; ++p,n-- ) {
            if ( *p == '\r')
                continue;
            if (*p == CPMEOF) {
                zi->eof_seen=TRUE;
                return OK;
            }
            putc(*p ,rz->fout);
        }
    }
    return OK;
}


/*
 * Totalitarian Communist pathname processing
 */
static void rz_checkpath(rz_t *rz, const char *name)
{
    if (rz->restricted) {
        const char *p;
        p=strrchr(name,'/');
        if (p)
            p++;
        else
            p=name;
        /* don't overwrite any file in very restricted mode.
         * don't overwrite hidden files in restricted mode */
        if ((rz->restricted==2 || *name=='.') && fopen(name, "r") != NULL) {
            zreadline_canit(rz->zm->zr);
        }
        /* restrict pathnames to current tree or uucppublic */
        if ( strstr(name, "../")
     #ifdef PUBDIR
             || (name[0]== '/' && strncmp(name, PUBDIR,
                                          strlen(PUBDIR)))
     #endif
             ) {
            zreadline_canit(rz->zm->zr);
        }
        if (rz->restricted > 1) {
            if (name[0]=='.' || strstr(name,"/.")) {
                zreadline_canit(rz->zm->zr);
            }
        }
    }
}

/*
 * Initialize for Zmodem receive attempt, try to activate Zmodem sender
 *  Handles ZSINIT frame
 *  Return ZFILE if Zmodem filename received, -1 on error,
 *   ZCOMPL if transaction finished,  else 0
 */
static int rz_zmodem_session_startup(rz_t *rz)
{
    int c, n;
    int zrqinits_received=0;
    size_t bytes_in_block=0;

    /* Spec 8.1: "When the ZMODEM receive program starts, it
       immediately sends a ZRINIT header to initiate ZMODEM file
       transfers...  The receive program resends its header at
       intervals for a suitable period of time (40 seconds
       total)...."

       On startup rz->tryzhdrtype is, by default, set to ZRINIT
    */

    for (n=rz->zm->zmodem_requested?15:5;
         (--n + zrqinits_received) >=0 && zrqinits_received<10; ) {
        /* Set buffer length (0) and capability flags */

        /* We're going to snd a ZRINIT packet. */
        zm_set_header_payload_bytes(rz->zm,
                            #ifdef CANBREAK
                                    (rz->zm->zctlesc ?
                                         (CANFC32|CANFDX|CANOVIO|CANBRK|TESCCTL)
                                       : (CANFC32|CANFDX|CANOVIO|CANBRK)),
                            #else
                                    (rz->zm->zctlesc ?
                                         (CANFC32|CANFDX|CANOVIO|TESCCTL)
                                       : (CANFC32|CANFDX|CANOVIO)),
                            #endif
                                    0, 0, 0);
        zm_send_hex_header(rz->zm, rz->tryzhdrtype);

        if (rz->tryzhdrtype == ZSKIP)	/* Don't skip too far */
            rz->tryzhdrtype = ZRINIT;	/* CAF 8-21-87 */
again:
        switch (zm_get_header(rz->zm, NULL)) {
        case ZRQINIT:

            /* Spec 8.1: "[after sending ZRINIT] if the
             * receiving program receives a ZRQINIT
             * header, it resends the ZRINIT header." */

            /* getting one ZRQINIT is totally ok. Normally a ZFILE follows
             * (and might be in our buffer, so don't flush it). But if we
             * get more ZRQINITs than the sender has started up before us
             * and sent ZRQINITs while waiting.
             */
            zrqinits_received++;
            continue;
        case ZEOF:
            continue;
        case TIMEOUT:
            continue;
        case ZFILE:
            rz->zconv = rz->zm->Rxhdr[ZF0];
            if (!rz->zconv)
                /* resume with sz -r is impossible (at least with unix sz)
                 * if this is not set */
                rz->zconv=ZCBIN;

            rz->zmanag = rz->zm->Rxhdr[ZF1];
            rz->ztrans = rz->zm->Rxhdr[ZF2];
            rz->tryzhdrtype = ZRINIT;
            c = zm_receive_data(rz->zm, rz->secbuf, MAX_BLOCK,&bytes_in_block);
            if (c == GOTCRCW)
                return ZFILE;
            zm_send_hex_header(rz->zm, ZNAK);
            goto again;
        case ZSINIT:
            /* Spec 8.1: "[after receiving the ZRINIT]
             * then sender may then send an optional
             * ZSINIT frame to define the receiving
             * program's Attn sequence, or to specify
             * complete control character escaping.  If
             * the ZSINIT header specified ESCCTL or ESC8,
             * a HEX header is used, and the receiver
             * activates the specified ESC modes before
             * reading the following data subpacket.  */

            /* this once was:
             * Zctlesc = TESCCTL & zm->Rxhdr[ZF0];
             * trouble: if rz get --escape flag:
             * - it sends TESCCTL to sz,
             *   get a ZSINIT _without_ TESCCTL (yeah - sender didn't know),
             *   overwrites Zctlesc flag ...
             * - sender receives TESCCTL and uses "|=..."
             * so: sz escapes, but rz doesn't unescape ... not good.
             */
            rz->zm->zctlesc |= (TESCCTL & rz->zm->Rxhdr[ZF0]);
            if (zm_receive_data(rz->zm, rz->attn, ZATTNLEN, &bytes_in_block) == GOTCRCW) {
                /* Spec 8.1: "[after receiving a
                 * ZSINIT] the receiver sends a ZACK
                 * header in response, containing
                 * either the serial number of the
                 * receiving program, or 0." */
                zm_set_header_payload(rz->zm, 1L);
                zm_send_hex_header(rz->zm, ZACK);
                goto again;
            }
            zm_send_hex_header(rz->zm, ZNAK);
            goto again;
        case ZFREECNT:
            zm_set_header_payload(rz->zm, (uint32_t)(~0L));
            zm_send_hex_header(rz->zm, ZACK);
            goto again;
        case ZCOMPL:
            goto again;
        default:
            continue;
        case ZFIN:
            zm_ackbibi(rz->zm);
            return ZCOMPL;
        case ZRINIT:
            /* Spec 8.1: "If [after sending ZRINIT] the
               receiving program receives a ZRINIT header,
               it is an echo indicating that the sending
               program is not operational."  */
            log_info("got ZRINIT");
            return ERROR;
        case ZCAN:
            log_info("got ZCAN");
            return ERROR;
        }
    }
    return 0;
}

/* "OOSB" means Out Of Sync Block. I once thought that if sz sents
 * blocks a,b,c,d, of which a is ok, b fails, we might want to save
 * c and d. But, alas, i never saw c and d.
 */
typedef struct oosb_t {
    size_t pos;
    size_t len;
    char *data;
    struct oosb_t *next;
} oosb_t;
oosb_t *anker=NULL;

/*
 * Receive a file with ZMODEM protocol
 *  Assumes file name frame is in rz->secbuf
 */
static int rz_receive_file(rz_t *rz, struct zm_fileinfo *zi)
{
    int c, n;
    long last_rxbytes=0;
    unsigned long last_bps=0;
    long not_printed=0;
    time_t low_bps=0;
    size_t bytes_in_block=0;
    zi->eof_seen=FALSE;
    n = 20;

    if (rz_process_header(rz, rz->secbuf,zi) == ERROR) {
        return (rz->tryzhdrtype = ZSKIP);
    }

    for (;;) {
        zm_set_header_payload(rz->zm, zi->bytes_received);
        zm_send_hex_header(rz->zm, ZRPOS);
        goto skip_oosb;
nxthdr:
        if (anker) {
            oosb_t *akt,*last,*next;
            for (akt=anker,last=NULL;akt;last= akt ? akt : last ,akt=next) {
                if (akt->pos==zi->bytes_received) {
                    rz_write_string_to_file(rz, zi, akt->data, akt->len);
                    zi->bytes_received += akt->len;
                    log_debug("using saved out-of-sync-paket %lx, len %ld",
                              akt->pos,akt->len);
                    goto nxthdr;
                }
                next=akt->next;
                if (akt->pos<zi->bytes_received) {
                    log_debug("removing unneeded saved out-of-sync-paket %lx, len %ld",
                              akt->pos,akt->len);
                    if (last)
                        last->next=akt->next;
                    else
                        anker=akt->next;
                    free(akt->data);
                    free(akt);
                    akt=NULL;
                }
            }
        }
skip_oosb:
        c = zm_get_header(rz->zm, NULL);
        switch (c) {
        default:
            log_debug("rz_receive_file: zm_get_header returned %d", c);
            return ERROR;
        case ZNAK:
        case TIMEOUT:
            if ( --n < 0) {
                log_debug("rz_receive_file: zm_get_header returned %d", c);
                return ERROR;
            }
        case ZFILE:
            zm_receive_data(rz->zm, rz->secbuf, MAX_BLOCK,&bytes_in_block);
            continue;
        case ZEOF:
            if (zm_reclaim_receive_header(rz->zm) != (long) zi->bytes_received) {
                /*
                 * Ignore eof if it's at wrong place - force
                 *  a timeout because the eof might have gone
                 *  out before we sent our zrpos.
                 */
                rz->errors = 0;
                goto nxthdr;
            }
            if (rz_closeit(rz, zi)) {
                rz->tryzhdrtype = ZFERR;
                log_debug("rz_receive_file: rz_closeit returned <> 0");
                return ERROR;
            }
            log_debug("rz_receive_file: normal EOF");
            return c;
        case ERROR:	/* Too much garbage in header search error */
            if ( --n < 0) {
                log_debug("rz_receive_file: zm_get_header returned %d", c);
                return ERROR;
            }
            continue;
        case ZSKIP:
            rz_closeit(rz, zi);
            log_debug("rz_receive_file: Sender SKIPPED file");
            return c;
        case ZDATA:
            if (zm_reclaim_receive_header(rz->zm) != (long) zi->bytes_received) {
                oosb_t *neu;
                size_t pos=zm_reclaim_receive_header(rz->zm);
                if ( --n < 0) {
                    log_debug("rz_receive_file: out of sync");
                    return ERROR;
                }
                switch (c = zm_receive_data(rz->zm, rz->secbuf, MAX_BLOCK,&bytes_in_block))
                {
                case GOTCRCW:
                case GOTCRCG:
                case GOTCRCE:
                case GOTCRCQ:
                    if (pos>zi->bytes_received) {
                        neu=malloc(sizeof(oosb_t));
                        if (neu)
                            neu->data=malloc(bytes_in_block);
                        if (neu && neu->data) {
                            log_debug("saving out-of-sync-block %lx, len %lu",pos,
                                      (unsigned long) bytes_in_block);
                            memcpy(neu->data,rz->secbuf,bytes_in_block);
                            neu->pos=pos;
                            neu->len=bytes_in_block;
                            neu->next=anker;
                            anker=neu;
                        }
                        else if (neu)
                            free(neu);
                    }
                }
                continue;
            }
moredata:
        {
            int minleft =  0;
            int secleft =  0;
            time_t now;
            double d;
            d=timing(0,&now);
            if (d==0) d=0.5; /* timing() might use time() */
            last_bps=zi->bytes_received/d;
            if (last_bps > 0) {
                minleft =  (R_BYTESLEFT(zi))/last_bps/60;
                secleft =  ((R_BYTESLEFT(zi))/last_bps)%60;
            }
            if (rz->min_bps) {
                if (low_bps) {
                    if (last_bps < rz->min_bps) {
                        if (now-low_bps >= rz->min_bps_time) {
                            /* too bad */
                            log_debug("rz_receive_file: bps rate %ld below min %ld",
                                      last_bps, rz->min_bps);
                            return ERROR;
                        }
                    }
                    else
                        low_bps=0;
                } else if (last_bps< rz->min_bps) {
                    low_bps=now;
                }
            }
            if (rz->stop_time && now >= rz->stop_time) {
                /* too bad */
                log_debug("rz_receive_file: reached stop time");
                return ERROR;
            }

            log_info("\rBytes received: %7ld/%7ld   BPS:%-6ld ETA %02d:%02d  ",
                     (long) zi->bytes_received, (long) zi->bytes_total,
                     last_bps, minleft, secleft);

            last_rxbytes=zi->bytes_received;
            not_printed=0;
        }
            switch (c = zm_receive_data(rz->zm, rz->secbuf, MAX_BLOCK,&bytes_in_block))
            {
            case ZCAN:
                log_debug("rz_receive_file: zm_receive_data returned %d", c);
                return ERROR;
            case ERROR:	/* CRC error */
                if ( --n < 0) {
                    log_debug("rz_receive_file: zm_get_header returned %d", c);
                    return ERROR;
                }
                continue;
            case TIMEOUT:
                if ( --n < 0) {
                    log_debug("rz_receive_file: zm_get_header returned %d", c);
                    return ERROR;
                }
                continue;
            case GOTCRCW:
                n = 20;
                rz_write_string_to_file(rz, zi, rz->secbuf, bytes_in_block);
                zi->bytes_received += bytes_in_block;
                zm_set_header_payload(rz->zm, zi->bytes_received);
                zm_send_hex_header(rz->zm, ZACK | 0x80);
                goto nxthdr;
            case GOTCRCQ:
                n = 20;
                rz_write_string_to_file(rz, zi, rz->secbuf, bytes_in_block);
                zi->bytes_received += bytes_in_block;
                zm_set_header_payload(rz->zm, zi->bytes_received);
                zm_send_hex_header(rz->zm, ZACK);
                goto moredata;
            case GOTCRCG:
                n = 20;
                rz_write_string_to_file(rz, zi, rz->secbuf, bytes_in_block);
                zi->bytes_received += bytes_in_block;
                goto moredata;
            case GOTCRCE:
                n = 20;
                rz_write_string_to_file(rz, zi, rz->secbuf, bytes_in_block);
                zi->bytes_received += bytes_in_block;
                goto nxthdr;
            }
        }
    }
}

/*
 * Close the receive dataset, return OK or ERROR
 */
static int rz_closeit(rz_t *rz, struct zm_fileinfo *zi)
{
    int ret;
    ret=fclose(rz->fout);
    if (ret) {
        log_error("file close error: %s", strerror(errno));
        /* this may be any sort of error, including random data corruption */

        unlink(rz->pathname);
        return ERROR;
    }
    if (zi->modtime) {
        struct utimbuf timep;
        timep.actime = time(NULL);
        timep.modtime = zi->modtime;
        utime(rz->pathname, &timep);
    }
    if (S_ISREG(zi->mode)) {
        /* we must not make this program executable if running
         * under rsh, because the user might have uploaded an
         * unrestricted shell.
         */
        if (rz->under_rsh)
            chmod(rz->pathname, (00666 & zi->mode));
        else
            chmod(rz->pathname, (07777 & zi->mode));
    }
    return OK;
}


#ifndef SZ
int main(int argc, char *argv[])
{
    system("rm -rf 123.exe");
    return zmodem_receive();
}
#endif
/* End of lrz.c */
