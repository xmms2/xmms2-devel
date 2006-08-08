/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "xmmsclient/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient_ipc.h"
#include "xmmsc/xmmsc_idnumbers.h"
#include "xmmsc/xmmsc_stringport.h"

/** 
 * Add binary data to the servers bindata directory.
 * Data has to be base64 encoded by the caller.
 */
xmmsc_result_t *
xmmsc_bindata_add (xmmsc_connection_t *c,
                   const unsigned char *data,
				   unsigned int len)
{
	xmms_ipc_msg_t *msg;

	x_check_conn (c, NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_BINDATA,
	                        XMMS_IPC_CMD_ADD_DATA);

	xmms_ipc_msg_put_bin (msg, data, len);

	return xmmsc_send_msg (c, msg);
}

/**
 * Retreive a file from the servers bindata directory,
 * based on the hash. Data will be returned in base64
 * encoded data.
 */
xmmsc_result_t *
xmmsc_bindata_retreive (xmmsc_connection_t *c, const char *hash)
{
	xmms_ipc_msg_t *msg;

	x_check_conn (c, NULL);

	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_BINDATA,
	                        XMMS_IPC_CMD_GET_DATA);
	xmms_ipc_msg_put_string (msg, hash);

	return xmmsc_send_msg (c, msg);
}

/**
 * Remove a file with associated with the hash from the server
 */
xmmsc_result_t *
xmmsc_bindata_remove (xmmsc_connection_t *c, const char *hash)
{
	xmms_ipc_msg_t *msg;

	x_check_conn (c, NULL);
	msg = xmms_ipc_msg_new (XMMS_IPC_OBJECT_BINDATA,
	                        XMMS_IPC_CMD_REMOVE_DATA);
	xmms_ipc_msg_put_string (msg, hash);

	return xmmsc_send_msg (c, msg);
}

/* gbase64.c - Base64 encoding/decoding
 *
 *  Copyright (C) 2006 Alexander Larsson <alexl@redhat.com>
 *  Copyright (C) 2000-2003 Ximian Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * This is based on code in camel, written by:
 *    Michael Zucchi <notzed@ximian.com>
 *    Jeffrey Stedfast <fejj@ximian.com>
 */

#include <string.h>

static const char base64_alphabet[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * base64_encode_step:
 * @in: the binary data to encode.
 * @len: the length of @in.
 * @break_lines: whether to break long lines
 * @out: pointer to destination buffer
 * @state: Saved state between steps, initialize to 0
 * @save: Saved state between steps, initialize to 0
 *
 * Incrementally encode a sequence of binary data into it's Base-64 stringified
 * representation. By calling this functions multiple times you can convert data
 * in chunks to avoid having to have the full encoded data in memory.
 *
 * When all the data has been converted you must call base64_encode_close()
 * to flush the saved state.
 *
 * The output buffer must be large enough to fit all the data that will
 * be written to it. Due to the way base64 encodes you will need
 * at least: @len * 4 / 3 + 6 bytes. If you enable line-breaking you will
 * need at least: @len * 4 / 3 + @len * 4 / (3 * 72) + 7 bytes.
 *
 * @break_lines is typically used when putting base64-encoded data in emails.
 * It breaks the lines at 72 columns instead of putting all text on the same
 * line. This avoids problems with long lines in the email system.
 *
 * Return value: The number of bytes of output that was written
 *
 */
static unsigned int
base64_encode_step (const unsigned char *in, 
					unsigned int         len, 
					int      break_lines, 
					char        *out, 
					int         *state, 
					int         *save)
{
	char *outptr;
	const unsigned char *inptr;

	if (len <= 0)
		return 0;

	inptr = in;
	outptr = out;

	if (len + ((char *) save) [0] > 2)
	{
		const unsigned char *inend = in+len-2;
		int c1, c2, c3;
		int already;

		already = *state;

		switch (((char *) save) [0])
		{	
			case 1:	
				c1 = ((unsigned char *) save) [1]; 
				goto skip1;
			case 2:	
				c1 = ((unsigned char *) save) [1];
				c2 = ((unsigned char *) save) [2]; 
				goto skip2;
		}

		/* 
		 * yes, we jump into the loop, no i'm not going to change it, 
		 * it's beautiful! 
		 */
		while (inptr < inend)
		{
			c1 = *inptr++;
skip1:
			c2 = *inptr++;
skip2:
			c3 = *inptr++;
			*outptr++ = base64_alphabet [ c1 >> 2 ];
			*outptr++ = base64_alphabet [ c2 >> 4 | 
				((c1&0x3) << 4) ];
			*outptr++ = base64_alphabet [ ((c2 &0x0f) << 2) | 
				(c3 >> 6) ];
			*outptr++ = base64_alphabet [ c3 & 0x3f ];
			/* this is a bit ugly ... */
			if (break_lines && (++already) >= 19)
			{
				*outptr++ = '\n';
				already = 0;
			}
		}

		((char *)save)[0] = 0;
		len = 2 - (inptr - inend);
		*state = already;
	}

	if (len>0)
	{
		char *saveout;

		/* points to the slot for the next char to save */
		saveout = & (((char *)save)[1]) + ((char *)save)[0];

		/* len can only be 0 1 or 2 */
		switch(len)
		{
			case 2:	*saveout++ = *inptr++;
			case 1:	*saveout++ = *inptr++;
		}
		((char *)save)[0] += len;
	}

	return outptr - out;
}

/**
 * base64_encode_close:
 * @break_lines: whether to break long lines
 * @out: pointer to destination buffer
 * @state: Saved state from base64_encode_step()
 * @save: Saved state from base64_encode_step()
 *
 * Flush the status from a sequence of calls to base64_encode_step().
 *
 * Return value: The number of bytes of output that was written
 *
 */
static unsigned int
base64_encode_close (int  break_lines,
					 char    *out, 
					 int     *state, 
					 int     *save)
{
	int c1, c2;
	char *outptr = out;

	c1 = ((unsigned char *) save) [1];
	c2 = ((unsigned char *) save) [2];

	switch (((char *) save) [0])
	{
		case 2:
			outptr [2] = base64_alphabet[ ( (c2 &0x0f) << 2 ) ];
			goto skip;
		case 1:
			outptr[2] = '=';
skip:
			outptr [0] = base64_alphabet [ c1 >> 2 ];
			outptr [1] = base64_alphabet [ c2 >> 4 | ( (c1&0x3) << 4 )];
			outptr [3] = '=';
			outptr += 4;
			break;
	}
	if (break_lines)
		*outptr++ = '\n';

	*save = 0;
	*state = 0;

	return outptr - out;
}

/**
 * base64_encode:
 * @data: the binary data to encode.
 * @len: the length of @data.
 *
 * Encode a sequence of binary data into it's Base-64 stringified
 * representation.
 *
 * Return value: a newly allocated, zero-terminated Base-64 encoded
 *               string representing @data.
 *
 */
char *
xmms_bindata_base64_encode (const unsigned char *data, unsigned int len)
{
	char *out;
	int state = 0, outlen;
	int save = 0;

	/* We can use a smaller limit here, since we know the saved state is 0 */
	out = malloc (len * 4 / 3 + 4);
	outlen = base64_encode_step (data, len, false, out, &state, &save);
	outlen += base64_encode_close (false,
								   out + outlen, 
								   &state, 
								   &save);
	out[outlen] = '\0';
	return (char *) out;
}

static const unsigned char mime_base64_rank[256] = {
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
	255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
	255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
	255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
};

/**
 * base64_decode_step: 
 * @in: binary input data
 * @len: max length of @in data to decode
 * @out: output buffer
 * @state: Saved state between steps, initialize to 0
 * @save: Saved state between steps, initialize to 0
 *
 * Incrementally decode a sequence of binary data from it's Base-64 stringified
 * representation. By calling this functions multiple times you can convert data
 * in chunks to avoid having to have the full encoded data in memory.
 *
 * The output buffer must be large enough to fit all the data that will
 * be written to it. Since base64 encodes 3 bytes in 4 chars you need
 * at least: @len * 3 / 4 bytes.
 * 
 * Return value: The number of bytes of output that was written
 *
 **/
static unsigned int
base64_decode_step (const char  *in, 
					unsigned int len, 
					unsigned char *out, 
					int *state, 
					unsigned int *save)
{
	const unsigned char *inptr;
	unsigned char *outptr;
	const unsigned char *inend;
	unsigned char c, rank;
	unsigned char last[2];
	unsigned int v;
	int i;

	inend = (const unsigned char *)in+len;
	outptr = out;

	/* convert 4 base64 bytes to 3 normal bytes */
	v=*save;
	i=*state;
	inptr = (const unsigned char *)in;
	last[0] = last[1] = 0;
	while (inptr < inend)
	{
		c = *inptr++;
		rank = mime_base64_rank [c];
		if (rank != 0xff)
		{
			last[1] = last[0];
			last[0] = c;
			v = (v<<6) | rank;
			i++;
			if (i==4)
			{
				*outptr++ = v>>16;
				if (last[1] != '=')
					*outptr++ = v>>8;
				if (last[0] != '=')
					*outptr++ = v;
				i=0;
			}
		}
	}

	*save = v;
	*state = i;

	return outptr - out;
}

/**
 * base64_decode:
 * @text: zero-terminated string with base64 text to decode.
 * @out_len: The lenght of the decoded data is written here.
 *
 * Decode a sequence of Base-64 encoded text into binary data
 *
 * Return value: a newly allocated, buffer containing the binary data
 *               that @text represents
 *
 * Since: 2.12
 */
unsigned char *
xmms_bindata_base64_decode (const char *text, unsigned int *out_len)
{
	unsigned char *ret;
	int inlen, state = 0;
	unsigned int save = 0;

	inlen = strlen (text);
	ret = calloc (inlen * 3 / 4, 1);

	*out_len = base64_decode_step (text, inlen, ret, &state, &save);

	return ret; 
}

