/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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
#include <string.h>

#include <xmmscpriv/xmmsv.h>
#include <xmmscpriv/xmmsc_util.h>

/**
 * @deprecated
 */
xmmsv_t *
xmmsv_bitbuffer_new_ro (const unsigned char *v, int len)
{
	return xmmsv_new_bitbuffer_ro (v, len);
}

/**
 * Allocates a new bitbuffer #xmmsv_t.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_bitbuffer_ro (const unsigned char *v, int len)
{
	xmmsv_t *val;

	val = _xmmsv_new (XMMSV_TYPE_BITBUFFER);
	val->value.bit.buf = (unsigned char *) v;
	val->value.bit.len = len * 8;
	val->value.bit.ro = true;
	return val;
}

/**
 * @deprecated
 */
xmmsv_t *
xmmsv_bitbuffer_new (void)
{
	return xmmsv_new_bitbuffer ();
}

/**
 * Allocates a new empty bitbuffer #xmmsv_t.
 * @return The new #xmmsv_t. Must be unreferenced with
 * #xmmsv_unref.
 */
xmmsv_t *
xmmsv_new_bitbuffer (void)
{
	xmmsv_t *val;

	val = _xmmsv_new (XMMSV_TYPE_BITBUFFER);
	val->value.bit.buf = NULL;
	val->value.bit.len = 0;
	val->value.bit.ro = false;
	return val;
}

int
xmmsv_bitbuffer_get_bits (xmmsv_t *v, int bits, int *res)
{
	int i, t, r;

	x_api_error_if (bits < 1, "less than one bit requested", 0);

	if (bits == 1) {
		int pos = v->value.bit.pos;

		if (pos >= v->value.bit.len)
			return 0;
		r = (v->value.bit.buf[pos / 8] >> (7-(pos % 8)) & 1);
		v->value.bit.pos += 1;
		*res = r;
		return 1;
	}

	r = 0;
	for (i = 0; i < bits; i++) {
		t = 0;
		if (!xmmsv_bitbuffer_get_bits (v, 1, &t))
			return 0;
		r = (r << 1) | t;
	}
	*res = r;
	return 1;
}

int
xmmsv_bitbuffer_get_data (xmmsv_t *v, unsigned char *b, int len)
{
	while (len) {
		int t;
		if (!xmmsv_bitbuffer_get_bits (v, 8, &t))
			return 0;
		*b = t;
		b++;
		len--;
	}
	return 1;
}

int
xmmsv_bitbuffer_put_bits (xmmsv_t *v, int bits, int d)
{
	unsigned char t;
	int pos;
	int i;

	x_api_error_if (v->value.bit.ro, "write to readonly bitbuffer", 0);
	x_api_error_if (bits < 1, "less than one bit requested", 0);

	if (bits == 1) {
		pos = v->value.bit.pos;

		if (pos >= v->value.bit.alloclen) {
			int ol, nl;
			nl = v->value.bit.alloclen * 2;
			ol = v->value.bit.alloclen;
			nl = nl < 128 ? 128 : nl;
			nl = (nl + 7) & ~7;
			v->value.bit.buf = realloc (v->value.bit.buf, nl / 8);
			memset (v->value.bit.buf + ol / 8, 0, (nl - ol) / 8);
			v->value.bit.alloclen = nl;
		}
		t = v->value.bit.buf[pos / 8];

		t = (t & (~(1<<(7-(pos % 8))))) | (d << (7-(pos % 8)));

		v->value.bit.buf[pos / 8] = t;

		v->value.bit.pos += 1;
		if (v->value.bit.pos > v->value.bit.len)
			v->value.bit.len = v->value.bit.pos;
		return 1;
	}

	for (i = 0; i < bits; i++) {
		if (!xmmsv_bitbuffer_put_bits (v, 1, !!(d & (1 << (bits-i-1)))))
			return 0;
	}

	return 1;
}

int
xmmsv_bitbuffer_put_bits_at (xmmsv_t *v, int bits, int d, int offset)
{
	int prevpos;
	prevpos = xmmsv_bitbuffer_pos (v);
	if (!xmmsv_bitbuffer_goto (v, offset))
		return 0;
	if (!xmmsv_bitbuffer_put_bits (v, bits, d))
		return 0;
	return xmmsv_bitbuffer_goto (v, prevpos);
}

int
xmmsv_bitbuffer_put_data (xmmsv_t *v, const unsigned char *b, int len)
{
	while (len) {
		int t;
		t = *b;
		if (!xmmsv_bitbuffer_put_bits (v, 8, t))
			return 0;
		b++;
		len--;
	}
	return 1;
}

int
xmmsv_bitbuffer_align (xmmsv_t *v)
{
	v->value.bit.pos = (v->value.bit.pos + 7) % 8;
	return 1;
}

int
xmmsv_bitbuffer_goto (xmmsv_t *v, int pos)
{
	x_api_error_if (pos < 0, "negative position", 0);
	x_api_error_if (pos > v->value.bit.len, "position after buffer end", 0);

	v->value.bit.pos = pos;
	return 1;
}

int
xmmsv_bitbuffer_pos (xmmsv_t *v)
{
	return v->value.bit.pos;
}

int
xmmsv_bitbuffer_rewind (xmmsv_t *v)
{
	return xmmsv_bitbuffer_goto (v, 0);
}

int
xmmsv_bitbuffer_end (xmmsv_t *v)
{
	return xmmsv_bitbuffer_goto (v, v->value.bit.len);
}

int
xmmsv_bitbuffer_len (xmmsv_t *v)
{
	return v->value.bit.len;
}

const unsigned char *
xmmsv_bitbuffer_buffer (xmmsv_t *v)
{
	return v->value.bit.buf;
}

/**
 * Retrieves the bit buffer from the value.
 *
 * @param val a #xmmsv_t containing a string.
 * @param r the return data. This data is owned by the value and will
 * be freed when the value is freed.
 * @param rlen the return length of data.
 * @return 1 upon success otherwise 0
 */
int
xmmsv_get_bitbuffer (const xmmsv_t *val, const unsigned char **r, unsigned int *rlen)
{
	if (!val || val->type != XMMSV_TYPE_BITBUFFER) {
		return 0;
	}

	*r = val->value.bin.data;
	*rlen = val->value.bin.len;

	return 1;
}
