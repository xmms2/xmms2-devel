#include <glib.h>
#include <glib/gprintf.h>

#include "daap_util.h"

void write_buffer_to_channel(GIOChannel *chan, gchar *buf, gint bufsize)
{
	gint sent_bytes, total_sent_bytes = 0;
	GIOStatus io_stat;
	GError *err = NULL;

	do {
		io_stat = g_io_channel_write_chars(chan,
		                                   buf + total_sent_bytes,
		                                   bufsize - total_sent_bytes,
		                                   &sent_bytes,
		                                   &err);
		if (io_stat == G_IO_STATUS_ERROR) {
			if (NULL != err) {
				g_printf("Error writing to channel: %s\n", err->message);
			}
			break;
		}

		bufsize -= sent_bytes;
		total_sent_bytes += sent_bytes;
	} while (bufsize > 0);

	g_io_channel_flush(chan, &err);
	if (NULL != err) {
		g_printf("warning: error flushing channel: %s\n", err->message);
	}
}

gint read_buffer_from_channel(GIOChannel *chan, gchar *buf, gint bufsize)
{
	GIOStatus io_stat;
	gint read_bytes, n_total_bytes_read = 0;
	GError *err = NULL;

	do {
		io_stat = g_io_channel_read_chars(chan,
		                                  buf + n_total_bytes_read,
		                                  bufsize - n_total_bytes_read,
		                                  &read_bytes,
		                                  &err);
		if (io_stat == G_IO_STATUS_ERROR) {
			g_printf("warning: error reading from channel: %s\n", err->message);
		}
		n_total_bytes_read += read_bytes;

		if (io_stat == G_IO_STATUS_EOF) {
			break;
		}
	} while (bufsize > n_total_bytes_read);

	return n_total_bytes_read;
}

