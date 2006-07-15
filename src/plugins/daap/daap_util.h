#ifndef DAAP_UTIL_H
#define DAAP_UTIL_H

gint read_buffer_from_channel(GIOChannel *chan, gchar *buf, gint bufsize);
void write_buffer_to_channel(GIOChannel *chan, gchar *buf, gint bufsize);

#endif
/* vim:noexpandtab:shiftwidth=4:set tabstop=4: */
