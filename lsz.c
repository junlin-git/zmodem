#define SS_NORMAL 0
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

#include <sys/mman.h>

#include "zm.h"
#include "crctab.h"
#include "zm.h"
#include "uart.h"

#define MAX_BLOCK 8192

typedef struct sz_ {
    zm_t *zm;		/* zmodem comm primitives' state */
    // state
    char txbuf[MAX_BLOCK];
    FILE *input_f;
    size_t lastsync;		/* Last offset to which we got a ZRPOS */
    size_t bytcnt;
    char crcflg;
    int firstsec;
    unsigned txwspac;	/* Spacing between zcrcq requests */
    unsigned txwcnt;	/* Counter used to space ack requests */
    size_t lrxpos;		/* Receiver's last reported offset */
    int errors;
    int under_rsh;
    char lastrx;
    long totalleft;
    int canseek; /* 1: can; 0: only rewind, -1: neither */
    size_t blklen;		/* length of transmitted records */
    int totsecs;		/* total number of sectors this file */
    int filcnt;		/* count of number of files opened */
    int lfseen;
    unsigned blkopt;		/* Override value for zmodem blklen */
    int zrqinits_sent;
    int rxflags;
    int rxflags2;
    int exitcode;
    time_t stop_time;
    int error_count;
    jmp_buf intrjmp;	/* For the interrupt on RX CAN */
    // parameters
    int lskipnocor;
    int no_unixmode;
    int filesleft;
    int restricted;
    int errcnt;		/* number of files unreadable */
    int optiong;		/* Let it rip no wait for sector ACK's */
    unsigned rxbuflen;	/* Receiver's max buffer length */
    unsigned tframlen;
    int wantfcs32;	/* want to send 32 bit FCS */
    size_t max_blklen;
    int hyperterm;

    void (*complete_cb)(const char *filename, int result, size_t size, time_t date);

} sz_t;


static sz_t* sz_init(int fd, size_t readnum, size_t bufsize, int no_timeout,
                     int rxtimeout, int znulls, int eflag, int zctlesc, int zrwindow,
                     unsigned txwspac,
                     int under_rsh, int no_unixmode, int restricted,
                     unsigned blkopt, int wantfcs32,
                     size_t max_blklen, time_t stop_time,
                     int hyperterm,
                     complete_call complete_cb
                     )
{
    sz_t *sz = malloc(sizeof(sz_t));
    memset(sz, 0, sizeof(sz_t));
    sz->zm = zm_init(fd, readnum, bufsize, no_timeout,
                     rxtimeout, znulls, eflag, zctlesc, zrwindow);
    sz->txwspac = txwspac;
    sz->txwcnt = 0;
    sz->under_rsh = under_rsh;
    sz->no_unixmode = no_unixmode;
    sz->filesleft = 0;
    sz->restricted = restricted;
    sz->rxbuflen = 16384;
    sz->blkopt = blkopt;
    sz->wantfcs32 = wantfcs32;
    sz->max_blklen = max_blklen;
    sz->stop_time = stop_time;
    sz->hyperterm = hyperterm;
    sz->complete_cb = complete_cb;
    return sz;
}

static int sz_transmit_file_by_zmodem (sz_t *sz, struct zm_fileinfo *zi, const char *buf, size_t blen);
static int sz_getnak (sz_t *sz);
static int sz_transmit_pathname (sz_t *sz, struct zm_fileinfo *);
static int sz_transmit_file (sz_t *sz, const char *oname);
static int sz_zfilbuf (sz_t *sz, struct zm_fileinfo *zi);
static int sz_getzrxinit (sz_t *sz);
static int sz_sendzsinit (sz_t *sz);
static int sz_transmit_file_contents_by_zmodem (sz_t *sz, struct zm_fileinfo *);
static int sz_getinsync (sz_t *sz, struct zm_fileinfo *, int flag);
static int sz_transmit_sector (sz_t *sz, char *buf, int sectnum, size_t cseclen);

#define ZM_SEND_DATA(x,y,z)						\
    do { if (sz->zm->crc32t) {zm_send_data32(sz->zm,x,y,z); } else {zm_send_data(sz->zm,x,y,z);}} while(0)
#define DATAADR (sz->txbuf)

/*
 * Attention string to be executed by receiver to interrupt streaming data
 *  when an error is detected.  A pause (0336) may be needed before the
 *  ^C (03) or after it.
 */
static char Myattn[] = { 0 };


#define OVERHEAD 18
#define OVER_ERR 20

#define MK_STRING(x) #x

static size_t zmodem_send(char *file_list,complete_call complete)
{
    int fd_tty=-1;
    init_uart(&fd_tty,"/dev/ttyUSB1");
    sz_t *sz = sz_init(fd_tty, /* fd */
                       128, /* readnum */
                       256, /* bufsize */
                       1, /* no_timeout */
                       600,	/* rxtimeout */
                       0, 	/* znulls */
                       0,	/* eflag */
                       0,	 /* zctlesc */
                       1400, /* zrwindow */
                       0,	  /* txwspac */
                       0,	  /* under_rsh */
                       0,	  /* no_unixmode */
                       0,	  /* restricted */
                       0,	  /* blkopt */
                       0,	  /* wantfcs32 */
                       1024,  /* max_blklen */
                       0,	  /* stop_time */
                       0,	  /* hyperterm */
                       complete  /* file complete callback */
                       );
    log_info("initial protocol is ZMODEM");
    zm_set_header_payload(sz->zm, 0L);
    zm_send_hex_header(sz->zm, ZRQINIT);//ZRQINIT 要求初始化
    //ZRINIT 回应接收能力(rz的实现是在空闲状态每过几秒就发送一次这个指令，多次无响应后退出)
    sz->exitcode=0;
    /* This is the main loop.  */
    if (sz_transmit_file(sz, file_list)==ERROR) {
        sz->exitcode=0200;
        zreadline_canit(sz->zm->zr);
    }
    if (sz->exitcode==0)
        log_info("Transfer complete");
    else
        log_info("Transfer incomplete");

    return sz->exitcode;
}


/* This routine should send one file from a list of files.  The
 filename is ONAME. REMOTENAME can be NULL. */
static int sz_transmit_file(sz_t *sz, const char *filename)
{
    struct stat f;
    char name[PATH_MAX+1];
    struct zm_fileinfo zi;

    if ((sz->input_f=fopen(filename, "r"))==NULL) {
        int e=errno;
        log_error("cannot open %s: %s", filename, strerror(e));
        ++sz->errcnt;
        return OK;	/* pass over it, there may be others */
    } else {
        strcpy(name, filename);
    }
    /* Check for directory or block special files */
    fstat(fileno(sz->input_f), &f);
    if (S_ISDIR(f.st_mode) || S_ISBLK(f.st_mode)) {
        log_error("is not a file: %s", name);
        fclose(sz->input_f);
        return OK;
    }
    /* Here we finally start filling in information about the
         * file in a ZI structure.  We need this for the ZMODEM
     * file header when we send it. */
    zi.fname=name;
    zi.modtime=f.st_mtime;
    zi.mode=f.st_mode;
    zi.bytes_total= f.st_size;
    zi.bytes_sent=0;
    zi.bytes_received=0;
    zi.bytes_skipped=0;
    zi.eof_seen=0;

    ++sz->filcnt;
    log_debug("st_mtime=%d st_size=%d",f.st_mtime,f.st_size);
    /* Now that the file information is validated and is in a ZI
     * structure, we try to transmit the file. */
    switch (sz_transmit_pathname(sz, &zi)) {
    case ERROR:
        return ERROR;
    case ZSKIP:
        log_error("skipped: %s", name);
        return OK;
    }

    /* Here we make a log message the transmission of a single
     * file. */
    long bps;
    double d=timing(0,NULL);
    if (d==0) /* can happen if timing() uses time() */
        d=0.5;
    bps=zi.bytes_sent/d;
    log_debug("Bytes Sent:%7ld   BPS:%-8ld",
              (long) zi.bytes_sent,bps);
    if (sz->complete_cb)
        sz->complete_cb(zi.fname, 0, zi.bytes_sent, zi.modtime);

    zm_saybibi(sz->zm);
    return 0;
}

/*
 * generate and transmit pathname block consisting of
 *  pathname (null terminated),
 *  file length, mode time and file mode in octal
 *  as provided by the Unix fstat call.
 *  N.B.: modifies the passed name, may extend it!
 */
static int sz_transmit_pathname(sz_t *sz, struct zm_fileinfo *zi)
{
    char *p, *q;
    struct stat f;

    /* The sz_getnak process is how the sender knows which protocol
     * is it allowed to use.  Hopefully the receiver allows
     * ZModem.  If it doesn't, we may fall back to YModem. */
    if (!sz->zm->zmodem_requested)
        if (sz_getnak(sz)) {
            log_debug("sz_getnak failed");
            return ERROR;
        }

    q = (char *) 0;


    for (p=zi->fname, q=sz->txbuf ; *p; )
        if ((*q++ = *p++) == '/')
            q = sz->txbuf;
    *q++ = 0;
    p=q;
    while (q < (sz->txbuf + MAX_BLOCK))
        *q++ = 0;
    if ((sz->input_f!=stdin) && *zi->fname && (fstat(fileno(sz->input_f), &f)!= -1)) {
        if (sz->hyperterm) {
            sprintf(p, "%lu", (long) f.st_size);
        } else {
            /* note that we may lose some information here
             * in case mode_t is wider than an int. But i believe
             * sending %lo instead of %o _could_ break compatability
             */

            /* Spec 8.2: "[the sender will send a] ZCRCW
             * data subpacket containing the file name,
             * file length, modification date, and other
             * information identical to that used by
             * YMODEM batch." */
            sprintf(p, "%lu %lo %o 0 %d %ld", (long) f.st_size,
                    f.st_mtime,
                    (unsigned int)((sz->no_unixmode) ? 0 : f.st_mode),
                    sz->filesleft, sz->totalleft);
        }
    }
    log_info("Sending: %s  %s",sz->txbuf,p);
    sz->totalleft -= f.st_size;
    if (--sz->filesleft <= 0)
        sz->totalleft = 0;
    if (sz->totalleft < 0)
        sz->totalleft = 0;

    /* force 1k blocks if name won't fit in 128 byte block */
    if (sz->txbuf[125])
        sz->blklen=1024;
    else {		/* A little goodie for IMP/KMD */
        sz->txbuf[127] = (f.st_size + 127) >>7;
        sz->txbuf[126] = (f.st_size + 127) >>15;
    }

    /* We'll send the file by ZModem, if the sz_getnak process succeeded.  */
    if (sz->zm->zmodem_requested)
        return sz_transmit_file_by_zmodem(sz, zi, sz->txbuf, 1+strlen(p)+(p-sz->txbuf));

    /* We'll have to send the file by YModem, I guess.  */
    if (sz_transmit_sector(sz, sz->txbuf, 0, 128)==ERROR) {
        log_debug("sz_transmit_sector failed");
        return ERROR;
    }
    return OK;
}


/* [mlg] Somewhere in this logic, this procedure tries to force the receiver
 * into ZModem mode? */
static int sz_getnak(sz_t *sz)
{
    int firstch;
    int tries=0;

    sz->lastrx = 0;
    for (;;) {
        tries++;
        switch (firstch = zreadline_getc(sz->zm->zr, 100)) {
        case ZPAD:
            /* Spec 7.3.1: "A binary header begins with
             * the sequence ZPAD, ZDLE, ZBIN. */
            /* Spec 7.3.3: "A hex header begins with the
             * sequence ZPAD ZPAD ZDLE ZHEX." */
            if (sz_getzrxinit(sz))return ERROR;
            return FALSE;
        case TIMEOUT:
            /* 30 seconds are enough */
            if (tries==3) {
                log_error( "Timeout on pathname");
                return TRUE;
            }
            /* don't send a second ZRQINIT _directly_ after the
             * first one. Never send more then 4 ZRQINIT, because
             * omen rz stops if it saw 5 of them */
            if ((sz->zrqinits_sent>1 || tries>1)
                    && sz->zrqinits_sent<4) {
                /* if we already sent a ZRQINIT we are
                 * using zmodem protocol and may send
                 * further ZRQINITs
                 */
                zm_set_header_payload(sz->zm, 0L);
                zm_send_hex_header(sz->zm, ZRQINIT);
                sz->zrqinits_sent++;
            }
            continue;
        case WANTG:
            /* Set cbreak, XON/XOFF, etc. */
            sz->optiong = TRUE;
            sz->blklen=1024;
        case WANTCRC:
            sz->crcflg = TRUE;
        case NAK:
            /* Spec 8.1: "The sending program awaits a
             * command from the receiving port to start
             * file transfers.  If a 'C', 'G', or NAK is
             * received, and XMODEM or YMODEM file
             * transfer is indicated.  */
            return FALSE;
        case CAN:
            if ((firstch = zreadline_getc(sz->zm->zr, 20)) == CAN
                    && sz->lastrx == CAN)
                return TRUE;

        default:
            log_info("%x ",firstch);
            break;
        }
        sz->lastrx = firstch;
    }
}

static int sz_transmit_sector(sz_t *sz, char *buf, int sectnum, size_t cseclen)
{
    int checksum, wcj;
    char *cp;
    unsigned oldcrc;
    int firstch;
    int attempts;

    firstch=0;	/* part of logic to detect CAN CAN */

    log_debug("Zmodem sectors/kbytes sent: %3d/%2dk", sz->totsecs, sz->totsecs/8 );
    for (attempts=0; attempts <= RETRYMAX; attempts++) {
        sz->lastrx= firstch;
        zreadline_write(cseclen==1024?STX:SOH);
        zreadline_write(sectnum & 0xFF);
        /* FIXME: clarify the following line - mlg */
        zreadline_write((-sectnum -1) & 0xFF);
        oldcrc=checksum=0;
        for (wcj=cseclen,cp=buf; --wcj>=0; ) {
            zreadline_write(*cp);
            oldcrc=updcrc((0377& *cp), oldcrc);
            checksum += *cp++;
        }
        if (sz->crcflg) {
            oldcrc=updcrc(0,updcrc(0,oldcrc));
            zreadline_write(((int)oldcrc>>8) & 0xFF);
            zreadline_write(((int)oldcrc) & 0xFF);
        }
        else
            zreadline_write(checksum & 0xFF);


        if (sz->optiong) {
            sz->firstsec = FALSE; return OK;
        }
        firstch = zreadline_getc(sz->zm->zr, sz->zm->rxtimeout);
gotnak:
        switch (firstch) {
        case CAN:
            if(sz->lastrx == CAN) {
cancan:
                log_error( "Cancelled");  return ERROR;
            }
            break;
        case TIMEOUT:
            log_error( "Timeout on sector ACK"); continue;
        case WANTCRC:
            if (sz->firstsec)
                sz->crcflg = TRUE;
        case NAK:
            log_error( "NAK on sector"); continue;
        case ACK:
            sz->firstsec=FALSE;
            sz->totsecs += (cseclen>>7);
            return OK;
        case ERROR:
            log_error( "Got burst for sector ACK"); break;
        default:
            log_error( "Got %02x for sector ACK", firstch); break;
        }
        for (;;) {
            sz->lastrx = firstch;
            if ((firstch = zreadline_getc(sz->zm->zr, sz->zm->rxtimeout)) == TIMEOUT)
                break;
            if (firstch == NAK || firstch == WANTCRC)
                goto gotnak;
            if (firstch == CAN && sz->lastrx == CAN)
                goto cancan;
        }
    }
    log_error( "Retry Count Exceeded");
    return ERROR;
}

/* Fill buffer with blklen chars */
static int sz_zfilbuf (sz_t *sz, struct zm_fileinfo *zi)
{
    size_t n;

    n = fread (sz->txbuf, 1, sz->max_blklen, sz->input_f);
    if (n <=0)
        zi->eof_seen = 1;
    return n;
}


/*
 * Get the receiver's init parameters
 */
static int sz_getzrxinit(sz_t *sz)
{
    static int dont_send_zrqinit=1;
    int old_timeout=sz->zm->rxtimeout;
    int n;
    struct stat f;
    uint32_t rxpos;
    int timeouts=0;

    sz->zm->rxtimeout=100; /* 10 seconds */

    for (n=10; --n>=0; ) {
        /* we might need to send another zrqinit in case the first is
         * lost. But *not* if getting here for the first time - in
         * this case we might just get a ZRINIT for our first ZRQINIT.
         * Never send more then 4 ZRQINIT, because
         * omen rz stops if it saw 5 of them.
         */
        if (sz->zrqinits_sent<4 && n!=10 && !dont_send_zrqinit) {
            sz->zrqinits_sent++;
            zm_set_header_payload(sz->zm, 0L);
            zm_send_hex_header(sz->zm, ZRQINIT);
        }
        dont_send_zrqinit=0;

        switch (zm_get_header(sz->zm, &rxpos)) {
        case ZCHALLENGE:	/* Echo receiver's challenge numbr */
            zm_set_header_payload(sz->zm, rxpos);
            zm_send_hex_header(sz->zm, ZACK);
            continue;
        case ZRINIT:
            sz->rxflags = 0377 & sz->zm->Rxhdr[ZF0];
            sz->rxflags2 = 0377 & sz->zm->Rxhdr[ZF1];
            sz->zm->txfcs32 = (sz->wantfcs32 && (sz->rxflags & CANFC32));
        {
            int old=sz->zm->zctlesc;
            sz->zm->zctlesc |= sz->rxflags & TESCCTL;
            /* update table - was initialised to not escape */
            if (sz->zm->zctlesc && !old)
                zm_escape_sequence_update(sz->zm);
        }
            sz->rxbuflen = (0377 & sz->zm->Rxhdr[ZP0])+((0377 & sz->zm->Rxhdr[ZP1])<<8);
            log_debug("Rxbuflen=%d Tframlen=%d", sz->rxbuflen, sz->tframlen);
            /* Override to force shorter frame length */
            if (sz->tframlen && sz->rxbuflen > sz->tframlen)
                sz->rxbuflen = sz->tframlen;
            if ( !sz->rxbuflen)
                sz->rxbuflen = 1024;
            log_debug("Rxbuflen=%d", sz->rxbuflen);

            /* If using a pipe for testing set lower buf len */
            fstat(0, &f);
            if (! (S_ISCHR(f.st_mode))) {
                sz->rxbuflen = MAX_BLOCK;
            }
            /*
             * If input is not a regular file, force ACK's to
             *  prevent running beyond the buffer limits
             */
            fstat(fileno(sz->input_f), &f);

            if (sz->rxbuflen && sz->blklen>sz->rxbuflen)
                sz->blklen = sz->rxbuflen;
            if (sz->blkopt && sz->blklen > sz->blkopt)
                sz->blklen = sz->blkopt;
            log_debug("Rxbuflen=%d blklen=%d", sz->rxbuflen, sz->blklen);
            sz->zm->rxtimeout = old_timeout;
            return (sz_sendzsinit(sz));
        case ZCAN:
        case TIMEOUT:
            if (timeouts++==0)
                continue; /* force one other ZRQINIT to be sent */
            return ERROR;
        case ZRQINIT:
            if (sz->zm->Rxhdr[ZF0] == ZCOMMAND)
                continue;
        default:
            zm_send_hex_header(sz->zm, ZNAK);
            continue;
        }
    }
    return ERROR;
}

/* Send send-init information */
static int sz_sendzsinit(sz_t *sz)
{
    int c;

    if (Myattn[0] == '\0' && (!sz->zm->zctlesc || (sz->rxflags & TESCCTL)))
        return OK;
    sz->errors = 0;
    for (;;) {
        zm_set_header_payload(sz->zm, 0L);
        if (sz->zm->zctlesc) {
            sz->zm->Txhdr[ZF0] |= TESCCTL;
            zm_send_hex_header(sz->zm, ZSINIT);
        }
        else
            zm_send_binary_header(sz->zm, ZSINIT);
        ZM_SEND_DATA(Myattn, 1+strlen(Myattn), ZCRCW);
        c = zm_get_header(sz->zm, NULL);
        switch (c) {
        case ZCAN:
            return ERROR;
        case ZACK:
            return OK;
        default:
            if (++sz->errors > 19)
                return ERROR;
            continue;
        }
    }
}

/* Send file name and related info */
static int sz_transmit_file_by_zmodem(sz_t *sz, struct zm_fileinfo *zi, const char *buf, size_t blen)
{
    int c;
    unsigned long crc;
    uint32_t rxpos;
    /* we are going to send a ZFILE. There cannot be much useful
     * stuff in the line right now (*except* ZCAN?).
     */
    for (;;) {
        /* Spec 8.2: "The sender then sends a ZFILE header
         * with ZMODEM Conversion, Management, and Transport
         * options followed by a ZCRCW data subpacket
         * containing the file name, ...." */
        sz->zm->Txhdr[ZF0] =0;// 	/* file conversion request */
        sz->zm->Txhdr[ZF1] =0; //	/* file management request */
        if (sz->lskipnocor)
            sz->zm->Txhdr[ZF1] |= ZF1_ZMSKNOLOC;
        sz->zm->Txhdr[ZF2] = 0;	/* file transport compression request */
        sz->zm->Txhdr[ZF3] = 0; /* extended options */
        zm_send_binary_header(sz->zm, ZFILE);
        ZM_SEND_DATA(buf, blen, ZCRCW);
again:
        c = zm_get_header(sz->zm, &rxpos);
        switch (c) {
        case ZRINIT:
            while ((c = zreadline_getc(sz->zm->zr, 50)) > 0)
                if (c == ZPAD) {
                    goto again;
                }
            /* **** FALL THRU TO **** */
        default:
            log_info("c=%x",c);
            continue;
        case ZRQINIT:  /* remote site is sender! */
            log_info("got ZRQINIT");
            return ERROR;
        case ZCAN:
            log_info("got ZCAN");
            return ERROR;
        case TIMEOUT:
            return ERROR;
        case ZABORT:
            return ERROR;
        case ZFIN:
            return ERROR;
        case ZCRC:
            /* Spec 8.2: "[if] the receiver has a file
             * with the same name and length, [it] may
             * respond with a ZCRC header with a byte
             * count, which requires the sender to perform
             * a 32-bit CRC on the specified number of
             * bytes in the file, and transmit the
             * complement of the CRC is an answering ZCRC
             * header." */
            crc = 0xFFFFFFFFL;
            if (sz->canseek >= 0)
            {
                if (rxpos==0) {
                    struct stat st;
                    if (0==fstat(fileno(sz->input_f),&st)) {
                        rxpos=st.st_size;
                    } else
                        rxpos=-1;
                }
                while (rxpos-- && ((c = getc(sz->input_f)) != EOF))
                    crc = UPDC32(c, crc);
                crc = ~crc;
                clearerr(sz->input_f);	/* Clear EOF */
                fseek(sz->input_f, 0L, 0);
            }
            zm_set_header_payload(sz->zm, crc);
            zm_send_binary_header(sz->zm, ZCRC);
            goto again;
        case ZSKIP:
            /* Spec 8.2: "[after deciding if the file name, file
         * size, etc are acceptable] The receiver may respond
         * with a ZSKIP header, which makes the sender proceed
         * to the next file (if any)." */
            if (sz->input_f) fclose(sz->input_f);

            log_debug("receiver skipped");
            return c;
        case ZRPOS:
            /* Spec 8.2: "A ZRPOS header from the receiver
             * initiates transmittion of the file data
             * starting at the offset in the file
             * specified by the ZRPOS header.  */
            /*
             * Suppress zcrcw request otherwise triggered by
             * lastsync==bytcnt
             */
            if (rxpos && fseek(sz->input_f, (long) rxpos, 0)) {
                int er=errno;
                log_debug("fseek failed: %s", strerror(er));
                return ERROR;
            }
            if (rxpos) zi->bytes_skipped=rxpos;
            sz->bytcnt = zi->bytes_sent = rxpos;
            sz->lastsync = rxpos -1;
            /* Spec 8.2: [in response to ZRPOS] the sender
             * sends a ZDATA binary header (with file
             * position) followed by one or more data
             * subpackets."  */
            return sz_transmit_file_contents_by_zmodem(sz, zi);
        }
    }
}

/* Send the data in the file */
static int sz_transmit_file_contents_by_zmodem (sz_t *sz, struct zm_fileinfo *zi)
{
    static int c;
    sz->lrxpos = 0;
somemore:
    /* Note that this whole next block is a
     * setjmp block for error recovery.  The
     * normal code path follows it. */
    if (setjmp (sz->intrjmp)) {
waitack:
        c = sz_getinsync (sz, zi, 0);
        switch (c) {
        default:
            if (sz->input_f)
                fclose (sz->input_f);
            return ERROR;
        case ZCAN:
            if (sz->input_f)
                fclose (sz->input_f);
            return ERROR;
        case ZSKIP:
            if (sz->input_f)
                fclose (sz->input_f);
            return c;
        case ZACK:
        case ZRPOS:
            break;
        case ZRINIT:
            return OK;
        }
    }
    sz->txwcnt = 0;
    zm_set_header_payload (sz->zm, zi->bytes_sent);
    zm_send_binary_header (sz->zm, ZDATA);
    log_trace("zm_send_binary_header");
    do {
        size_t n;
        int e;
        n = sz_zfilbuf (sz, zi);
        if(n==sz->max_blklen)
        {
            e = ZCRCW;
        }
        else {
            e = ZCRCE;
        }
        ZM_SEND_DATA (sz->txbuf, n, e);
        log_trace("%c n=%d %d max_blklen=%ld %d bytcnt=%d",
                  e,n,fileno(sz->input_f),sz->max_blklen,zi->eof_seen,sz->bytcnt);
        sz->bytcnt = zi->bytes_sent += n;
        if (e == ZCRCW)
            /* Spec 8.2: "ZCRCW data subpackets expect a
             * response before the next frame is sent." */
            goto waitack;
    } while (!zi->eof_seen);

        for (;;) {
            /* Spec 8.2: [after sending a file] The sender sends a
         * ZEOF header with the file ending offset equal to
         * the number of characters in the file. */
            log_trace("bytes_sent=%d\r\n",zi->bytes_sent);
            zm_set_header_payload (sz->zm, zi->bytes_sent);
            zm_send_binary_header (sz->zm, ZEOF);
            switch (sz_getinsync (sz, zi, 0)) {
            case ZACK:
                continue;
            case ZRPOS:
                goto somemore;
            case ZRINIT:
                /* If the receiver is satisfied with the file,
             * it returns ZRINIT. */
                return OK;
            case ZSKIP:
                if (sz->input_f)
                    fclose (sz->input_f);
                return c;
            default:
                if (sz->input_f)
                    fclose (sz->input_f);
                return ERROR;
            }
        }
}

/*
 * Respond to receiver's complaint, get back in sync with receiver
 */
static int sz_getinsync(sz_t *sz, struct zm_fileinfo *zi, int flag)
{
    int c;
    uint32_t rxpos;

    for (;;) {
        c = zm_get_header(sz->zm, &rxpos);
        switch (c) {
        case ZCAN:
        case ZABORT:
        case ZFIN:
        case TIMEOUT:
            return ERROR;
        case ZRPOS:
            /* ************************************* */
            /*  If sending to a buffered modem, you  */
            /*   might send a break at this point to */
            /*   dump the modem's buffer.		 */
            log_error("\r\n===========retry send=====\r\n");
            if (fseek(sz->input_f, (long) rxpos, 0))
                return ERROR;
            zi->eof_seen = 0;
            sz->bytcnt = sz->lrxpos = zi->bytes_sent = rxpos;
            if (sz->lastsync == rxpos) {
                sz->error_count++;
            }
            sz->lastsync = rxpos;
            return c;
        case ZACK:
            sz->lrxpos = rxpos;
            if (flag || zi->bytes_sent == rxpos)
                return ZACK;
            continue;
        case ZRINIT:
        case ZSKIP:
            if (sz->input_f)fclose(sz->input_f);
            return c;
        case ERROR:
        default:
            sz->error_count++;
            zm_send_binary_header(sz->zm, ZNAK);
            continue;
        }
    }
}



static void complete_cb(const char *filename, int result, size_t size, time_t date)
{
    if (result == RZSZ_NO_ERROR)
        fprintf(stderr, "'%s (%zu bytes)': successful send\n", filename, size);
    else
        fprintf(stderr, "'%s': failed to send\n", filename);
}

int main(int argc, char *argv[])
{
    char *filenames= "/home/junlin/opensoure/zbar-0.10.tar.bz2";
    //char *filenames= "/home/junlin/beyondcompare.sh";
    zmodem_send(filenames,&complete_cb);
    return 0;
}

/* End of lsz.c */
