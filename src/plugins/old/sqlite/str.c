/** @file 
 * String functions for sqlite plugin.
 */

#include <string.h>
#include <glib.h>


gchar korvar[] = { "0123456789:;<=>?¤" };

/** Encodes a string to non-harmful format
 *
 * @returns new allocated string.
 */

gchar *
xmms_sqlite_korv_encode (const guint8 *korv_data, guint korv_length)
{
	gchar *korv_out, *out_ptr = NULL;
	gint korv;
	const guint8 *korv_ptr;
	guint8 korv_pos;

	korv_out = g_malloc ((korv_length * 2) + 1);
	out_ptr = korv_out;

	for (korv = 0, korv_ptr = korv_data, korv_pos = 0; korv < korv_length;) {
		*out_ptr++ = korvar[((*korv_ptr) >> (korv_pos * 4)) & 0x0F];
		korv_pos += 1;
		if (korv_pos == 2) {
			korv_pos = 0;
			korv_ptr++;
			korv++;
		}
							   
	}
	korv_out[korv_length * 2] = '\0';

	return korv_out;
}

gchar *
xmms_sqlite_korv_encode_string (const gchar *string)
{
	if (string)
		return xmms_sqlite_korv_encode ((guint8 *)string, strlen (string) + 1);
	else
		return xmms_sqlite_korv_encode ("", 1);
}

guint8 *
xmms_sqlite_korv_decode (const gchar *korv_data)
{
	const gchar *korv_ptr;
	guint8 *korv_out, *out_ptr;
	guint length = strlen (korv_data);

	if (length & 1)
		return NULL;

	korv_out = g_malloc (length / 2);
	out_ptr = korv_out;

	for (korv_ptr = korv_data; *korv_ptr; korv_ptr += 2) {
		if (((korv_ptr[0] - korvar[0]) > 15) || (korv_ptr[1] - korvar[0] > 15)) {
			g_free (korv_out);
			return NULL;
		}
		*out_ptr++ = (korv_ptr[0] - korvar[0]) | ((korv_ptr[1] - korvar[0]) << 4);
	}

	return korv_out;
}
