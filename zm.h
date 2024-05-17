#ifndef ZM_H
#define ZM_H

#include <stdint.h>
#include <stdbool.h>
#include "zglobal.h"
#ifdef __cplusplus
extern "C" {
#endif

/*zmodem start*/

#define ZPAD '*'	/* 052 Padding character begins frames */

/* Spec 7.2: The ZDLE character is special. ZDLE represents a control
 * sequence of some sort.... The Value of ZDLE is octal 030. */
#define ZDLE 030

/* Spec 7.2: If a ZDLE character appears in binary data, it is prefixed
 * with ZDLE, then sent as ZDLEE. */
#define ZDLEE (ZDLE^0100)

#define ZBIN 'A'	/* Binary frame indicator */
#define ZHEX 'B'	/* HEX frame indicator */
#define ZBIN32 'C'	/* Binary frame with 32 bit FCS */

/* Frame types (see array "frametypes" in zm.c) */
#define ZRQINIT	0	/* Request receive init */
#define ZRINIT	1	/* Receive init */
#define ZSINIT 2	/* Send init sequence (optional) */
#define ZACK 3		/* ACK to above */
#define ZFILE 4		/* File name from sender */
#define ZSKIP 5		/* To sender: skip this file */
#define ZNAK 6		/* Last packet was garbled */
#define ZABORT 7	/* Abort batch transfers */
#define ZFIN 8		/* Finish session */
#define ZRPOS 9		/* Resume data trans at this position */
#define ZDATA 10	/* Data packet(s) follow */
#define ZEOF 11		/* End of file */
#define ZFERR 12	/* Fatal Read or Write error Detected */
#define ZCRC 13		/* Request for file CRC and response */
#define ZCHALLENGE 14	/* Receiver's Challenge */
#define ZCOMPL 15	/* Request is complete */
#define ZCAN 16		/* Other end canned session with CAN*5 */
#define ZFREECNT 17	/* Request for free bytes on filesystem */
#define ZCOMMAND 18	/* Command from sending program */
#define ZSTDERR 19	/* Output to standard error, data follows */

/* ZDLE sequences */
#define ZCRCE 'h'	/* CRC next, frame ends, header packet follows */
#define ZCRCG 'i'	/* CRC next, frame continues nonstop */
#define ZCRCQ 'j'	/* CRC next, frame continues, ZACK expected */
#define ZCRCW 'k'	/* CRC next, ZACK expected, end of frame */
#define ZRUB0 'l'	/* Translate to rubout 0177 */
#define ZRUB1 'm'	/* Translate to rubout 0377 */

/* zdlread return values (internal) */
/* -1 is general error, -2 is timeout */
#define GOTOR 0400
#define GOTCRCE (ZCRCE|GOTOR)	/* ZDLE-ZCRCE received */
#define GOTCRCG (ZCRCG|GOTOR)	/* ZDLE-ZCRCG received */
#define GOTCRCQ (ZCRCQ|GOTOR)	/* ZDLE-ZCRCQ received */
#define GOTCRCW (ZCRCW|GOTOR)	/* ZDLE-ZCRCW received */
#define GOTCAN	(GOTOR|030)	/* CAN*5 seen */

/* Byte positions within header array */
#define ZF0	3	/* First flags byte */
#define ZF1	2
#define ZF2	1
#define ZF3	0
#define ZP0	0	/* Low order 8 bits of position */
#define ZP1	1
#define ZP2	2
#define ZP3	3	/* High order 8 bits of file position */

/* Bit Masks for ZRINIT flags byte ZF0 */
#define CANFDX	0x01	/* Rx can send and receive true FDX */
#define CANOVIO	0x02	/* Rx can receive data during disk I/O */
#define CANBRK	0x04	/* Rx can send a break signal */
#define CANCRY	0x08	/* Receiver can decrypt */
#define CANLZW	0x10	/* Receiver can uncompress */
#define CANFC32	0x20	/* Receiver can use 32 bit Frame Check */
#define ESCCTL  0x40	/* Receiver expects ctl chars to be escaped */
#define ESC8    0x80	/* Receiver expects 8th bit to be escaped */
/* Bit Masks for ZRINIT flags byze ZF1 */
#define ZF1_CANVHDR  0x01  /* Variable headers OK, unused in lrzsz */

/* Parameters for ZSINIT frame */
#define ZATTNLEN 32	/* Max length of attention string */
/* Bit Masks for ZSINIT flags byte ZF0 */
#define TESCCTL 0100	/* Transmitter expects ctl chars to be escaped */
#define TESC8   0200	/* Transmitter expects 8th bit to be escaped */

/* Parameters for ZFILE frame */
/* Conversion options one of these in ZF0 */
#define ZCBIN	1	/* Binary transfer - inhibit conversion */
#define ZCNL	2	/* Convert NL to local end of line convention */
#define ZCRESUM	3	/* Resume interrupted file transfer */
/* Management include options, one of these ored in ZF1 */
#define ZF1_ZMSKNOLOC   0x80 /* Skip file if not present at rx */
/* Management options, one of these ored in ZF1 */
#define ZF1_ZMMASK	    0x1f /* Mask for the choices below */
#define ZF1_ZMNEWL         1 /* Transfer if source newer or longer */
#define ZF1_ZMCRC          2 /* Transfer if different file CRC or length */
#define ZF1_ZMAPND         3 /* Append contents to existing file (if any) */
#define ZF1_ZMCLOB         4 /* Replace existing file */
#define ZF1_ZMNEW          5 /* Transfer if source newer */
/* Number 5 is alive ... */
#define ZF1_ZMDIFF         6 /* Transfer if dates or lengths different */
#define ZF1_ZMPROT         7 /* Protect destination file */
#define ZF1_ZMCHNG         8 /* Change filename if destination exists */

/* Transport options, one of these in ZF2 */
#define ZTLZW	1	/* Lempel-Ziv compression */
#define ZTCRYPT	2	/* Encryption */
#define ZTRLE	3	/* Run Length encoding */
/* Extended options for ZF3, bit encoded */
#define ZXSPARS	64	/* Encoding for sparse file operations */

/* Parameters for ZCOMMAND frame ZF0 (otherwise 0) */
#define ZCACK1	1	/* Acknowledge, then do command */


/* Result codes */
#define RZSZ_NO_ERROR (0)
#define RZSZ_ERROR (1)

/* Flags */
#define RZSZ_FLAGS_NONE (0x0000)

/*zmodem end*/

#define ZCRC_DIFFERS (ERROR+1)
#define ZCRC_EQUAL (ERROR+2)

/* These are the values for the escape sequence table. */
#define ZM_ESCAPE_NEVER ((char) 0)
#define ZM_ESCAPE_ALWAYS ((char) 1)
#define ZM_ESCAPE_AFTER_AMPERSAND ((char) 2)

extern int bytes_per_error;  /* generate one error around every x bytes */

typedef struct zm_ {
    zreadline_t *zr;	/* Buffered, interruptable input. */
    char Rxhdr[4];		/* Received header */
    char Txhdr[4];		/* Transmitted header */
    int rxtimeout;          /* Constant: tenths of seconds to wait for something */
    int znulls;             /* Constant: Number of nulls to send at beginning of ZDATA hdr */
    int eflag;              /* Constant: local display of non zmodem characters */
    /* 0:  no display */
    /* 1:  display printing characters only */
    /* 2:  display all non ZMODEM characters */
    int zrwindow;		/* RX window size (controls garbage count) */

    int zctlesc;            /* Variable: TRUE means to encode control characters */
    int txfcs32;            /* Variable: TRUE means send binary frames with 32 bit FCS */

    int rxtype;		/* State: type of header received */
    char escape_sequence_table[256]; /* State: conversion chart for zmodem escape sequence encoding */
    char lastsent;		/* State: last byte send */
    int crc32t;             /* State: display flag indicating 32-bit CRC being sent */
    int crc32;              /* State: display flag indicating 32 bit CRC being received */
    int rxframeind;	        /* State: ZBIN, ZBIN32, or ZHEX type of frame received */
    int zmodem_requested;
}zm_t;

zm_t *zm_init(int fd, size_t readnum, size_t bufsize, int no_timeout,
              int rxtimeout, int znulls, int eflag,int zctlesc, int zrwindow);
void zm_deinit(zm_t *zm);
int zm_get_zctlesc(zm_t *zm);
void zm_set_zctlesc(zm_t *zm, int zctlesc);
void zm_escape_sequence_update(zm_t *zm);
void zm_put_escaped_char (zm_t *zm, int c);
void zm_send_binary_header (zm_t *zm, int type);
void zm_send_hex_header (zm_t *zm, int type);
void zm_send_data (zm_t *zm, const char *buf, size_t length, int frameend);
void zm_send_data32 (zm_t *zm, const char *buf, size_t length, int frameend);
void zm_set_header_payload (zm_t *zm, uint32_t val);
void zm_set_header_payload_bytes(zm_t *zm, uint8_t x0, uint8_t x1, uint8_t x2, uint8_t x3);

long zm_reclaim_send_header (zm_t *zm);
long zm_reclaim_receive_header (zm_t *zm);
int zm_receive_data (zm_t *zm, char *buf, int length, size_t *received);
int zm_get_header (zm_t *zm, uint32_t *payload);
void zm_ackbibi (zm_t *zm);
void zm_saybibi(zm_t *zm);
int zm_do_crc_check(zm_t *zm, FILE *f, size_t remote_bytes, size_t check_bytes);

double timing (int reset, time_t *nowp);

typedef void (*complete_call)(const char *filename, int result, size_t size, time_t date);
typedef bool (*tick_call)(long bytes_sent, long bytes_total, long last_bps, int min_left, int sec_left);
typedef bool (*approver_call)(const char *filename, size_t size, time_t date);
#ifdef __cplusplus
}
#endif

#endif
