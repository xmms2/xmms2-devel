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

#include <sys/cdio.h>
#include <sys/cdrio.h>


#define MSF_OFFSET 150
#define CD_LEADOUT_TRACK 0xAA

gint
xmms_cdae_read_data (gint fd, gint pos, gchar *buffer, gint len)
{
	gint bsize = 2352;
	gint frames_read = len / XMMS_CDAE_FRAMESIZE;

	if (ioctl (fd, CDRIOCSETBLOCKSIZE, &bsize) < 0)
		return -1;

	if (pread (fd, buffer, frames_read * bsize, (pos - MSF_OFFSET) * bsize) != frames_read * bsize)
		return -1;

	return frames_read * XMMS_CDAE_FRAMESIZE;
}

xmms_cdae_toc_t *
xmms_cdae_get_toc (int fd)
{
	struct ioc_toc_header tochdr;
	struct ioc_read_toc_single_entry tocentry;
	xmms_cdae_toc_t *toc;
	int i;

	if (ioctl (fd, CDIOREADTOCHEADER, &tochdr)) {
		xmms_log_error ("Couldn't read TOC");
		return NULL;
	}

	toc = g_new0 (xmms_cdae_toc_t, 1);

	for (i = tochdr.starting_track; i <= tochdr.ending_track; i++) {

		tocentry.address_format = CD_MSF_FORMAT;
		tocentry.track = i;

		if (ioctl (fd, CDIOREADTOCENTRY, &tocentry)) {
			xmms_log_error ("Couldn't read track %d", i);
			g_free (toc);
			return NULL;
		}

		toc->track[i].minute = tocentry.entry.addr.msf.minute;
		toc->track[i].second = tocentry.entry.addr.msf.second;
		toc->track[i].frame = tocentry.entry.addr.msf.frame;

	}

	XMMS_DBG ("Wow, the CD has %d tracks", i - 1); 

	tocentry.track = CD_LEADOUT_TRACK;
	tocentry.address_format = CD_MSF_FORMAT;

	if (ioctl (fd, CDIOREADTOCENTRY, &tocentry)) {
		xmms_log_error ("Couldn't read leadout");
		g_free (toc);
		return NULL;
	}

	toc->leadout.minute = tocentry.entry.addr.msf.minute;
	toc->leadout.second = tocentry.entry.addr.msf.second;
	toc->leadout.frame = tocentry.entry.addr.msf.frame;

	toc->first_track = tochdr.starting_track;
	toc->last_track = tochdr.ending_track;
	
	return toc;

}
