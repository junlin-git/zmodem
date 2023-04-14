#ifndef LIBZMODEM_ZMODEM_H
#define LIBZMODEM_ZMODEM_H

#include <stdint.h>
#include <stdbool.h>
#include "zglobal.h"
#ifdef __cplusplus
extern "C" {
#endif

/* zmodem.h - ZMODEM protocol constants

  Copyright (C) until 1998 Chuck Forsberg (OMEN Technology Inc)
  Copyright (C) 1996, 1997 Uwe Ohse

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

 *    05-23-87  Chuck Forsberg Omen Technology Inc
*/
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

/* This runs a zmodem receiver.

   DIRECTORY is the root directory to which files will be downloaded,
   if those files are specified with a relative path.

   DIRECTORY may be NULL.  If DIRECTORY is NULL, the current working
   directory is used.

   APPROVER is a callback function. It is called when a file transfer
   request is received from the sender.  It must return 'true' to
   approve a download of the file. It is up to APPROVER to decide if
   filenames are appropriate, if absolute paths are allowed, if files
   are too large or small, and if existing files may be overwritten.

   APPROVER may be NULL.  If APPROVER is NULL, all files with relative
   pathnames will be approved -- even if they overwrite existing files
   -- and all files with absolute pathnames will be rejected.

   TICK is a callback function.  It is called after each packet is
   received from the sender.  If it returns 'true', the zmodem fetch
   will continue.  If it returns 'false', the zmodem fetch will
   terminate.  It should return quickly.

   If TICK is NULL, the transfer will continue until completion, unless
   the MIN_BPS check fails.

   COMPLETE is a callback function.  It is called when a file download
   has completed, either successfully or unsuccessfully. It should
   return quickly.  The RESULT parameter will return a result code
   indicating if the transfer was successful.

   COMPLETE may be NULL, meaning that completion data is ignored.

   MIN_BPS is the minimum data transfer rate that will be tolerated,
   in bits per second.  If the sender transfers data slower than
   MIN_BPS, the fetch will terminate.

   If MIN_BPS is zero, this check will be disabled.

   FLAGS determine how this zmodem receiver operates.

   The return value is the sum of the sizes of the files successfully
   transfered. */
size_t zmodem_receive(const char *directory,
                      bool (*approver)(const char *filename, size_t size, time_t date),
                      bool tick_cb(const char *fname, long bytes_sent, long bytes_total, long last_bps, int min_left, int sec_left),
                      void (*complete)(const char *filename, int result, size_t size, time_t date),
                      uint64_t min_bps,
                      uint32_t flags);

/* This runs a zmodem receiver.

   DIRECTORY is the root directory from which files will be downloaded,
   if those files are specified with a relative path.

   If DIRECTORY is null, the current working directory is used.

   FILE_COUNT is the number of files to be transferred, and FILE_LIST
   is an array of strings that contains their (relative or absolute)
   pathnames.

   FILE_COUNT must be 1 or greater, and FILE_LIST must be valid,
   otherwise the result is unspecified.

   TICK is a callback function.  It is called after each packet is
   sent.  If it returns 'true', the zmodem send will continue.  If it
   returns 'false', the zmodem send will terminate prematurely.  It
   should return quickly.

   TICK may be NULL.  If TICK is NULL, the send will continue until
   completion, unless the MIN_BPS check fails.

   COMPLETE is a callback function.  It is called when a file download
   has completed, either successfully or unsuccessfully. It should
   return quickly.  The RESULT parameter will indicate if the send was
   successful.

   COMPLETE may be NULL.

   MIN_BPS is the minimum data transfer rate that will be tolerated.
   If the sender transfers data slower than MIN_BPS, the fetch
   will terminate.

   If MIN_BPS is zero, this test will be disabled.

   FLAGS determine how this zmodem sender operates.

   The return value is the sum of the sizes of the files successfully
   transfered. */
size_t zmodem_send(int file_count,
                   char **file_list,
                   bool (*tick)(long bytes_sent, long bytes_total, long last_bps, int min_left, int sec_left),
                   void (*complete)(const char *filename, int result, size_t size, time_t date),
                   uint64_t min_bps,
                   uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif
