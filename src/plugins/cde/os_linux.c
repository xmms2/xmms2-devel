/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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


#include "xmms/util.h"

#include "cdae.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <linux/cdrom.h>


#define MSF_OFFSET 150

gint
xmms_cdae_read_data (gint fd, gint pos, gchar *buffer, gint len)
{
	struct cdrom_read_audio cdra;
	gint frames_read = len / XMMS_CDAE_FRAMESIZE;

	cdra.addr.lba = pos - MSF_OFFSET;
	cdra.addr_format = CDROM_LBA;
	cdra.nframes = frames_read;
	cdra.buf = buffer;

	if (ioctl (fd, CDROMREADAUDIO, &cdra) < 0)
		return -1;

	return cdra.nframes * XMMS_CDAE_FRAMESIZE;
}

xmms_cdae_toc_t *
xmms_cdae_get_toc (int fd)
{
	struct cdrom_tochdr tochdr;
	struct cdrom_tocentry tocentry;
	xmms_cdae_toc_t *toc;
	int i;

	if (ioctl (fd, CDROMREADTOCHDR, &tochdr)) {
		xmms_log_error ("Couldn't read TOC");
		return NULL;
	}

	toc = g_new0 (xmms_cdae_toc_t, 1);

	for (i = tochdr.cdth_trk0; i <= tochdr.cdth_trk1; i++) {

		tocentry.cdte_format = CDROM_MSF;
		tocentry.cdte_track = i;

		if (ioctl (fd, CDROMREADTOCENTRY, &tocentry)) {
			xmms_log_error ("Couldn't read track %d", i);
			g_free (toc);
			return NULL;
		}

		toc->track[i].minute = tocentry.cdte_addr.msf.minute;
		toc->track[i].second = tocentry.cdte_addr.msf.second;
		toc->track[i].frame = tocentry.cdte_addr.msf.frame;

	}

	XMMS_DBG ("Wow, the CD has %d tracks", i); 

	tocentry.cdte_track = CDROM_LEADOUT;
	tocentry.cdte_format = CDROM_MSF;

	if (ioctl (fd, CDROMREADTOCENTRY, &tocentry)) {
		xmms_log_error ("Couldn't read leadout");
		g_free (toc);
		return NULL;
	}

	toc->leadout.minute = tocentry.cdte_addr.msf.minute;
	toc->leadout.second = tocentry.cdte_addr.msf.second;
	toc->leadout.frame = tocentry.cdte_addr.msf.frame;

	toc->first_track = tochdr.cdth_trk0;
	toc->last_track = tochdr.cdth_trk1;
	
	return toc;

}
